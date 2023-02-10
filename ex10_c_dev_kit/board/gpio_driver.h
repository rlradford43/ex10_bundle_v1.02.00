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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @struct Ex10GpioDriver
 * The Ex10 GPIO driver interface.
 */
struct Ex10GpioDriver
{
    /**
     * Acquire ownership of the Ex10 connection pins from the the GPIO driver
     * and initialize their state. i.e Set the pins' directions and levels.
     *
     * @param board_power_on If true, asserts the PWR_EN line to enable the
     *                       the Impinj Reader Chip power supply.
     *                       If false, power is disabled to the Reader Chip.
     * @param ex10_enable    If true the Ex10 ENABLE is asserted high.
     *                       If false the Ex10 ENABLE line is deasserted low.
     * @param reset          If true the RESET_N line is asserted low.
     *                       if false the RESET_N line is deasserted high.
     */
    void (*gpio_initialize)(bool board_power_on, bool ex10_enable, bool reset);
    void (*gpio_cleanup)(void);

    /**
     * Sets the PWR_EN line high to supply the EX10 VDD with power or
     * sets the PWR_EN line low to disable power to the EX10 VDD.
     *
     * @param power_on true to assert PWR_EN high; false to deassert PWR_EN.
     */
    void (*set_board_power)(bool power_on);
    bool (*get_board_power)(void);

    /**
     * Assert the ENABLE line high or deassert the ENABLE line low.
     * When the ENABLE line is low the EX10 device will internally power down.
     * When the ENABLE line transitions from low to high the Ex10 device
     * executes a POR (power-on-reset).
     *
     * @note If the power_on parameter is true, this call will not return until
     *       the RESET_N pin is high ('1').
     *
     * @param enable When true the ENABLE line is assert high; when false the
     *               ENABLE line is deasserted low.
     */
    void (*set_ex10_enable)(bool enable);
    bool (*get_ex10_enable)(void);

    /**
     * Registers a callback function which occurs on the falling edge of the
     * IRQ_N_PIN and is processed within the irq_n_monitor thread context.
     *
     * Successful registration of the interrupt callback will start the
     * IRQ_N monitor thread, which will call back within this thread context
     * to the client supplied cb_func parameter when IRQ_N asserts low.
     *
     * @param cb_func The function that is called when IRQ_N asserts low.
     *
     * @return int POSIX error code indicating success (0) or failure (!= 0).
     */
    int (*register_irq_callback)(void (*cb_func)(void));

    /**
     * Deregister the callback function from the IRQ_N monitor thread.
     * This will destroy the IRQ_N monitor thread and remove the callback
     *  entry from the gpio driver.
     *
     * @return int POSIX error code indicating success (0) or failure (!= 0).
     */
    int (*deregister_irq_callback)(void);

    /**
     * Enables or disables the irq based on the passed in
     * enable bool. Asserts that the pthread lock returns
     * no error.
     *
     * @param enable If true, the irq is enabled, else
     *               it is disabled.
     */
    void (*irq_enable)(bool enable);

    /**
     * Determine whether the IRQ_N monitor thread is the same thread calling
     * this function. This is useful for profiling and debug purpposes.
     *
     * @return bool true if the calling thread matches the IRQ_N monitor thread,
     *              false if it does not.
     */
    bool (*thread_is_irq_monitor)(void);

    void (*assert_reset_n)(void);    ///< Assert the RESET_N line low.
    void (*deassert_reset_n)(void);  ///< Deassert the RESET_N line high.

    /// Sets the READY_N line as an input.
    void (*release_ready_n)(void);

    /// Assert the READY_N line low.
    void (*assert_ready_n)(void);

    /// Pulse the RESET_N line low, then high; resetting the Impinj Reader Chip.
    void (*reset_device)(void);

    /**
     * Waits for the READY_N pin to assert LOW with a specified timeout.
     *
     * @param timeout_ms The amount of time to wait, in milliseconds, for
     *                    READY_N to assert.
     *
     * @return int A indicator of success or failure.
     * @retval     0 The call was successful.
     * @retval    -1 The call was unsuccessful.
     *               Check errno for the failure reason.
     */
    int (*busy_wait_ready_n)(uint32_t timeout_ms);

    /**
     * Get the state of the READY_N GPI line.
     *
     * @return int The state of READY_N
     */
    int (*ready_n_pin_get)(void);

    /**
     * Get the number of GPIO pins on the board which can be used as debug pins.
     *
     * @return size_t The number of GPIO pins that can be used for debug
     * purposes.
     */
    size_t (*debug_pin_get_count)(void);

    /**
     * Get the debug pin state.
     *
     * @param pin_idx The debug pin to inquire about: [0 ... N), where N is the
     *                number of debug pins reported by the debug_pin_get_count()
     *                function.
     *
     * @return bool true: The debug pin is '1', false if it is '0'.
     */
    bool (*debug_pin_get)(uint8_t pin_idx);

    /**
     * Set the debug pin state.
     *
     * @param pin_idx The debug pin to set: [0 ... N) where N is the number of
     *                debug pins reported by the debug_pin_get_count() function.
     *
     * @param value Sets the debug pin output level: true = '1', false = '0'.
     */
    void (*debug_pin_set)(uint8_t pin_idx, bool value);

    /**
     * Toggle the debug pin state.
     *
     * @param pin_idx The debug pin to toggle: [0 ... N) where N is the number
     * of debug pins reported by the debug_pin_get_count() function.
     */
    void (*debug_pin_toggle)(uint8_t pin_idx);

    /**
     * Get the number of pins on the board which are connected to LEDs.
     *
     * @return size_t The number of GPIO pins that are connected to LEDs.
     */
    size_t (*led_pin_get_count)(void);

    /**
     * Get the LED pin state.
     *
     * @param pin_idx The LED pin to inquire about: [0 ... N), where N is the
     *                number of LEDs reported by the led_pin_get_count()
     * function.
     *
     * @return bool true: The LED is lit, false if it is not lit.
     */
    bool (*led_pin_get)(uint8_t pin_idx);

    /**
     * Set the LED pin state.
     *
     * @param pin_idx The LED pin to set: [0 ... N), where N is the number of
     *                LEDs reported by the led_pin_get_count() function.
     *
     * @param value Set to true to light the LED, to false ot turn it off.
     */
    void (*led_pin_set)(uint8_t pin_idx, bool value);

    /**
     * Toggle the LED pin state.
     *
     * @param pin_idx The LED pin to toggle: [0 ... N), where N is the number of
     *                LEDs reported by the led_pin_get_count() function.
     */
    void (*led_pin_toggle)(uint8_t pin_idx);
};

struct Ex10GpioDriver const* get_ex10_gpio_driver(void);

#ifdef __cplusplus
}
#endif
