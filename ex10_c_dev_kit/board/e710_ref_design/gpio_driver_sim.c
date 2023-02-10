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

#include "board/gpio_driver.h"
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>


static void gpio_initialize(bool board_power_on, bool ex10_enable, bool reset)
{
    (void)board_power_on;
    (void)ex10_enable;
    (void)reset;
}

static int register_irq_callback(void (*cb_func)(void))
{
    assert(cb_func == NULL);
    return 0;
}

static void irq_enable(bool enable)
{
    (void)enable;
}
static int deregister_irq_callback(void)
{
    return 0;
}
static void gpio_cleanup(void) {}
static void set_board_power(bool power_on)
{
    (void)power_on;
}
static bool get_board_power(void)
{
    return true;
}
static void set_ex10_enable(bool enable)
{
    (void)enable;
}
static bool get_ex10_enable(void)
{
    return true;
}
static void assert_ready_n(void) {}
static void release_ready_n(void) {}
static bool thread_is_irq_monitor(void)
{
    return false;
}
static void assert_reset_n(void) {}
static void deassert_reset_n(void) {}
static int  busy_wait_ready_n(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return 0;
}
static int ready_n_pin_get(void)
{
    return 1;
}

static void reset_device(void) {}

static size_t debug_pin_get_count(void)
{
    return 5;
}

static bool debug_pin_get(uint8_t pin_idx)
{
    (void)pin_idx;
    return true;
}

static void debug_pin_set(uint8_t pin_idx, bool value)
{
    (void)pin_idx;
    (void)value;
}

static void debug_pin_toggle(uint8_t pin_idx)
{
    (void)pin_idx;
}

static size_t led_pin_get_count(void)
{
    return 4;
}

static bool led_pin_get(uint8_t pin_idx)
{
    (void)pin_idx;
    return true;
}

static void led_pin_set(uint8_t pin_idx, bool value)
{
    (void)pin_idx;
    (void)value;
}

static void led_pin_toggle(uint8_t pin_idx)
{
    (void)pin_idx;
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
