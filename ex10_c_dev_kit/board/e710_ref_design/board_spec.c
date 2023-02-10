/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2022 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

#include "board/board_spec.h"
#include "board/e710_ref_design/ex10_gpio.h"

#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

int ex10_get_thread_id(void)
{
    return syscall(__NR_gettid);
}

static uint32_t get_default_gpio_output_levels(void)
{
    struct Ex10GpioConfig const gpio_config = {
        .antenna         = 1,
        .baseband_filter = BasebandFilterBandpass,
        .dio_0           = false,
        .dio_1           = false,
        .dio_6           = false,
        .dio_8           = false,
        .dio_13          = false,
        .pa_bias_enable  = true,
        .power_range     = PowerRangeHigh,
        .rf_enable       = true,
        .rf_filter       = UPPER_BAND,
    };

    return get_ex10_gpio_helpers()->get_levels(&gpio_config);
}

static uint32_t get_gpio_output_levels(
    uint8_t                 antenna,
    enum BasebandFilterType rx_baseband_filter,
    enum RfFilter           rf_filter)
{
    struct Ex10GpioConfig const gpio_config = {
        .antenna         = antenna,
        .baseband_filter = rx_baseband_filter,
        .pa_bias_enable  = true,
        .power_range     = PowerRangeHigh,
        .rf_enable       = true,
        .rf_filter       = rf_filter,
    };

    return get_ex10_gpio_helpers()->get_levels(&gpio_config);
}

static uint32_t get_gpio_output_enables(void)
{
    return get_ex10_gpio_helpers()->get_output_enables();
}

static void get_gpio_output_pins_set_clear(
    struct GpioPinsSetClear* gpio_pins_set_clear,
    uint8_t                  antenna,
    uint16_t                 tx_power_cdbm,
    enum BasebandFilterType  baseband_filter,
    enum RfFilter            tx_rf_filter)
{
    enum PowerRange const power_range =
        (tx_power_cdbm <= LOW_BIAS_TX_POWER_MAX_CDBM) ? PowerRangeLow
                                                      : PowerRangeHigh;

    gpio_pins_set_clear->output_level_set    = 0u;
    gpio_pins_set_clear->output_level_clear  = 0u;
    gpio_pins_set_clear->output_enable_set   = 0u;
    gpio_pins_set_clear->output_enable_clear = 0u;

    struct Ex10GpioHelpers const* gpio_helpers = get_ex10_gpio_helpers();

    gpio_helpers->set_antenna_port(gpio_pins_set_clear, antenna);
    gpio_helpers->set_pa_power_range(gpio_pins_set_clear, power_range);
    gpio_helpers->set_rx_baseband_filter(gpio_pins_set_clear, baseband_filter);
    gpio_helpers->set_tx_rf_filter(gpio_pins_set_clear, tx_rf_filter);
}

static struct RxGainControlFields const* get_default_rx_analog_config(void)
{
    static struct RxGainControlFields const rx_gain = {
        .rx_atten        = RxAttenAtten_0_dB,
        .pga1_gain       = Pga1GainGain_12_dB,
        .pga2_gain       = Pga2GainGain_0_dB,
        .pga3_gain       = Pga3GainGain_18_dB,
        .Reserved0       = 0u,
        .mixer_gain      = MixerGainGain_11p2_dB,
        .pga1_rin_select = false,
        .Reserved1       = 0u,
        .mixer_bandwidth = true};

    return &rx_gain;
}

static uint16_t get_sjc_residue_threshold(void)
{
    /**
     * @see set_residue_threshold()
     * @details This value is empirically derived. It represents the SJC
     * residue magnitude threshold for measured LO signal at the
     * PGA3 output stage of the receiver.
     * Residue magnitude values larger than this are considered a failed
     * SJC solution.
     */
    return 10000u;
}

/**
 * @details
 * @note This value is empirically derived using the time_to_first_tag.c
 *       example.
 */
static uint32_t get_pa_bias_power_on_delay_ms(void)
{
    return 2u;
}

static struct Ex10BoardSpec const ex10_board_spec = {
    .get_default_gpio_output_levels = get_default_gpio_output_levels,
    .get_gpio_output_levels         = get_gpio_output_levels,
    .get_gpio_output_enables        = get_gpio_output_enables,
    .get_gpio_output_pins_set_clear = get_gpio_output_pins_set_clear,
    .get_default_rx_analog_config   = get_default_rx_analog_config,
    .get_sjc_residue_threshold      = get_sjc_residue_threshold,
    .get_pa_bias_power_on_delay_ms  = get_pa_bias_power_on_delay_ms,
};

struct Ex10BoardSpec const* get_ex10_board_spec(void)
{
    return &ex10_board_spec;
}
