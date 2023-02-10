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

#include <stddef.h>
#include <stdint.h>

#include "ex10_api/regions_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct SynthesizerParams
 * The synthesizer parameters required to set the LO frequency.
 */
struct SynthesizerParams
{
    uint32_t freq_khz;
    uint8_t  r_divider_index;
    uint16_t n_divider;
};

/**
 * @struct Ex10Region
 * The region programming interface.
 */
struct Ex10Region
{
    /**
     * Initializes the requested region.
     *
     * Load the region parameters from the regions table and builds the region's
     * frequency channels table.
     * @see ex10_api/src/regions.c
     * @param region_name Requested regulatory region name, which should match
     * the 'name' field in the region_table.
     * @param tcxo_freq_khz Crystal oscillator frequency used with the Ex10
     * device.
     */
    void (*init)(const char* region_name, uint32_t tcxo_freq_khz);

    /**
     * Get the active region name.
     *
     * @return char const* The ASCII null terminated string associated with the
     *                     region that was initialized and is now active.
     * @retval NULL        The region, when initialized, was invalid,
     *                     or the region failed to initialize correctly.
     */
    char const* (*get_name)(void);

    /**
     * Update which frequency to use in the next regulatory round.
     */
    void (*update_active_channel)(void);

    /**
     * Get the number of channels within the region channel table.
     *
     * @note The initialize_region() function must have been called,
     *       otherwise this function will return zero.
     *
     * @return size_t The number of channels in the region's channel table.
     */
    size_t (*get_channel_table_size)(void);

    /**
     * Gets the frequency used by the current CW ramp cycle
     * @return current channel frequency (kHz)
     */
    uint32_t (*get_active_channel_khz)(void);

    /**
     * Gets the frequency used by the next channel in the region table
     * This is used as a look ahead to calculate the parameters to pass
     * to the cw_on() function for ramping up on the next channel.
     * @return current channel frequency (kHz)
     */
    uint32_t (*get_next_channel_khz)(void);

    /**
     * Gets the current channel index in the region table used
     * by the current CW ramp cycle
     */
    size_t (*get_active_channel_index)(void);

    /**
     * Gets the next channel index in the region table that will
     * be used when the it ramps up next
     */
    size_t (*get_next_channel_index)(void);

    /**
     * Helper function to look up what the channel index is for
     * a given frequency.  It will return -1 if the frequency is
     * not in the frequency hop list
     *
     * @param frequency_khz target channel frequency in khz
     *
     * @return the channel index of the frequency
     */
    int (*get_channel_index)(uint32_t frequency_khz);

    /**
     * Returns the regulatory dwell times for this region.
     * @param [out] timers A struct to hold returned timer information.
     */
    void (*get_regulatory_timers)(struct RegulatoryTimers* timers);

    /**
     * Generates the required synthesizer parameters for the next channel in the
     * region.
     * @param freq_khz     The frequency to use.  If 0, choose channel.
     * @param synth_params A struct to hold returned parameters.
     *
     * @return Updated synth_params.
     */
    void (*get_synthesizer_params)(uint32_t                  freq_khz,
                                   struct SynthesizerParams* synth_params);

    /**
     * Based on the board region settings and the synthesizer params,
     * return the synthesizer LO frequency in kHz.
     *
     * @param r_divider_index
     * These values must match the values enumerated in the documentation
     * RfSynthesizerControl: Parameters for the RF synthesizer
     * Bits 18:16 RDivider.
     * @note r_divider_index references to a R divider value and is not the
     *       R divider itself.
     *
     * @param n_divider The synthesizer divisor where:
     *
     *     F_LO = (FREF * NDivider) / (RDivider * 4)
     *     FREF is the Tcxo of the board.
     *
     * @return uint32_t The synthesizer LO frequency in kHz.
     */
    uint32_t (*get_synthesizer_frequency_khz)(uint8_t  r_divider_index,
                                              uint16_t n_divider);

    /**
     * Returns the filter that should be used for this region.
     * @return an enum value indicating which filter to use
     */
    enum RfFilter (*get_rf_filter)(void);

    /**
     * Get the PLL R divider from the current table
     * @return R divider value of this region
     */
    uint32_t (*get_pll_r_divider)(void);

    /**
     * Calculate n_divider using known TXCO frequency, frequency, and r_divider.
     * @param frequency The LO frequency in kHz.
     * @param r_divider PLL R divider value
     *
     * @return N divider value for this frequency
     */
    uint16_t (*calculate_n_divider)(uint32_t frequency, uint8_t r_divider);

    /**
     * Translate from R divider values to index in the rang [0,7]
     * @param r_divider PLL R divider value
     *
     * @return Index representing the provided R divider value
     */
    uint8_t (*calculate_r_divider_index)(uint8_t r_divider);
};

struct Ex10Region const* get_ex10_region(void);

#ifdef __cplusplus
}
#endif
