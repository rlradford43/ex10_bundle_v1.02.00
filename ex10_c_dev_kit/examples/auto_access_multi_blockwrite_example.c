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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
static uint8_t const  antenna             = 1;
static uint16_t const rf_mode             = mode_103;   // was 223 use fastest mode available in all parts including E310
static uint16_t const transmit_power_cdbm = 3000;
static uint8_t const  initial_q           = 0;          //use Q=0 so we read single tag as fast as possible
static bool const     dual_target         = false;
static uint8_t const  session             = 0;
static bool const     tag_focus_enable    = false;
static bool const     fast_id_enable      = false;

/* Global state */
struct Ex10Interfaces  ex10_iface                     = {0u};
struct InfoFromPackets packet_info                    = {0u, 0u, 0u, 0u, {0u}};
struct Gen2CommandSpec access_cmds[MaxTxCommandCount] = {0u};
bool                   auto_access_enables[MaxTxCommandCount] = {0u};

/* Gen2 parameters for this example */

// EPC data to be block written
uint8_t blockwrite_payload_1[4] = {0xA5, 0xA5, 0x01, 0x23};
uint8_t blockwrite_payload_2[4] = {0x5A, 0x5A, 0xAB, 0xCD};
uint8_t blockwrite_payload_3[4] = {0xAB, 0xCD, 0xEF, 0x01};
const uint16_t blockwrite_length = 96/3;
struct BitSpan blockwrite_data_1 = {.data = blockwrite_payload_1, .length = blockwrite_length};
struct BitSpan blockwrite_data_2 = {.data = blockwrite_payload_2, .length = blockwrite_length};
struct BitSpan blockwrite_data_3 = {.data = blockwrite_payload_3, .length = blockwrite_length};


// Block write command arguments
struct BlockWriteCommandArgs blockwrite_args_1 = {
    .memory_bank  = EPC,
    .word_pointer = 2u,                 // EPC starts at word 2
    .word_count   = 2u,                 // 2 words of EPC
    .data         = &blockwrite_data_1,   // pointer to block write data
};

struct BlockWriteCommandArgs blockwrite_args_2 = {
    .memory_bank  = EPC,
    .word_pointer = 4u,                 // EPC starts at word 2
    .word_count   = 2u,                 // 2 words of EPC
    .data         = &blockwrite_data_2,   // pointer to block write data
};

struct BlockWriteCommandArgs blockwrite_args_3 = {
    .memory_bank  = EPC,
    .word_pointer = 6u,                 // EPC starts at word 2
    .word_count   = 2u,                 // 2 words of EPC
    .data         = &blockwrite_data_3,   // pointer to block write data
};

/**
 * Before starting inventory, setup gen2 sequence to blockwrite the entire EPC
 */
static void setup_gen2_blockwrite_sequence(void)
{
    /* Setup blockwrite command ahead of time */

    struct Gen2CommandSpec blockwrite_cmd_1 = {
        .command = Gen2BlockWrite,
        .args    = &blockwrite_args_1,
    };

    struct Gen2CommandSpec blockwrite_cmd_2 = {
        .command = Gen2BlockWrite,
        .args    = &blockwrite_args_2,
    };

    struct Gen2CommandSpec blockwrite_cmd_3 = {
        .command = Gen2BlockWrite,
        .args    = &blockwrite_args_3,
    };

    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    g2tcm->clear_local_sequence();

    struct Gen2TxCommandManagerError error =
        g2tcm->encode_and_append_command(&blockwrite_cmd_1, 0);

        //printf("Blockwrite_cmd_1.word_pointer: %s", &blockwrite_args_1.data);

    /* First command added must have index 0 */
    assert(error.current_index == 0u);

    auto_access_enables[error.current_index] = true;
    access_cmds[error.current_index]         = blockwrite_cmd_1;

    error =
        g2tcm->encode_and_append_command(&blockwrite_cmd_2, 0);

    /* First command added must have index 0 */
    assert(error.current_index == 1u);

    auto_access_enables[error.current_index] = true;
    access_cmds[error.current_index]         = blockwrite_cmd_2;
    
    error =
        g2tcm->encode_and_append_command(&blockwrite_cmd_3, 0);

    /* First command added must have index 0 */
    assert(error.current_index == 2u);

    auto_access_enables[error.current_index] = true;
    access_cmds[error.current_index]         = blockwrite_cmd_3;

    /* Enable the access command as auto access to be sent for every
     * singulated tag if auto-access is enabled in the inventory control
     * register.
     */
    g2tcm->write_sequence();
    g2tcm->write_auto_access_enables(auto_access_enables, MaxTxCommandCount);
}

static int run_inventory_round(enum SelectType const         select_type,
                               struct Gen2CommandSpec const* select_config)
{
    struct InventoryRoundControlFields inventory_config = {
        .initial_q            = initial_q,
        .max_q                = initial_q,
        .min_q                = initial_q,
        .num_min_q_cycles     = 1,
        .fixed_q_mode         = true,
        .q_increase_use_query = false,
        .q_decrease_use_query = false,
        .session              = session,
        .select               = select_type,
        .target               = 0,
        .halt_on_all_tags     = false,
        .tag_focus_enable     = tag_focus_enable,
        .fast_id_enable       = fast_id_enable,
        .auto_access          = true,
        .abort_on_fail        = false,
        .halt_on_fail         = false,
        .rfu                  = 0,
    };

    struct InventoryRoundControl_2Fields const inventory_config_2 = {
        .max_queries_since_valid_epc = 0};

    // stop after one round
    struct StopConditions const stop_cond = {.max_duration_us      = 500000,
                                             .max_number_of_rounds = 1,
                                             .max_number_of_tags   = 0};

    // kick off the inventory round
    struct OpCompletionStatus op_status;
    op_status = ex10_iface.reader->continuous_inventory(antenna,
                                                        rf_mode,
                                                        transmit_power_cdbm,
                                                        &inventory_config,
                                                        &inventory_config_2,
                                                        select_config,
                                                        &stop_cond,
                                                        dual_target,
                                                        0u,
                                                        false);
    if (op_status.error_occurred)
    {
        printf("\n\nContinuous Inventory failure\n");
        printf("Op Status Op Id : %d\n", op_status.ops_status.op_id);
        printf("Op Status error %d\n", op_status.ops_status.error);
        printf("command_error: %d\n", op_status.command_error);
        printf("timeout_error: %d\n", op_status.timeout_error);
        printf("\n\n");
    }
    if (!ex10_iface.helpers->check_ops_status_errors(op_status))
    {
        return -1;
    }

    return op_status.error_occurred;
}

/**
 * Return pointer to next enabled Gen2 Access Command
 * NOTE: Assumes at least one valid access command at index 0
 */
static struct Gen2CommandSpec const* next_cmd(void)
{
    static size_t                 cmd_index = 0u;
    struct Gen2CommandSpec const* next      = &access_cmds[0u];

    for (size_t iter = cmd_index; iter < 10u; iter++)
    {
        if (auto_access_enables[iter])
        {
            next      = &access_cmds[iter];
            cmd_index = iter + 1u;
            break;
        }
    }
    return next;
}

/**
 * Run inventory round, execute auto access sequence
 * then parse through the event fifo data and make sure
 * we get what we expect.
 */
static int run_access_blockwrite_seq(void)
{
    setup_gen2_blockwrite_sequence();

    // Recording the start time
    uint32_t const start_time_ms = get_ex10_time_helpers()->time_now();
    printf("Start time of run access blockwrite %d \n",start_time_ms);

    ex10_iface.helpers->discard_packets(false, true, false);

    if (run_inventory_round(SelectAll, NULL))
    {
        printf("Run Inventory Round returned an error");
        return -1;
    }
    // this should all be over in less than a second
    uint32_t const                failsafe_timeout_ms = 1000u;
    struct EventFifoPacket const* packet              = NULL;

    int              total_singulations      = 0;
    int              total_gen2_transactions = 0;
    uint16_t         reply_words[10u]        = {0};
    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};

    bool inventory_done = false;
    while (inventory_done == false)
    {
        if ((failsafe_timeout_ms) && (get_ex10_time_helpers()->time_elapsed(
                                          start_time_ms) > failsafe_timeout_ms))
        {
            printf("Timed out before Inventory summary packets received\n");
            return -1;
        }
    
        packet = ex10_iface.reader->packet_peek();
        //packet = reader->packet_peek();
        if (packet != NULL)
        {
            if (packet->packet_type == TagRead)
            {
                total_singulations++;
                // don't care about the tag read data just how
                // many were singulated
            }
            else if (packet->packet_type == Gen2Transaction)
            {
                total_gen2_transactions++;
                struct Gen2CommandSpec const* comm_spec = next_cmd();
                // if the command is a read we grab the data so
                // it can be verified below
                if (comm_spec->command == Gen2Read)
                {
                    ex10_iface.gen2_commands->decode_reply(
                        comm_spec->command, packet, &reply);
                    assert(ex10_iface.helpers->check_gen2_error(&reply) ==
                           false);
                }
                else if (comm_spec->command == Gen2BlockWrite)
                {
                    ex10_iface.gen2_commands->decode_reply(
                        comm_spec->command, packet, &reply);
                    assert(ex10_iface.helpers->check_gen2_error(&reply) ==
                           false);
                }
            }
            else if (packet->packet_type == ContinuousInventorySummary)
            {
                inventory_done = true;
            }
            ex10_iface.reader->packet_remove();
            packet = ex10_iface.reader->packet_peek();
            //packet = reader->packet_peek();
        }
    }

    /* make sure we did three transactions */
    printf("total_gen2_transactions = %d \n", total_gen2_transactions);
    assert(total_gen2_transactions == 3);
    /* make sure we only sigulated one tag */
    assert(total_singulations == 1);

    printf("Exiting multi-blockwrite call\n");
   
    return 0;
}

int main(void)
{
    printf("Blockwritedata_1 0x%2X%2X%2X%2X \n", blockwrite_payload_1[0],
            blockwrite_payload_1[1],
            blockwrite_payload_1[2],
            blockwrite_payload_1[3]);
            printf("Blockwritedata_1 Part 0x%2X \n", blockwrite_payload_1[2]);
            printf("Blockwritedata_3 Part 0x%2X \n", blockwrite_payload_3[3]);
    
    uint32_t const start_time_bw = get_ex10_time_helpers()->time_now();
    
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    ex10_iface = ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");  // was "FCC_915750"
    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        //uint32_t const start_time_bw = get_ex10_time_helpers()->time_now();
        printf("Call to blockwrite at time %d uS\n", start_time_bw);
        // call to actually do the blockwrites
        result = run_access_blockwrite_seq();
        uint32_t const start_time_bwe = get_ex10_time_helpers()->time_now();


        printf("Return from Blockwrite at time %d uS\n", start_time_bwe);
        printf("Time to run blockwrite %d uS\n",(start_time_bwe-start_time_bw));
        
        if (result == -1)
        {
            printf("Read/write access issue.\n");
            ex10_iface.reader->stop_transmitting();
        }
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);
    printf("Exiting multi-blockwrite program \n");
    return result;
}
