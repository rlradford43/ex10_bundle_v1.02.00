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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board/board_spec.h"
#include "ex10_api/board_init.h"
#include "ex10_api/command_transactor.h"
#include "ex10_api/ex10_protocol.h"

/**
 * Execute repeated varied length data transfers via the ex10 host
 * interface using the TransferTest command.
 *
 * The number of iterations is a single argument on the command line.
 * Use zero to run forever.
 *
 * @note
 * One limitation is that the 'time_helper' routines assert when the call to
 * clock() rolls over. When this occurs, you'll see:
 *   ...
 *   Test transfer 17064366
 *   Test transfer 17064367
 *   exercise_transfers.bin: time_helpers.c:20: time_start:
 *   Assertion `time <= * 0xFFFFFFFF' failed. Aborted (core dumped)
 */

static const bool verbose = true;

int main(int argc, char* argv[])
{
    struct Ex10CommandsHostErrors response_code = {
        .error_occurred  = false,
        .host_result     = HostCommandsSuccess,
        .device_response = Success,
    };

    uint8_t tx_data[EX10_SPI_BURST_SIZE];
    uint8_t rx_data[EX10_SPI_BURST_SIZE];

    struct ConstByteSpan tx = {
        .data   = tx_data,
        .length = EX10_SPI_BURST_SIZE,
    };

    struct ByteSpan rx = {
        .data   = rx_data,
        .length = EX10_SPI_BURST_SIZE,
    };

    uint32_t iterations = 1000u;

    if (argc < 2)
    {
        printf("using default iteration count: %u\n", iterations);
    }
    else
    {
        iterations = atoi(argv[1u]);
    }

    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");
    ex10_iface.helpers->check_board_init_status(Application);

    struct Ex10Protocol const* ex10_protocol = ex10_iface.protocol;

    tx.length = 0u;
    for (size_t iteration_number = 0u;
         (iterations == 0u) || (iteration_number < iterations);
         ++iteration_number)
    {
        memset(tx_data, iteration_number & UINT8_MAX, tx.length);
        rx.length = tx.length;

        if (verbose)
        {
            printf("Test transfer %zu (size %zu)\n",
                   iteration_number++,
                   tx.length);
        }
        bool const verify = true;
        response_code     = ex10_protocol->test_transfer(&tx, &rx, verify);
        if (response_code.error_occurred)
        {
            printf("Response code from device: %u\n",
                   response_code.device_response);
            printf("Response code from commands layer: %u\n",
                   response_code.host_result);
            break;
        }

        tx.length += 1u;
        if (tx.length >= EX10_SPI_BURST_SIZE)
        {
            // Note the wrap-around comparison is 1 less than the
            // EX10_SPI_BURST_SIZE. This allows room for the TransferTest
            // command byte to be inserted.
            tx.length = 0u;
        }
    }

    if (response_code.error_occurred == false)
    {
        printf("Pass\n");
    }
    else
    {
        printf("Fail\n");
    }

    ex10_typical_board_teardown();
    return response_code.error_occurred;
}
