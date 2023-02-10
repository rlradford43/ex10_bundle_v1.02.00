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

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"
#include "ex10_api/application_register_definitions.h"

#ifdef randy
//**************************************************************
// Add SDD logging code here as a test. 
// What determines the file name
// enable power control loop logs​
void enable_sdd(const struct Ex10Interfaces ex10_iface)
{
    struct LogEnablesFields log_enables = {0};
    log_enables.op_logs                 = true;
    log_enables.ramping_logs            = true;
    log_enables.lmac_logs               = true;
    log_enables.insert_fifo_event_logs  = true;
    log_enables.host_irq_logs           = true;
    log_enables.regulatory_logs         = true;
    const uint8_t log_speed_mhz         = 12;
    ex10_iface.reader->enable_sdd_logs(log_enables, log_speed_mhz);
}

//disable undesired fields​
//    memset(log_enables_reg_value, 0, sizeof (log_enables_reg_value));​
    
//enable desired field(s)​
    log_enables.ramping_logs = true;​

//set clock frequency​
    uint8_t log_sdd_clk_MHz = 4;​

​//configure fields and clock frequency​
    ex10_iface.ops->enable_sdd_logs(log_enables_reg_value, log_sdd_clk_MHz);

//**************************************************************
#endif 

/* Settings used when running this example */
static uint32_t const inventory_duration_ms = 500;  // Duration in milliseconds
static uint8_t const  antenna               = 1;
static uint16_t const rf_mode               = mode_148;
static uint16_t const transmit_power_cdbm   = 3000;
static uint8_t const  initial_q             = 2;
static bool const     dual_target           = true;
static uint8_t const  session               = 0;
static bool const     tag_focus_enable      = false;
static bool const     fast_id_enable        = false;


/* Global state */
struct Ex10Interfaces  ex10_iface                     = {0u};
struct InfoFromPackets packet_info                    = {0u, 0u, 0u, 0u, {0u}};
struct Gen2CommandSpec access_cmds[MaxTxCommandCount] = {0u};
bool                   halted_enables[MaxTxCommandCount] = {0u};

/* Gen2 parameters for this example */
struct WriteCommandArgs write_args = {
    .memory_bank  = User,
    .word_pointer = 0u,
    .data         = 1234u,  // Modify value before use
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

    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    g2tcm->clear_local_sequence();

    write_args.data                  = data_word;
    struct Gen2CommandSpec write_cmd = {
        .command = Gen2Write,
        .args    = &write_args,
    };

    struct Gen2TxCommandManagerError error =
        g2tcm->encode_and_append_command(&write_cmd, 0);
    assert(false == error.error_occurred);
    assert(error.current_index == 0u);
    halted_enables[error.current_index] = true;
    access_cmds[error.current_index]    = write_cmd;

    struct Gen2CommandSpec read_cmd = {
        .command = Gen2Read,
        .args    = &read_args,
    };

    error = g2tcm->encode_and_append_command(&read_cmd, 1);
    assert(false == error.error_occurred);
    assert(error.current_index == 1u);
    halted_enables[error.current_index] = true;
    access_cmds[error.current_index]    = read_cmd;

    g2tcm->write_sequence();

    g2tcm->write_halted_enables(halted_enables, MaxTxCommandCount);
}

static int run_inventory_rounds(enum SelectType const select_type)
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
        .halt_on_all_tags     = true,
        .tag_focus_enable     = tag_focus_enable,
        .fast_id_enable       = fast_id_enable,
    };

    struct InventoryRoundControl_2Fields const inventory_config_2 = {
        .max_queries_since_valid_epc = 0};

    struct InventoryHelperParams ihp = {
        .antenna               = antenna,
        .rf_mode               = rf_mode,
        .tx_power_cdbm         = transmit_power_cdbm,
        .inventory_config      = &inventory_config,
        .inventory_config_2    = &inventory_config_2,
        .send_selects          = false,
        .remain_on             = false,
        .dual_target           = dual_target,
        .inventory_duration_ms = inventory_duration_ms,
        .packet_info           = &packet_info,
        .verbose               = true};

    if (ex10_iface.helpers->simple_inventory(&ihp))
    {
        return -1;
    };

    if (!packet_info.total_singulations)
    {
        printf("No tags found in inventory\n");
        return -1;
    }

    return 0;
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
        if (halted_enables[iter])
        {
            next      = &access_cmds[iter];
            cmd_index = iter + 1u;
            break;
        }
    }
    return next;
}

/**
 * Run inventory round, halt on first tag, execute gen2 sequence
 */
static int run_access_read_write_seq(void)
{
    write_args.data = rand() % UINT16_MAX;
    printf("Number to write %X\n",write_args.data);
    setup_gen2_write_read_sequence(write_args.data);

    if (run_inventory_rounds(SelectAll) == -1)
    {
        ex10_iface.reader->stop_transmitting();
        return -1;
    }

    /* Should be halted on a tag now */
    bool halted = ex10_iface.helpers->inventory_halted();
    assert(halted && "Failed to halt on a tag");

    printf("Sending write and read commands\n");

    /* Trigger stored Gen2 sequence */
    ex10_iface.ops->send_gen2_halted_sequence();

    /* Wait for Gen2Transaction packets to be returned */
    uint32_t const   timeout          = 1000;
    uint16_t         reply_words[10u] = {0};
    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};
    
    size_t gen2_packet_count_expected = 2u;

    uint32_t const start_time = get_ex10_time_helpers()->time_now();
    while (get_ex10_time_helpers()->time_elapsed(start_time) < timeout &&
           gen2_packet_count_expected)
    {
        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        while (packet != NULL)
        {
            get_ex10_event_fifo_printer()->print_packets(packet);
            if (packet->packet_type == Gen2Transaction)
            {
                gen2_packet_count_expected--;
                reply.error_code = NoError;
                memset(reply_words, 0x00, sizeof(reply_words));

                ex10_iface.gen2_commands->decode_reply(
                    next_cmd()->command, packet, &reply);
                assert(ex10_iface.helpers->check_gen2_error(&reply) == false);
            }
            ex10_iface.reader->packet_remove();
            packet = ex10_iface.reader->packet_peek();
        }
    }

    assert((gen2_packet_count_expected == 0u) &&
           "Did not receive expected number of Gen2Transaction Packets");

    /* Last gen2 reply has result of read command */
    if (reply_words[0] == write_args.data)
    {
        printf("Response 0x%04X from Read command matched what was written\n",
               reply_words[0]);
    }
    else
    {
        printf("Expected: 0x%04x, read: 0x%04x\n", write_args.data, reply_words[0]);

        printf("Gen2Reply for Read:\n");
        printf("  Reply command: %d\n", reply.reply);
        printf("  TagErrorCode: %d\n", reply.error_code);
        printf("  data[0]: 0x%04x\n", reply.data[0]);
    }
    assert(reply_words[0] == write_args.data);

    /* Demonstrate continuing to next tag, not used here. */
    ex10_iface.reader->continue_from_halted(false);

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
