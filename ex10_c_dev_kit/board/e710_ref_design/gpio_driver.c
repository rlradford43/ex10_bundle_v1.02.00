/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2021 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

#define _GNU_SOURCE

#include "board/gpio_driver.h"
#include "board/board_spec.h"
#include "board/time_helpers.h"
#include "ex10_api/trace.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <gpiod.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0u]))

enum R807_PIN_NUMBERS
{
    BOARD_POWER_PIN = 5,
    READY_N_PIN     = 7,
    EX10_ENABLE_PIN = 12,
    RESET_N_PIN     = 13,
    TEST            = 24,
    IRQ_N_PIN       = 25,
};

/**
 * The following RPI GPIO pins are not connected to any R807 functions
 * and can be used for debug purposes.
 */
static uint8_t const r807_debug_pins[] = {2, 3, 4};

/**
 * The R807 board LEDs are connected to the RPi GPIO pins:
 * - LED_0: RPi GPIO 16
 * - LED_1: RPi GPIO 26
 * - LED_2: RPi GPIO 20
 * - LED_3: RPi GPIO  6
 * The ordering of r807_led_pins[] should match the hardware.
 */
static uint8_t const r807_led_pins[] = {16, 26, 20, 6};

/* libgpiod handles */
static const char*        consumer         = "Ex10 SDK";
static const char*        gpiochip_label   = "pinctrl-bcm2835";
static const char*        gpiochip_label2  = "pinctrl-bcm2711";
static struct gpiod_chip* chip             = NULL;
static struct gpiod_line* power_line       = NULL;
static struct gpiod_line* reset_line       = NULL;
static struct gpiod_line* ex10_enable_line = NULL;
static struct gpiod_line* ready_n_line     = NULL;
static struct gpiod_line* ex10_test_line   = NULL;
static struct gpiod_line* irq_n_line       = NULL;

static struct gpiod_line* debug_lines[ARRAY_SIZE(r807_debug_pins)] = {NULL};
static struct gpiod_line* led_lines[ARRAY_SIZE(r807_led_pins)]     = {NULL};

static pthread_t irq_n_monitor_pthread;

static void (*irq_n_cb)(void) = NULL;

/*
 * A lock used to ensure that command transactions are not interrupted by the
 * IRQ_N line interrupt handler. The interrupt handler will need to send
 * commands to an Ex10 chip which could interfere with an ongoing transaction
 * from the main process context.
 *
 * A lock is used in the Impinj Reference Design because on the RPi the
 * "interrupt handler" for IRQ_N is a normal POSIX thread. A lock can be
 * acquired because it is done in a thread and NOT in an interrupt context.
 *
 * On a port to a microcontroller, a lock should NOT be used because acquiring a
 * lock in an interrupt handler can cause a deadlock. In this case, the IRQ_N
 * interrupt should be temporarily disabled.
 */
static pthread_mutex_t irq_lock = PTHREAD_MUTEX_INITIALIZER;

static void irq_n_pthread_cleanup(void* arg)
{
    struct gpiod_line* irq_n_line_arg = arg;
    if (irq_n_line_arg != irq_n_line)
    {
        fprintf(stderr,
                "error: %s: irq_n_line_arg != irq_n_line: %p, %p\n",
                __func__,
                (void const*)irq_n_line_arg,
                (void const*)irq_n_line);
    }

    if (irq_n_line)  // Valid line detected
    {
        gpiod_line_release(irq_n_line);
        irq_n_line = NULL;
    }
    else
    {
        fprintf(stderr, "error: %s: irq_line: NULL\n", __func__);
    }

    tracepoint(pi_ex10sdk, GPIO_mutex_unlock, ex10_get_thread_id());

    // Try to take the irq_lock. If the try fails then the lock is already
    // acquired. Regardless of try's success or failure, release the lock.
    // Note: pthread cancellation may leave the mutex irq_lock in a locked
    // state and it is not an error. This happens when the call sequence:
    //   Ex10GpioDriver.irq_enable(false);
    //   Ex10SpiDriver.read() or write();
    //   Ex10GpioDriver.irq_enable(true);
    // is cancelled during the read() or write() operation; the call to
    // irq_enable(true) will not be made, and this cancellation cleanup handler
    // will be invoked.
    int const try_result = pthread_mutex_trylock(&irq_lock);
    (void)try_result;

    int const unlock_result = pthread_mutex_unlock(&irq_lock);
    if (unlock_result != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_mutex_unlock() failed: %d %s\n",
                __func__,
                unlock_result,
                strerror(unlock_result));
    }
}

/* pthread for monitoring IRQ_N for interrupts. */
static void* irq_n_monitor(void* arg)
{
    (void)(arg);

    // Cancellation is deferred until the thread next calls a function
    // that is a cancellation point
    int old_type = -1;
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_type);

    if (irq_n_line != NULL)
    {
        fprintf(stderr,
                "error: %s: tid: %u, irq_n_line already allocated\n",
                __func__,
                ex10_get_thread_id());
    }

    // Reserve IRQ_N pin
    assert(chip);
    irq_n_line = gpiod_chip_get_line(chip, IRQ_N_PIN);
    if (irq_n_line == NULL)
    {
        fprintf(stderr,
                "error: %s: tid: %u, "
                "gpiod_chip_get_line(IRQ_N_PIN) failed: %d %s\n",
                __func__,
                ex10_get_thread_id(),
                errno,
                strerror(errno));
        assert(irq_n_line);
    }
    pthread_cleanup_push(irq_n_pthread_cleanup, (void*)irq_n_line);

    // Add edge monitoring
    int const ret =
        gpiod_line_request_falling_edge_events(irq_n_line, consumer);
    if (ret != 0)
    {
        fprintf(stderr,
                "error: %s: tid: %u, "
                "gpiod_line_request_falling_edge_events() failed: %d %s\n",
                __func__,
                ex10_get_thread_id(),
                errno,
                strerror(errno));
        assert(ret == 0);
    }

    while (true)
    {
        // Block waiting, with no timeout, for a falling edge.
        // This function calls ppoll(), which is a pthread cancellation point,
        // on an array (gpiod calls it a 'bulk') of file descriptors.
        int const event_status = gpiod_line_event_wait(irq_n_line, NULL);
        tracepoint(pi_ex10sdk, GPIO_irq_n_low);

        // Clear the falling edge event.
        // The underlying system call is read(), which is a cancellation point.
        struct gpiod_line_event event;
        gpiod_line_event_read(irq_n_line, &event);
        if (event.event_type != GPIOD_LINE_EVENT_FALLING_EDGE)
        {
            fprintf(
                stderr,
                "error: %s: unexpected: event_type: %d, irq_n_monitor() exit\n",
                __func__,
                event.event_type);

            assert(event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE);
            break;
        }

        if (event_status == 1)
        {
            if (irq_n_cb)
            {
                (*irq_n_cb)();
            }
        }
        else
        {
            fprintf(stderr, "IRQ_N monitoring failed with %d\n", event_status);
            break;
        }
    }
    pthread_cleanup_pop(irq_n_line);

    // Satisfy pthread's function prototype.
    return NULL;
}

static void irq_enable(bool enable)
{
    if (enable)
    {
        // Unlock to allow IRQ_N handler to run
        tracepoint(pi_ex10sdk, GPIO_mutex_unlock, ex10_get_thread_id());
        pthread_mutex_unlock(&irq_lock);
    }
    else
    {
        // Lock to prevent IRQ_N handler from running
        tracepoint(pi_ex10sdk, GPIO_mutex_lock_request, ex10_get_thread_id());
        pthread_mutex_lock(&irq_lock);
        tracepoint(pi_ex10sdk, GPIO_mutex_lock_acquired, ex10_get_thread_id());
    }
}

enum PudnConfig
{
    PudnNone = 0,
    PudnUp   = 1,
    PudnDown = 2,
};

static void bcm2835_configure_pudn(uint32_t volatile* gpio_base,
                                   uint8_t            pin,
                                   enum PudnConfig    config)
{
    // See the BCM2835 Arm Peripherials Manual
    // Write the desired config
    const uint32_t gppud_offset = 0x25;
    *(gpio_base + gppud_offset) = config;

    // Wait 150 cycles
    get_ex10_time_helpers()->busy_wait_ms(10);

    // Write GPPUDCLK0 to indicate which GPIOs to apply the configuration to.
    // This is a typical bitmask where bit n is set to apply the configuration
    // for GPIOn.
    uint32_t       value            = (1 << pin);
    const uint32_t gppudclk0_offset = 0x26;
    *(gpio_base + gppudclk0_offset) = value;

    // Wait 150 cycles
    get_ex10_time_helpers()->busy_wait_ms(10);

    // Remove GPIO selection
    *(gpio_base + gppudclk0_offset) = 0;
}

static void bcm2711_configure_pudn(uint32_t volatile* gpio_base,
                                   uint8_t            pin,
                                   enum PudnConfig    config)
{
    // See the BCM2711 Arm Peripherials Manual
    const uint32_t gpio_pup_pdn_cntrl_reg0_offset = 0x39;

    // Do a read-modify-write to the config register. Each GPIO has 2 bits of
    // configuration in this register, so there's a total of 16 GPIO configs
    // packed in here. Clear and then set the bits for the GPIO we care about.
    uint32_t gpio_pull_reg = *(gpio_base + gpio_pup_pdn_cntrl_reg0_offset);
    gpio_pull_reg &= (3 << pin);
    gpio_pull_reg |= (config << pin);
    *(gpio_base + gpio_pup_pdn_cntrl_reg0_offset) = gpio_pull_reg;
}

static void configure_gpio_pudn(uint8_t pin, enum PudnConfig config)
{
    int model_fd = open("/proc/device-tree/model", O_RDONLY);
    assert(model_fd != -1);

    char model_str[64];
    memset(&model_str, 0, sizeof(model_str));
    ssize_t num_bytes = read(model_fd, model_str, 64);
    assert(num_bytes > 0);
    assert(model_str[0] != 0);
    close(model_fd);

    int gpio_base_fd = open("/dev/gpiomem", O_RDWR | O_CLOEXEC);
    if (gpio_base_fd == -1)
    {
        fprintf(stderr, "Unable to open /dev/gpiomem (%s)\n", strerror(errno));
        assert(0);
    }

    uint32_t* gpio_base =
        mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, gpio_base_fd, 0);
    if (gpio_base == MAP_FAILED)
    {
        fprintf(stderr, "Unable to mmap /dev/gpiomem (%s)\n", strerror(errno));
        assert(0);
    }

    if (strstr(model_str, "Pi 3") != NULL)
    {
        bcm2835_configure_pudn(gpio_base, pin, config);
    }
    else if (strstr(model_str, "Pi 4") != NULL)
    {
        bcm2711_configure_pudn(gpio_base, pin, config);
    }
    else
    {
        fprintf(stderr, "Unknown device model %s\n", model_str);
        assert(0);
    }

    munmap(gpio_base, 1024);
    close(gpio_base_fd);
}

static void set_board_power(bool power_on)
{
    if (power_line != NULL)
    {
        gpiod_line_set_value(power_line, power_on);
    }
}

static bool get_board_power(void)
{
    if (power_line != NULL)
    {
        return gpiod_line_get_value(power_line);
    }

    // Note: The assumption here is that if the pin was not allocated then
    // the pin level is false.
    // Board specific implementations will need to set this value appropriately.
    return false;
}

static void set_ex10_enable(bool enable)
{
    if (ex10_enable_line != NULL)
    {
        gpiod_line_set_value(ex10_enable_line, enable);
    }
}

static bool get_ex10_enable(void)
{
    if (ex10_enable_line != NULL)
    {
        return gpiod_line_get_value(ex10_enable_line);
    }
    return false;
}

static int register_irq_callback(void (*cb_func)(void))
{
    if (irq_n_cb != NULL)
    {
        fprintf(stderr, "error: %s: already registered\n", __func__);
        assert(0);
        return -1;
    }

    irq_n_cb = cb_func;

    int const error_mutex = pthread_mutex_init(&irq_lock, NULL);
    if (error_mutex != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_mutex_init() failed: %d, %s\n",
                __func__,
                error_mutex,
                strerror(error_mutex));
        return error_mutex;
    }

    int const error_thread =
        pthread_create(&irq_n_monitor_pthread, NULL, irq_n_monitor, NULL);
    if (error_thread != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_create() failed: %d, %s\n",
                __func__,
                error_thread,
                strerror(error_thread));
        return error_thread;
    }

    return 0;
}

static int deregister_irq_callback(void)
{
    if (irq_n_cb == NULL)
    {
        // Note: this early return is expected when called from
        // Ex10GpioDriver.cleanup().
        // The callback has already been removed by Ex10Protocol.deinit().
        return 0;
    }

    // Reason(s) for pthread_join() or pthread_cancel() to fail in the
    // gpio_driver.c context:
    // ESRCH No thread with the ID thread could be found.
    //       i.e. the thread was not successfully created when
    //       register_irq_callback() was caled.
    int error_thread = pthread_cancel(irq_n_monitor_pthread);
    if (error_thread != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_cancel(), failed: %d %s\n",
                __func__,
                error_thread,
                strerror(error_thread));
    }

    error_thread = pthread_join(irq_n_monitor_pthread, NULL);
    if (error_thread != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_join(), failed: %d %s\n",
                __func__,
                error_thread,
                strerror(error_thread));
    }

    int const error_mutex = pthread_mutex_destroy(&irq_lock);
    if (error_mutex != 0)
    {
        fprintf(stderr,
                "error: %s: pthread_mutex_destroy(), failed: %d %s\n",
                __func__,
                error_mutex,
                strerror(error_mutex));
    }

    // Set irq_n_cb to NULL after the IRQ_N monitor thread is cancelled.
    // This avoids the race of checking the callback and using the callback.
    // See irq_n_monitor(), while() loop: if (irq_n_cb) {...} conditional.
    irq_n_cb = NULL;

    return (error_thread != 0) ? error_thread : error_mutex;
}

static bool thread_is_irq_monitor(void)
{
    pthread_t const tid_self = pthread_self();
    return pthread_equal(tid_self, irq_n_monitor_pthread) ? true : false;
}

static void gpio_release_all_lines(void)
{
    if (power_line)
    {
        gpiod_line_release(power_line);
        power_line = NULL;
    }
    if (reset_line)
    {
        gpiod_line_release(reset_line);
        reset_line = NULL;
    }
    if (ex10_enable_line)
    {
        gpiod_line_release(ex10_enable_line);
        ex10_enable_line = NULL;
    }
    if (ready_n_line)
    {
        gpiod_line_release(ready_n_line);
        ready_n_line = NULL;
    }
    if (ex10_test_line)
    {
        gpiod_line_release(ex10_test_line);
        ex10_test_line = NULL;
    }
}

static void gpio_initialize(bool board_power_on, bool ex10_enable, bool reset)
{
    // NOTE: Inputs default to Pull-Up enable. Pull-up/pull-down values are not
    //       modifiable from userspace until Linux v5.5.
    chip = gpiod_chip_open_by_label(gpiochip_label);
    /* Kernel 5.x-based RPi OS uses a different chip label */
    if (!chip)
    {
        chip = gpiod_chip_open_by_label(gpiochip_label2);
    }
    assert(chip);

    gpio_release_all_lines();

    // The EX10 TEST line should always be driven low.
    ex10_test_line = gpiod_chip_get_line(chip, TEST);
    assert(ex10_test_line);
    int ret = gpiod_line_request_output(ex10_test_line, consumer, 0u);
    assert(ret == 0);

    configure_gpio_pudn(BOARD_POWER_PIN, PudnNone);
    power_line = gpiod_chip_get_line(chip, BOARD_POWER_PIN);
    assert(power_line);
    ret = gpiod_line_request_output(power_line, consumer, board_power_on);
    assert(ret == 0);

    configure_gpio_pudn(EX10_ENABLE_PIN, PudnNone);
    ex10_enable_line = gpiod_chip_get_line(chip, EX10_ENABLE_PIN);
    assert(ex10_enable_line);
    ret = gpiod_line_request_output(ex10_enable_line, consumer, ex10_enable);
    assert(ret == 0);

    configure_gpio_pudn(RESET_N_PIN, PudnNone);
    reset_line = gpiod_chip_get_line(chip, RESET_N_PIN);
    assert(reset_line);
    ret = gpiod_line_request_output(reset_line, consumer, !reset);
    assert(ret == 0);

    configure_gpio_pudn(READY_N_PIN, PudnNone);
    ready_n_line = gpiod_chip_get_line(chip, READY_N_PIN);
    assert(ready_n_line);
    ret = gpiod_line_request_input(ready_n_line, consumer);
    assert(ret == 0);

    // Enable debug pins as outputs with their initial level at '1'.
    for (size_t idx = 0u; idx < ARRAY_SIZE(r807_debug_pins); ++idx)
    {
        configure_gpio_pudn(r807_debug_pins[idx], PudnNone);
        debug_lines[idx] = gpiod_chip_get_line(chip, r807_debug_pins[idx]);
        assert(debug_lines[idx]);
        ret = gpiod_line_request_output(debug_lines[idx], consumer, 1u);
        assert(ret == 0);
    }

    // Enable LED pins as outputs with their initial level at '0' (LEDs off)
    for (size_t idx = 0u; idx < ARRAY_SIZE(r807_led_pins); ++idx)
    {
        configure_gpio_pudn(r807_led_pins[idx], PudnNone);
        led_lines[idx] = gpiod_chip_get_line(chip, r807_led_pins[idx]);
        assert(led_lines[idx]);
        ret = gpiod_line_request_output(led_lines[idx], consumer, 0u);
        assert(ret == 0);
    }

    if (ex10_enable && !board_power_on)
    {
        fprintf(stderr, "Ex10 Line Conflict: enable on without board power");
        assert(0);
    }

    if (board_power_on && ex10_enable)
    {
        // Wait for the TCXO to settle.
        get_ex10_time_helpers()->busy_wait_ms(10);
        set_ex10_enable(true);
    }
}

static void gpio_cleanup(void)
{
    deregister_irq_callback();

    gpio_release_all_lines();

    for (size_t idx = 0u; idx < ARRAY_SIZE(debug_lines); ++idx)
    {
        if (debug_lines[idx] != NULL)
        {
            gpiod_line_release(debug_lines[idx]);
            debug_lines[idx] = NULL;
        }
    }

    for (size_t idx = 0u; idx < ARRAY_SIZE(led_lines); ++idx)
    {
        if (led_lines[idx] != NULL)
        {
            gpiod_line_release(led_lines[idx]);
            led_lines[idx] = NULL;
        }
    }

    if (chip)
    {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

static void assert_ready_n(void)
{
    if (ready_n_line != NULL)
    {
        gpiod_line_release(ready_n_line);
        gpiod_line_request_output(ready_n_line, consumer, 0);
    }
}

static void release_ready_n(void)
{
    if (ready_n_line != NULL)
    {
        gpiod_line_release(ready_n_line);
        gpiod_line_request_input(ready_n_line, consumer);
    }
}

static void assert_reset_n(void)
{
    if (reset_line != NULL)
    {
        gpiod_line_set_value(reset_line, 0);
    }
}

static void deassert_reset_n(void)
{
    if (reset_line != NULL)
    {
        gpiod_line_set_value(reset_line, 1);
    }
}

static void reset_device(void)
{
    assert_reset_n();
    get_ex10_time_helpers()->busy_wait_ms(10);
    deassert_reset_n();
}

static int ready_n_pin_get(void)
{
    int gpio_level = gpiod_line_get_value(ready_n_line);
    return gpio_level;
}

static int busy_wait_ready_n(uint32_t timeout_ms)
{
    struct Ex10TimeHelpers const* time_helpers = get_ex10_time_helpers();

    uint32_t const start_time = time_helpers->time_now();

    // Check for ready n low or get a timeout
    int ret_val    = 0;
    int gpio_level = 1;
    do
    {
        gpio_level = gpiod_line_get_value(ready_n_line);
        if (gpio_level == -1)
        {
            fprintf(stderr,
                    "error: %s: gpiod_line_get_value() failed: %d, %s\n",
                    __func__,
                    errno,
                    strerror(errno));
            ret_val = -1;
        }

        if (time_helpers->time_elapsed(start_time) > timeout_ms)
        {
            fprintf(stderr,
                    "error: %s: timeout: %u ms expired\n",
                    __func__,
                    timeout_ms);
            errno   = ETIMEDOUT;
            ret_val = -1;
        }

    } while ((gpio_level != 0) && (ret_val == 0));

    tracepoint(pi_ex10sdk, GPIO_ready_n_low);
    return ret_val;
}

static size_t debug_pin_get_count(void)
{
    return ARRAY_SIZE(r807_debug_pins);
}

static bool debug_pin_get(uint8_t pin_idx)
{
    if (pin_idx < ARRAY_SIZE(r807_debug_pins))
    {
        return gpiod_line_get_value(debug_lines[pin_idx]);
    }
    return false;
}

static void debug_pin_set(uint8_t pin_idx, bool value)
{
    if (pin_idx < ARRAY_SIZE(r807_debug_pins))
    {
        gpiod_line_set_value(debug_lines[pin_idx], value);
    }
}

static void debug_pin_toggle(uint8_t pin_idx)
{
    if (pin_idx < ARRAY_SIZE(r807_debug_pins))
    {
        int const value = gpiod_line_get_value(debug_lines[pin_idx]);
        gpiod_line_set_value(debug_lines[pin_idx], value ^ 1u);
    }
}

static size_t led_pin_get_count(void)
{
    return ARRAY_SIZE(r807_led_pins);
}

static bool led_pin_get(uint8_t pin_idx)
{
    if (pin_idx < ARRAY_SIZE(r807_led_pins))
    {
        return gpiod_line_get_value(led_lines[pin_idx]);
    }

    return false;
}

static void led_pin_set(uint8_t pin_idx, bool value)
{
    if (pin_idx < ARRAY_SIZE(r807_led_pins))
    {
        gpiod_line_set_value(led_lines[pin_idx], value);
    }
}

static void led_pin_toggle(uint8_t pin_idx)
{
    if (pin_idx < ARRAY_SIZE(r807_led_pins))
    {
        int const value = gpiod_line_get_value(led_lines[pin_idx]);
        gpiod_line_set_value(led_lines[pin_idx], value ^ 1u);
    }
}

static struct Ex10GpioDriver const ex10_gpio_driver = {
    .gpio_initialize         = gpio_initialize,
    .gpio_cleanup            = gpio_cleanup,
    .set_board_power         = set_board_power,
    .get_board_power         = get_board_power,
    .set_ex10_enable         = set_ex10_enable,
    .get_ex10_enable         = get_ex10_enable,
    .register_irq_callback   = register_irq_callback,
    .deregister_irq_callback = deregister_irq_callback,
    .thread_is_irq_monitor   = thread_is_irq_monitor,
    .irq_enable              = irq_enable,
    .assert_reset_n          = assert_reset_n,
    .deassert_reset_n        = deassert_reset_n,
    .release_ready_n         = release_ready_n,
    .assert_ready_n          = assert_ready_n,
    .reset_device            = reset_device,
    .busy_wait_ready_n       = busy_wait_ready_n,
    .ready_n_pin_get         = ready_n_pin_get,
    .debug_pin_get_count     = debug_pin_get_count,
    .debug_pin_get           = debug_pin_get,
    .debug_pin_set           = debug_pin_set,
    .debug_pin_toggle        = debug_pin_toggle,
    .led_pin_get_count       = led_pin_get_count,
    .led_pin_get             = led_pin_get,
    .led_pin_set             = led_pin_set,
    .led_pin_toggle          = led_pin_toggle,
};

struct Ex10GpioDriver const* get_ex10_gpio_driver(void)
{
    return &ex10_gpio_driver;
}
