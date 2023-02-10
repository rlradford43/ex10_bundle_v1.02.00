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

#include <assert.h>
#include <stdio.h>

#include "board/region.h"

//#define NDEBUG     //Enable NODEBUG to bypass all assertions at runtime.

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0u]))

static struct Region const* region;
static uint32_t             tcxo_frequency_khz = 0;
static uint16_t             channel_hop_table[MAX_CHANNELS];
static uint32_t             channel_table_khz[MAX_CHANNELS];
static size_t               active_channel_index  = 0;
static size_t               len_channel_hop_table = 0;

/**
 * @note These values must match the values enumerated in the documentation
 * Section 6.5.1 RfSynthesizerControl: Parameters for the RF synthesizer
 * Bits 18:16 RDivider.
 */
static const uint8_t divider_value_to_index[] =
    {24u, 48u, 96u, 192u, 30u, 60u, 120u, 240u};

static void initialize_region(const char* region_name, uint32_t tcxo_freq_khz)
{
    assert(region_name);

    tcxo_frequency_khz   = tcxo_freq_khz;
    active_channel_index = 0;

    struct Ex10RegionsTable const* regions_table = get_ex10_regions_table();

    /* Find region */
    region = regions_table->get_region(region_name);
    assert(region);

    /* Build channel hop table (kHz) */
    len_channel_hop_table = regions_table->build_channel_table(
        &region->regulatory_channels, channel_hop_table);
    assert(len_channel_hop_table <= MAX_CHANNELS);

    for (size_t iter = 0; iter < len_channel_hop_table; iter++)
    {
        channel_table_khz[iter] =
            region->regulatory_channels.start_freq +
            (channel_hop_table[iter] - 1) * region->regulatory_channels.spacing;
    }

    active_channel_index = 0;
}

static char const* get_name(void)
{
    return region->name;
}

static size_t get_next_index(void)
{
    size_t next_index = active_channel_index + 1;
    if (next_index == len_channel_hop_table)
    {
        next_index = 0;
    }
    return next_index;
}

static void update_active_channel(void)
{
    active_channel_index = get_next_index();
}

static size_t get_channel_table_size(void)
{
    return len_channel_hop_table;
}

static uint32_t get_active_channel_khz(void)
{
    return channel_table_khz[active_channel_index];
}

static uint32_t get_next_channel_khz(void)
{
    size_t next_channel_index = get_next_index();
    return channel_table_khz[next_channel_index];
}

static size_t get_active_channel_index(void)
{
    return active_channel_index;
}

static size_t get_next_channel_index(void)
{
    return get_next_index();
}

static int get_channel_index(uint32_t frequency_khz)
{
    int channel_index = -1;
    for (size_t iter = 0; iter < len_channel_hop_table; iter++)
    {
        if (channel_table_khz[iter] == frequency_khz)
        {
            channel_index = iter;
            break;
        }
    }

    return channel_index;
}

static void get_regulatory_timers(struct RegulatoryTimers* timers)
{
    assert(timers);
    assert(sizeof(*timers) == sizeof(region->regulatory_timers));

    timers->nominal          = region->regulatory_timers.nominal;
    timers->extended         = region->regulatory_timers.extended;
    timers->regulatory       = region->regulatory_timers.regulatory;
    timers->off_same_channel = region->regulatory_timers.off_same_channel;
}

static uint32_t get_pll_r_divider(void)
{
    return region->pll_divider;
}

static uint16_t calculate_n_divider(uint32_t freq_khz, uint8_t r_divider)
{
    return (uint16_t)((4 * freq_khz * r_divider + tcxo_frequency_khz / 2) /
                      tcxo_frequency_khz);
}

static uint8_t calculate_r_divider_index(uint8_t r_divider)
{
    for (uint8_t iter = 0; iter < sizeof(divider_value_to_index); iter++)
    {
        if (r_divider == divider_value_to_index[iter])
        {
            return iter;
        }
    }

    fprintf(stderr, "Unknown R divider value %u\n", r_divider);
    assert(0);

    /* Can't reach this point unless NDEBUG.  Calling function should check
     * return. */
    return 0xffu;
}

static void get_synthesizer_params(uint32_t                  freq_khz,
                                   struct SynthesizerParams* params)
{
    assert(params);

    if (freq_khz == 0u)
    {
        freq_khz = get_next_channel_khz();
    }

    uint32_t r_divider = get_pll_r_divider();

    params->freq_khz        = freq_khz;
    params->r_divider_index = calculate_r_divider_index(r_divider);
    params->n_divider       = calculate_n_divider(freq_khz, r_divider);
}

static uint32_t get_synthesizer_frequency_khz(uint8_t  r_divider_index,
                                              uint16_t n_divider)
{
    if (r_divider_index < ARRAY_SIZE(divider_value_to_index))
    {
        // For F tcxo = 24,000 kHz, F lo = 930,000 kHz, Rdiv = 240
        // The expected max N div < 40E3, F lo 930E3 > UIN32_MAX,
        // Use uint64_t for the numerator.
        uint32_t const r_divider   = divider_value_to_index[r_divider_index];
        uint64_t const numerator   = ((uint64_t)tcxo_frequency_khz) * n_divider;
        uint32_t const denominator = 4u * r_divider;
        uint64_t const frequency_khz = numerator / denominator;
        return (uint32_t)frequency_khz;
    }

    assert(0 && "r_divider_index out of range");
    return 0u;
}

static enum RfFilter get_rf_filter(void)
{
    return region->rf_filter;
}

static struct Ex10Region const ex10_region = {
    .init                          = initialize_region,
    .get_name                      = get_name,
    .update_active_channel         = update_active_channel,
    .get_channel_table_size        = get_channel_table_size,
    .get_active_channel_khz        = get_active_channel_khz,
    .get_next_channel_khz          = get_next_channel_khz,
    .get_active_channel_index      = get_active_channel_index,
    .get_next_channel_index        = get_next_channel_index,
    .get_channel_index             = get_channel_index,
    .get_regulatory_timers         = get_regulatory_timers,
    .get_synthesizer_params        = get_synthesizer_params,
    .get_synthesizer_frequency_khz = get_synthesizer_frequency_khz,
    .get_rf_filter                 = get_rf_filter,
    .get_pll_r_divider             = get_pll_r_divider,
    .calculate_n_divider           = calculate_n_divider,
    .calculate_r_divider_index     = calculate_r_divider_index,
};

struct Ex10Region const* get_ex10_region(void)
{
    return &ex10_region;
}
