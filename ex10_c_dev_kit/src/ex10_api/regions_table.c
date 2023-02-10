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

#include "ex10_api/regions_table.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
// IPJ_autogen | generate_application_regions_c {

static uint16_t const HK_usable_channels[] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,};
static uint16_t const TAIWAN_usable_channels[] = { 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,};
static uint16_t const ETSI_LOWER_usable_channels[] = {  4,  7, 10, 13,};
static uint16_t const ETSI_UPPER_usable_channels[] = {  3,  6,  9,};
static uint16_t const MALAYSIA_usable_channels[] = { 34, 35, 36, 37, 38, 39, 40, 41,};
static uint16_t const CHINA_usable_channels[] = {  3,  7, 11, 15,};
static uint16_t const BRAZIL_usable_channels[] = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,};
static uint16_t const THAILAND_usable_channels[] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,};
static uint16_t const SINGAPORE_usable_channels[] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,};
static uint16_t const AUSTRALIA_usable_channels[] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,};
static uint16_t const INDIA_usable_channels[] = {  1,  4,  7, 10,};
static uint16_t const URUGUAY_usable_channels[] = { 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,};
static uint16_t const VIETNAM_usable_channels[] = { 33, 34, 35, 36, 37, 38, 39, 40,};
static uint16_t const ISRAEL_usable_channels[] = { 28,};
static uint16_t const INDONESIA_usable_channels[] = { 37, 38, 39, 30,};
static uint16_t const NEW_ZEALAND_usable_channels[] = { 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,};
static uint16_t const JAPAN2_usable_channels[] = {  5, 11, 17, 23,};
static uint16_t const PERU_usable_channels[] = { 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,};
static uint16_t const RUSSIA_usable_channels[] = {  1,  2,  3,  4,};

static struct Region const region_table[] = {
{
     .name                  = "FCC",

     .region_enum           = 0,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 50,
        .usable             = NULL,
        .usable_count       = 0u,
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "HK",

     .region_enum           = 3,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 10,
        .usable             = HK_usable_channels,
        .usable_count       = sizeof(HK_usable_channels) / sizeof(HK_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "TAIWAN",

     .region_enum           = 4,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 14,
        .usable             = TAIWAN_usable_channels,
        .usable_count       = sizeof(TAIWAN_usable_channels) / sizeof(TAIWAN_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "ETSI_LOWER",

     .region_enum           = 7,
     .regulatory_timers     =
     {
        .nominal            = 3800,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 100
     },
    .regulatory_channels    =
    {
        .start_freq         = 865100,
        .spacing            = 200,
        .count              = 4,
        .usable             = ETSI_LOWER_usable_channels,
        .usable_count       = sizeof(ETSI_LOWER_usable_channels) / sizeof(ETSI_LOWER_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 60,
    .rf_filter              = LOWER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "ETSI_UPPER",

     .region_enum           = 29,
     .regulatory_timers     =
     {
        .nominal            = 0,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 100
     },
    .regulatory_channels    =
    {
        .start_freq         = 915500,
        .spacing            = 400,
        .count              = 3,
        .usable             = ETSI_UPPER_usable_channels,
        .usable_count       = sizeof(ETSI_UPPER_usable_channels) / sizeof(ETSI_UPPER_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 60,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "KOREA",

     .region_enum           = 8,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 917300,
        .spacing            = 600,
        .count              = 6,
        .usable             = NULL,
        .usable_count       = 0u,
        .random_hop         = true
     },
    .pll_divider            = 60,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "MALAYSIA",

     .region_enum           = 9,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 8,
        .usable             = MALAYSIA_usable_channels,
        .usable_count       = sizeof(MALAYSIA_usable_channels) / sizeof(MALAYSIA_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "CHINA",

     .region_enum           = 10,
     .regulatory_timers     =
     {
        .nominal            = 1800,
        .extended           = 1980,
        .regulatory         = 2000,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 920125,
        .spacing            = 250,
        .count              = 4,
        .usable             = CHINA_usable_channels,
        .usable_count       = sizeof(CHINA_usable_channels) / sizeof(CHINA_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 48,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "SOUTH_AFRICA",

     .region_enum           = 12,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 915600,
        .spacing            = 200,
        .count              = 17,
        .usable             = NULL,
        .usable_count       = 0u,
        .random_hop         = true
     },
    .pll_divider            = 60,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "BRAZIL",

     .region_enum           = 13,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 35,
        .usable             = BRAZIL_usable_channels,
        .usable_count       = sizeof(BRAZIL_usable_channels) / sizeof(BRAZIL_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "THAILAND",

     .region_enum           = 14,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 10,
        .usable             = THAILAND_usable_channels,
        .usable_count       = sizeof(THAILAND_usable_channels) / sizeof(THAILAND_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "SINGAPORE",

     .region_enum           = 15,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 10,
        .usable             = SINGAPORE_usable_channels,
        .usable_count       = sizeof(SINGAPORE_usable_channels) / sizeof(SINGAPORE_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "AUSTRALIA",

     .region_enum           = 16,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 10,
        .usable             = AUSTRALIA_usable_channels,
        .usable_count       = sizeof(AUSTRALIA_usable_channels) / sizeof(AUSTRALIA_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "INDIA",

     .region_enum           = 17,
     .regulatory_timers     =
     {
        .nominal            = 3800,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 100
     },
    .regulatory_channels    =
    {
        .start_freq         = 865100,
        .spacing            = 200,
        .count              = 4,
        .usable             = INDIA_usable_channels,
        .usable_count       = sizeof(INDIA_usable_channels) / sizeof(INDIA_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 60,
    .rf_filter              = LOWER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "URUGUAY",

     .region_enum           = 18,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 23,
        .usable             = URUGUAY_usable_channels,
        .usable_count       = sizeof(URUGUAY_usable_channels) / sizeof(URUGUAY_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "VIETNAM",

     .region_enum           = 19,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 8,
        .usable             = VIETNAM_usable_channels,
        .usable_count       = sizeof(VIETNAM_usable_channels) / sizeof(VIETNAM_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "ISRAEL",

     .region_enum           = 0xFF,
     .regulatory_timers     =
     {
        .nominal            = 0,
        .extended           = 0,
        .regulatory         = 0,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 1,
        .usable             = ISRAEL_usable_channels,
        .usable_count       = sizeof(ISRAEL_usable_channels) / sizeof(ISRAEL_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "PHILIPPINES",

     .region_enum           = 21,
     .regulatory_timers     =
     {
        .nominal            = 3800,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 918250,
        .spacing            = 500,
        .count              = 4,
        .usable             = NULL,
        .usable_count       = 0u,
        .random_hop         = false
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "INDONESIA",

     .region_enum           = 23,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 4,
        .usable             = INDONESIA_usable_channels,
        .usable_count       = sizeof(INDONESIA_usable_channels) / sizeof(INDONESIA_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "NEW_ZEALAND",

     .region_enum           = 24,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 10,
        .usable             = NEW_ZEALAND_usable_channels,
        .usable_count       = sizeof(NEW_ZEALAND_usable_channels) / sizeof(NEW_ZEALAND_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "JAPAN2",

     .region_enum           = 25,
     .regulatory_timers     =
     {
        .nominal            = 3800,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 50
     },
    .regulatory_channels    =
    {
        .start_freq         = 916000,
        .spacing            = 200,
        .count              = 4,
        .usable             = JAPAN2_usable_channels,
        .usable_count       = sizeof(JAPAN2_usable_channels) / sizeof(JAPAN2_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 60,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "PERU",

     .region_enum           = 27,
     .regulatory_timers     =
     {
        .nominal            = 200,
        .extended           = 380,
        .regulatory         = 400,
        .off_same_channel   = 0
     },
    .regulatory_channels    =
    {
        .start_freq         = 902750,
        .spacing            = 500,
        .count              = 23,
        .usable             = PERU_usable_channels,
        .usable_count       = sizeof(PERU_usable_channels) / sizeof(PERU_usable_channels[0u]),
        .random_hop         = true
     },
    .pll_divider            = 24,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
{
     .name                  = "RUSSIA",

     .region_enum           = 0xFF,
     .regulatory_timers     =
     {
        .nominal            = 3800,
        .extended           = 3980,
        .regulatory         = 4000,
        .off_same_channel   = 100
     },
    .regulatory_channels    =
    {
        .start_freq         = 916200,
        .spacing            = 1200,
        .count              = 4,
        .usable             = RUSSIA_usable_channels,
        .usable_count       = sizeof(RUSSIA_usable_channels) / sizeof(RUSSIA_usable_channels[0u]),
        .random_hop         = false
     },
    .pll_divider            = 60,
    .rf_filter              = UPPER_BAND,
    .max_power_cdbm         = 3000,
},
};
// IPJ_autogen }
// clang-format on

static struct Region const* const region_table_begin = &region_table[0u];
static struct Region const* const region_table_end =
    &region_table[0u] + sizeof(region_table) / sizeof(region_table[0u]);

static struct Region const* get_region(char const* region_name)
{
    for (struct Region const* iter = region_table_begin;
         iter < region_table_end;
         ++iter)
    {
        // Note: strncmp() is not required here since each iter->name
        // field is statically defined and immutable.
        if (strcmp(iter->name, region_name) == 0)
        {
            return iter;
        }
    }
    return NULL;
}

/**
 * Shuffle a 'deck' of uint16_t 'cards'.
 *
 * @note This function uses the rand() function to shuffle the deck.
 * Seeding with srand() prior to calling this function is left to the client.
 * If the same seed for srand() is used each time, or if srand() is not called
 * then the shuffle will be the same each time.
 *
 * @param deck  An array of uint16_t nodes.
 * @param count The number of uint16_t nodes in the 'deck'.
 */
static void shuffle_u16(uint16_t* deck, size_t count)
{
    for (size_t idx = 0u; idx < count; ++idx)
    {
        size_t const   pick = rand() % (count - idx);
        uint16_t const temp = deck[idx];  // swap deck[idx] <-> deck[pick]
        deck[idx]           = deck[pick];
        deck[pick]          = temp;
    }
}

static size_t build_channel_table(
    struct RegulatoryChannels const* regulatory_channels,
    uint16_t*                        channel_table)
{
    _Static_assert(
        sizeof(*channel_table) == sizeof(*regulatory_channels->usable),
        "Mismatched channel index size");

    size_t channel_count = 0u;

    if (regulatory_channels->usable_count > 0u)
    {
        assert(regulatory_channels->usable);
        memcpy(channel_table,
               regulatory_channels->usable,
               regulatory_channels->usable_count * sizeof(*channel_table));

        channel_count = regulatory_channels->usable_count;
    }
    else
    {
        channel_count = regulatory_channels->count;

        for (uint16_t chn_idx = 0u; chn_idx < channel_count; ++chn_idx)
        {
            channel_table[chn_idx] = chn_idx + 1;
        }
    }

    if (regulatory_channels->random_hop)
    {
        shuffle_u16(channel_table, channel_count);
    }

    return channel_count;
}

static struct Ex10RegionsTable const ex10_regions_table = {
    .get_region          = get_region,
    .build_channel_table = build_channel_table,
};

struct Ex10RegionsTable const* get_ex10_regions_table(void)
{
    return &ex10_regions_table;
}