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
#include <stdlib.h>

#include "board/region.h"
#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
static const uint8_t  antenna                     = 1u;
static const uint16_t rf_mode                     = mode_103;
static const uint16_t transmit_power_cdbm         = 3000u;
static const uint8_t  initial_q                   = 2u;
static const uint8_t  max_q                       = 15u;
static const uint8_t  min_q                       = 0u;
static const uint8_t  num_min_q_cycles            = 1u;
static const uint16_t max_queries_since_valid_epc = 16u;
static const uint8_t  select_all                  = 0u;
static const bool     dual_target                 = true;
static const uint8_t  session                     = 0u;
static const uint8_t  target                      = 0u;


struct InfoFromPackets            packet_info = {0u, 0u, 0u, 0u, {0u}};
struct ContinuousInventorySummary continuous_inventory_summary = {0};
struct StopConditions             stop_conditions              = {0};

static int continuous_inventory_stop_on_inventory_round_count(
    struct Ex10Interfaces            ex10_iface,
    struct ContInventoryHelperParams cihp)
{
    stop_conditions.max_number_of_tags   = 0u;
    stop_conditions.max_duration_us      = 0u;
    stop_conditions.max_number_of_rounds = 7u;

    if (ex10_iface.helpers->continuous_inventory(&cihp))
    {
        return -1;
    }

    assert(continuous_inventory_summary.reason == SRMaxNumberOfRounds);
    assert(packet_info.total_singulations > 0);

    printf("Total Singulations: %zd\n", packet_info.total_singulations);

    return 0;
}

static int continuous_inventory_stop_on_max_tags_count(
    struct Ex10Interfaces            ex10_iface,
    struct ContInventoryHelperParams cihp)
{
    stop_conditions.max_number_of_tags   = 40u;
    stop_conditions.max_duration_us      = 0u;
    stop_conditions.max_number_of_rounds = 0u;

    if (ex10_iface.helpers->continuous_inventory(&cihp))
    {
        return -1;
    }

    assert(continuous_inventory_summary.reason == SRMaxNumberOfTags);
    assert(packet_info.total_singulations > 0);

    printf("Total Singulations: %zd\n", packet_info.total_singulations);

    return 0;
}

static int continuous_inventory_stop_on_duration(
    struct Ex10Interfaces            ex10_iface,
    struct ContInventoryHelperParams cihp)
{
    stop_conditions.max_number_of_tags   = 0u;
    stop_conditions.max_duration_us      = 10u * 1000u * 1000u;
    stop_conditions.max_number_of_rounds = 0u;

    if (ex10_iface.helpers->continuous_inventory(&cihp))
    {
        return -1;
    }

    assert(continuous_inventory_summary.reason == SRMaxDuration);
    assert(packet_info.total_singulations > 0);

    printf("Total Singulations: %zd\n", packet_info.total_singulations);

    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);

    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        printf("Starting continuous inventory example\n");

        struct InventoryRoundControlFields inventory_config = {
            .initial_q            = initial_q,
            .max_q                = max_q,
            .min_q                = min_q,
            .num_min_q_cycles     = num_min_q_cycles,
            .fixed_q_mode         = false,
            .q_increase_use_query = false,
            .q_decrease_use_query = false,
            .session              = session,
            .select               = select_all,
            .target               = target,
            .halt_on_all_tags     = false,
            .tag_focus_enable     = false,
            .fast_id_enable       = false,
        };

        struct InventoryRoundControl_2Fields const inventory_config_2 = {
            .max_queries_since_valid_epc = max_queries_since_valid_epc};

        struct InventoryHelperParams inventory_params = {
            .antenna               = antenna,
            .rf_mode               = rf_mode,
            .tx_power_cdbm         = transmit_power_cdbm,
            .inventory_config      = &inventory_config,
            .inventory_config_2    = &inventory_config_2,
            .send_selects          = false,
            .remain_on             = false,
            .dual_target           = dual_target,
            .inventory_duration_ms = 0,  // irrelevant for continuous inventory
            .packet_info           = &packet_info,
            .verbose               = true,
        };

        struct ContInventoryHelperParams cihp = {
            .inventory_params = &inventory_params,
            .stop_conditions  = &stop_conditions,
            .summary_packet   = &continuous_inventory_summary};

        result =
            (continuous_inventory_stop_on_inventory_round_count(ex10_iface,
                                                                cihp) ||
             continuous_inventory_stop_on_max_tags_count(ex10_iface, cihp) ||
             continuous_inventory_stop_on_duration(ex10_iface, cihp))
                ? -1
                : 0;
    }

    printf("Ending continuous inventory example\n");

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
