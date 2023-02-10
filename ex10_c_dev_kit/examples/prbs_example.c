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
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
const uint32_t prbs_time_on        = 8 * 1000;  // Duration in milliseconds
const uint8_t  antenna             = 1;
const uint16_t rf_mode             = mode_148;
const uint16_t transmit_power_cdbm = 3000;

int prbs_example(struct Ex10Interfaces ex10_iface)
{
    printf("Starting PRBS test\n");

    bool     transmitting = false;
    uint32_t start_time   = get_ex10_time_helpers()->time_now();
    while (get_ex10_time_helpers()->time_elapsed(start_time) < prbs_time_on)
    {
        if (!transmitting)
        {
            uint32_t const frequency_khz = 0u;  // frequency from hopping table
            uint32_t const remain_on     = false;  // Use regulatory times
            struct OpCompletionStatus op_status =
                ex10_iface.reader->prbs_test(antenna,
                                             rf_mode,
                                             transmit_power_cdbm,
                                             frequency_khz,
                                             remain_on);
            if (!ex10_iface.helpers->check_ops_status_errors(op_status))
            {
                return -1;
            }
            transmitting = true;
        }

        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        if (packet)
        {
            if (packet->packet_type == TxRampDown)
            {
                transmitting = false;
            }
            ex10_iface.reader->packet_remove();
        }
    }

    ex10_iface.reader->stop_transmitting();
    printf("Ending PRBS test\n");
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
        result = prbs_example(ex10_iface);
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
