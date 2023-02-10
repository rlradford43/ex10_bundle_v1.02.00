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
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
static uint8_t const  antenna             = 1;
static uint16_t const rf_mode             = mode_148;
static uint16_t const transmit_power_cdbm = 3000;
static uint8_t const  initial_q           = 2;
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
struct WriteCommandArgs write_args = {
    .memory_bank  = User,
    .word_pointer = 0u,
    .data         = 0u,  // Modify value before use
};

struct ReadCommandArgs read_args = {
    .memory_bank  = User,
    .word_pointer = 0u,
    .word_count   = 1u,
};

/**
 * Before starting inventory, setup gen2 sequence to write a random 16-bit
 * value to user memory bank offset 0, and then read back the word at that
 * location
 */
static void setup_gen2_write_read_sequence(uint16_t data_word)
{
    /* Setup read and write commands ahead of time */
    write_args.data = data_word;

    struct Gen2CommandSpec write_cmd = {
        .command = Gen2Write,
        .args    = &write_args,
    };

    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    g2tcm->clear_local_sequence();

    struct Gen2TxCommandManagerError error =
        g2tcm->encode_and_append_command(&write_cmd, 0);

    /* First command added must have index 0 */
    assert(error.current_index == 0u);

    auto_access_enables[error.current_index] = true;
    access_cmds[error.current_index]         = write_cmd;

    struct Gen2CommandSpec read_cmd = {
        .command = Gen2Read,
        .args    = &read_args,
    };

    error = g2tcm->encode_and_append_command(&read_cmd, 0);
    auto_access_enables[error.current_index] = true;
    access_cmds[error.current_index]         = read_cmd;

    /* Second command added must have index 1 */
    assert(error.current_index == 1u);

    /* Enable the two access commands as auto access to be sent for every
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
static int run_access_read_write_seq(void)
{
    struct Ex10Reader const* reader      = get_ex10_reader();
    uint16_t                 write_value = rand() % UINT16_MAX;
    setup_gen2_write_read_sequence(write_value);

    // Recording the start time
    uint32_t const start_time_ms = get_ex10_time_helpers()->time_now();

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

        packet = reader->packet_peek();
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
            }
            else if (packet->packet_type == ContinuousInventorySummary)
            {
                inventory_done = true;
            }
            ex10_iface.reader->packet_remove();
            packet = reader->packet_peek();
        }
    }

    /* Last gen2 reply has result of read command */
    if (reply_words[0] == write_value)
    {
        printf("Response 0x%04x from Read command matched what was written\n",
               reply_words[0]);
    }
    else
    {
        printf("Expected: 0x%04x, read: 0x%04x\n", write_value, reply_words[0]);

        printf("Gen2Reply for Read:\n");
        printf("  Reply command: %d\n", reply.reply);
        printf("  TagErrorCode: %d\n", reply.error_code);
        printf("  data[0]: 0x%04x\n", reply.data[0]);
    }
    assert(reply_words[0] == write_value);
    /* make sure we did two transactions */
    assert(total_gen2_transactions == 2);
    /* make sure we only sigulated one tag */
    assert(total_singulations == 1);

    printf("Ending write+read sequence example\n");

    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    ex10_iface = ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        result = run_access_read_write_seq();
        if (result == -1)
        {
            printf("Read/write access issue.\n");
            ex10_iface.reader->stop_transmitting();
        }
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);
    return result;
}
