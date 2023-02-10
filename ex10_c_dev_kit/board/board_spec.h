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

#include "board/ex10_gpio.h"
#include "board_spec_constants.h"

#include "ex10_api/application_register_definitions.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @return int A tid for the current thread context. */
int ex10_get_thread_id(void);

struct Ex10BoardSpec
{
    /**
     * Generate the default GPIO output level configuration desired.
     * This can be used at boot or when not running inventory;
     * i.e. when CW is not ramped up to operational power.
     *
     * @return uint32_t A bit field of GPIO levels which, when set to '1',
     *                  indicates that the GPIO output level is high.
     */
    uint32_t (*get_default_gpio_output_levels)(void);

    /**
     * Generate the GPIO output level configuration required to
     * ramp to power using the provided parameters.
     *
     * @param antenna            The antenna port to select.
     * @param rx_baseband_filter The Rx baseband filter selection.
     * @param tx_rf_filter    An enum RfFilter value used to select the Tx SAW.
     *
     * @return uint32_t A bit field of GPIO levels which can be written to the
     *                  GpioOutputLevel register.
     */
    uint32_t (*get_gpio_output_levels)(
        uint8_t                 antenna,
        enum BasebandFilterType rx_baseband_filter,
        enum RfFilter           tx_rf_filter);

    /**
     * Get the GPIO output enable bit-field that will enable the appropriate
     * GPIO pins required for proper board operation.
     *
     * @return uint32_t A bit field of GPIO pins which must be enabled as
     *                  outputs for proper board operation.
     */
    uint32_t (*get_gpio_output_enables)(void);

    /**
     * Based on the parameters antenna, tx_power_cdbm, rx_baseband_filter,
     * and tx_rf_filter, modify the parameter 'gpio' to set the GPIO
     * output levels and enables.
     *
     * @note Only pins associated with the parameters
     *       antenna, rx_baseband_filter, tx_power_cdbm and tx_rf_filter
     *       are modified. The other pins represented by the member bit-fields
     *       within the struct GpioPinsSetClear are unchanged.
     *
     * @param [in] [out] gpio    A pointer to struct GpioPinsSetClear whose
     *                           fields will be bit-wise OR'd with the specific
     *                           bits associated with the passed in parameter
     *                           values. This combines the parameter based GPIO
     *                           pin settings with the pin settings passed in.
     * @param antenna            The antenna port to select.
     * @param tx_power_cdbm      The transmit power in cdBm units.
     * @param rx_baseband_filter The Rx baseband filter selection.
     * @param tx_rf_filter    An enum RfFilter value used to select the Tx SAW.
     */
    void (*get_gpio_output_pins_set_clear)(
        struct GpioPinsSetClear* gpio_pins_set_clear,
        uint8_t                  antenna,
        uint16_t                 tx_power_cdbm,
        enum BasebandFilterType  rx_baseband_filter,
        enum RfFilter            tx_rf_filter);

    /**
     * Returns the default receiver gains to use with the device.
     *
     * @return struct RxGainControlFields The default receiver gain settings.
     */
    struct RxGainControlFields const* (*get_default_rx_analog_config)(void);

    /**
     * Returns the SJC residue voltage magnitude threshold
     * used to determine SJC pass/fail criterion.
     * @see Ex10SjcAccessor::set_residue_threshold().
     */
    uint16_t (*get_sjc_residue_threshold)(void);

    /**
     * @return uint32_t The time, in milliseconds, required after power and bias
     *                  is applied to the power amplifier, until the system is
     *                  ready to inventory tags.
     */
    uint32_t (*get_pa_bias_power_on_delay_ms)(void);
};

struct Ex10BoardSpec const* get_ex10_board_spec(void);

#ifdef __cplusplus
}
#endif
