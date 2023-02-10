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

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this test */
static const uint32_t etsi_burst_time_on =
    15 * 1000;  // Duration in milliseconds
static const uint8_t  antenna             = 1;
static const uint16_t rf_mode             = mode_241;
static const uint16_t transmit_power_cdbm = 3000;
static const uint8_t  initial_q           = 4;
static const bool     verbose             = true;

static int etsi_burst_example(struct Ex10Interfaces ex10_iface)
{
    printf("Starting ETSI Burst test\n");

    bool     ramp_down_seen         = false;
    bool     ramp_up_seen           = false;
    bool     inventory_summary_seen = false;
    uint32_t start_time             = get_ex10_time_helpers()->time_now();

    struct InventoryRoundControlFields inventory_config = {
        .initial_q            = initial_q,
        .max_q                = initial_q,
        .min_q                = initial_q,
        .num_min_q_cycles     = 1,
        .fixed_q_mode         = true,
        .q_increase_use_query = false,
        .q_decrease_use_query = false,
        .session              = 0,
        .select               = 0,
        .target               = 0,
        .halt_on_all_tags     = false,
        .tag_focus_enable     = false,
        .fast_id_enable       = false,
    };

    struct InventoryRoundControl_2Fields inventory_config_2 = {
        .max_queries_since_valid_epc = 0};

    // Choose any frequency to start on
    struct OpCompletionStatus op_status = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    op_status = ex10_iface.reader->etsi_burst_test(
        &inventory_config,
        &inventory_config_2,
        antenna,
        rf_mode,
        transmit_power_cdbm,
        400,  // Time to remain on and transmitting etsi burst for each round
        100,  // Time to remain off between transmitting rounds
        0);   // Grab a frequency from the region
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    // Throw away startup reports to ensure they are from etsi burst
    bool first_ramp_down_recieved = false;
    while (first_ramp_down_recieved == false)
    {
        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        if (packet)
        {
            if (packet->packet_type == TxRampDown)
            {
                first_ramp_down_recieved = true;
            }
            ex10_iface.reader->packet_remove();
        }
    }

    // Begin loop to ensure etsi burst is running
    while (get_ex10_time_helpers()->time_elapsed(start_time) <
           etsi_burst_time_on)
    {
        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        if (packet)
        {
            if (verbose)
            {
                printf("packet type: %d\n", packet->packet_type);
            }

            // Check for necessary events that are part of etsi burst
            if (packet->packet_type == TxRampDown)
            {
                assert(packet->static_data->tx_ramp_down.reason ==
                       RampDownRegulatory);
                ramp_down_seen = true;
            }
            else if (packet->packet_type == TxRampUp)
            {
                ramp_up_seen = true;
            }
            else if (packet->packet_type == InventoryRoundSummary)
            {
                inventory_summary_seen = true;
            }
            ex10_iface.reader->packet_remove();
        }
    }

    ex10_iface.reader->stop_transmitting();

    assert(ramp_down_seen == true);
    assert(ramp_up_seen == true);
    assert(inventory_summary_seen == true);

    printf("Ending ETSI Burst test\n");

    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "ETSI_LOWER");

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        result = etsi_burst_example(ex10_iface);
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
