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

#include <stdio.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
const uint8_t  antenna             = 1;
const uint16_t rf_mode             = mode_148;
const uint16_t transmit_power_cdbm = 3000;
char           region_name[]       = "FCC";
const bool     verbose             = true;

static int simple_ramp_example(struct Ex10Interfaces ex10_iface)
{
    printf("Starting ramp test\n");

    // Sanity check for lower power output
    uint16_t                  adc       = 0u;
    struct OpCompletionStatus op_status = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    op_status = ex10_iface.ops->measure_aux_adc(AdcResultPowerLoSum, 1u, &adc);
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    if (adc > 200)
    {
        printf("ERROR: Is power already ramped?\n");
        return -1;
    }

    // Set a low EventFifo threshold, just for ramping events.
    ex10_iface.protocol->set_event_fifo_threshold(0u);

    // Use high-level reader interface to transmit CW
    uint32_t const frequency_khz = 0u;     // Use frequency from hopping table
    uint32_t const remain_on     = false;  // Use regulatory times
    op_status                    = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    op_status = ex10_iface.reader->cw_test(
        antenna, rf_mode, transmit_power_cdbm, frequency_khz, remain_on);
    // Checking the error code more explicitly since CW test aggregates multiple
    // ops together
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    // Sanity check for a relatively high power output, indicating that the
    // ex10 device is transmitting.
    op_status = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    op_status = ex10_iface.ops->measure_aux_adc(AdcResultPowerLoSum, 1u, &adc);
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    if (adc < 500)
    {
        printf("ERROR: Power did not ramp up (%d)\n", adc);
        return -1;
    }

    // Setup a delay to wait for the ramp down
    struct Region const* region =
        get_ex10_regions_table()->get_region(region_name);
    ex10_iface.ops->start_timer_op(region->regulatory_timers.nominal * 1000);
    op_status = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    // Wait out the timer
    ex10_iface.ops->wait_timer_op();
    op_status = ex10_iface.ops->wait_op_completion();
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }


    // Wait for TxRampDown packet
    uint32_t const start_time = get_ex10_time_helpers()->time_now();
    while (true)
    {
        // Fail if TxRampDown not seen after 5 seconds
        if (get_ex10_time_helpers()->time_elapsed(start_time) > 5000)
        {
            printf("ERROR: TxRampDown not seen after 5 seconds\n");
            return -1;
        }

        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        if (packet)
        {
            if (verbose)
            {
                get_ex10_event_fifo_printer()->print_packets(packet);
            }
            bool const ramp_down = (packet->packet_type == TxRampDown);
            ex10_iface.reader->packet_remove();

            if (ramp_down)
            {
                break;
            }
        }
    }

    printf("Ending ramp test\n");

    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, region_name);

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        result = simple_ramp_example(ex10_iface);
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
