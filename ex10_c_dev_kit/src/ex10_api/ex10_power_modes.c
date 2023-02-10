/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2022 Impinj, Inc. All rights reserved.                      *
 *                                                                           *
 *****************************************************************************/

#include "ex10_api/ex10_power_modes.h"

#include "board/board_spec.h"
#include "board/ex10_gpio.h"
#include "board/time_helpers.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/power_transactor.h"
#include "ex10_api/trace.h"

/**
 * The timeout, in milliseconds, to wait for the Ex10Reader layer to complete
 * packet processing after Ex10Reader.stop_transmitting() is called.
 */
static uint32_t const stop_transmitter_timeout_ms = 500u;

struct Ex10PowerModesPrivate
{
    struct Ex10Reader const* reader;
    struct Ex10Ops const*    ops;
    enum PowerMode           power_mode;
};

static struct Ex10PowerModesPrivate power_modes = {
    .reader     = NULL,
    .ops        = NULL,
    .power_mode = PowerModeReady,
};

static void init(void)
{
    power_modes.reader     = get_ex10_reader();
    power_modes.ops        = get_ex10_ops();
    power_modes.power_mode = PowerModeReady;
}

static void deinit(void) {}

static struct OpCompletionStatus stop_transmitter_and_wait(void)
{
    struct OpCompletionStatus op_error =
        power_modes.reader->stop_transmitting();
    if (op_error.error_occurred)
    {
        return op_error;
    }

    enum InventoryState inventory_state =
        power_modes.reader->get_continuous_inventory_state()->state;

    uint32_t const start_time_ms = get_ex10_time_helpers()->time_now();
    while ((inventory_state != InvIdle) &&
           (get_ex10_time_helpers()->time_elapsed(start_time_ms) <
            stop_transmitter_timeout_ms))
    {
        inventory_state =
            power_modes.reader->get_continuous_inventory_state()->state;
    }

    if (inventory_state != InvIdle)
    {
        op_error.error_occurred = true;
        op_error.timeout_error  = true;
    }

    return op_error;
}

static struct OpCompletionStatus set_gpio_pins(bool pa_bias_enable,
                                               bool rf_ps_enable)
{
    struct Ex10GpioHelpers const* gpio_helpers   = get_ex10_gpio_helpers();
    struct GpioPinsSetClear       gpio_set_clear = {0u, 0u, 0u, 0u};
    gpio_helpers->set_rf_power_supply_enable(&gpio_set_clear, rf_ps_enable);
    gpio_helpers->set_pa_bias_enable(&gpio_set_clear, pa_bias_enable);
    power_modes.ops->set_clear_gpio_pins(&gpio_set_clear);
    return power_modes.ops->wait_op_completion();
}

static struct OpCompletionStatus powerup_and_init_ex10(void)
{
    int const powerup_status =
        get_ex10_power_transactor()->power_up_to_application();
    if (powerup_status != Application)
    {
        struct OpCompletionStatus op_error = *power_modes.ops->op_error_none;
        op_error.error_occurred            = true;
        return op_error;
    }

    struct OpCompletionStatus op_error = power_modes.ops->init_ex10();

    if (op_error.error_occurred == false)
    {
        op_error = power_modes.reader->init_ex10();
    }

    return op_error;
}

static struct OpCompletionStatus set_power_mode_cold(bool radio_power_enable)
{
    struct OpCompletionStatus op_error = stop_transmitter_and_wait();

    if (op_error.error_occurred == false)
    {
        bool const pa_bias_enable = false;
        bool const rf_ps_enable   = false;
        op_error                  = set_gpio_pins(pa_bias_enable, rf_ps_enable);
    }

    if (op_error.error_occurred == false)
    {
        power_modes.ops->radio_power_control(radio_power_enable);
        op_error = power_modes.ops->wait_op_completion();
    }

    return op_error;
}

static struct OpCompletionStatus set_power_mode_off(void)
{
    struct OpCompletionStatus const op_error = stop_transmitter_and_wait();

    get_ex10_power_transactor()->power_down();

    // Flush all packets contained within the SDK FifoBufferNode nodes.
    // Note: flush_packets must be false since the Ex10 is powered down.
    bool const print_packets      = false;
    bool const flush_packets      = false;
    bool const debug_aggregate_op = false;
    get_ex10_helpers()->discard_packets(
        print_packets, flush_packets, debug_aggregate_op);

    power_modes.power_mode =
        op_error.error_occurred ? power_modes.power_mode : PowerModeOff;

    return op_error;
}

static struct OpCompletionStatus set_power_mode_standby(void)
{
    struct OpCompletionStatus op_error = *power_modes.ops->op_error_none;

    bool const ex10_radio_power_enable = false;
    op_error = set_power_mode_cold(ex10_radio_power_enable);
    power_modes.power_mode =
        op_error.error_occurred ? power_modes.power_mode : PowerModeStandby;
    return op_error;
}

static struct OpCompletionStatus set_power_mode_ready_cold(void)
{
    struct OpCompletionStatus op_error = *power_modes.ops->op_error_none;

    bool const ex10_radio_power_enable = true;
    op_error = set_power_mode_cold(ex10_radio_power_enable);
    power_modes.power_mode =
        op_error.error_occurred ? power_modes.power_mode : PowerModeReadyCold;
    return op_error;
}

static struct OpCompletionStatus set_power_mode_ready(void)
{
    struct OpCompletionStatus op_error = *power_modes.ops->op_error_none;

    bool const ex10_radio_power_enable = true;
    power_modes.ops->radio_power_control(ex10_radio_power_enable);
    op_error = power_modes.ops->wait_op_completion();

    if (op_error.error_occurred == false)
    {
        bool const pa_bias_enable = true;
        bool const rf_ps_enable   = true;
        op_error                  = set_gpio_pins(pa_bias_enable, rf_ps_enable);
    }

    uint32_t const delay_time_ms =
        get_ex10_board_spec()->get_pa_bias_power_on_delay_ms();
    get_ex10_time_helpers()->busy_wait_ms(delay_time_ms);

    power_modes.power_mode =
        op_error.error_occurred ? power_modes.power_mode : PowerModeReady;
    return op_error;
}

static struct OpCompletionStatus set_power_mode(enum PowerMode power_mode)
{
    if (power_modes.power_mode != power_mode)
    {
        if (power_modes.power_mode == PowerModeOff)
        {
            struct OpCompletionStatus const op_error = powerup_and_init_ex10();
            if (op_error.error_occurred)
            {
                return op_error;
            }
        }

        switch (power_mode)
        {
            case PowerModeInvalid:
                break;  // Invalid state, handle as error condition.
            case PowerModeOff:
                return set_power_mode_off();
            case PowerModeStandby:
                return set_power_mode_standby();
            case PowerModeReadyCold:
                return set_power_mode_ready_cold();
            case PowerModeReady:
                return set_power_mode_ready();
            default:
                break;  // Invalid state, handle as error condition.
        }

        // Invalid state encountered.
        // No op was run, but indicate that an error occurred.
        struct OpCompletionStatus op_error = *power_modes.ops->op_error_none;
        op_error.error_occurred            = true;
        return op_error;
    }

    // The power mode is unchanged, do nothing and report all is well.
    return *power_modes.ops->op_error_none;
}

static enum PowerMode get_power_mode(void)
{
    return power_modes.power_mode;
}

struct Ex10PowerModes const* get_ex10_power_modes(void)
{
    static struct Ex10PowerModes power_modes_instance = {
        .init           = init,
        .deinit         = deinit,
        .set_power_mode = set_power_mode,
        .get_power_mode = get_power_mode,
    };

    return &power_modes_instance;
}
