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
#include <string.h>

#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
static uint32_t const inventory_duration_ms = 500;  // Duration in milliseconds
static uint8_t const  antenna               = 1;
static uint16_t const rf_mode               = mode_148;
static uint16_t const transmit_power_cdbm   = 3000;
static uint8_t const  initial_q             = 4;
static bool const     dual_target           = true;
static uint8_t const  session               = 0;
static bool const     halt_on_all_tags      = false;
static bool const     tag_focus_enable      = false;
static bool const     fast_id_enable        = false;


/* Global state */
static struct Ex10Interfaces  ex10_iface  = {0u};
static struct InfoFromPackets packet_info = {0u, 0u, 0u, 0u, {0u}};

/* Gen2 parameters for this example */
/* Note for the mask: Everything is loaded in left-justified.
 * This means the bytes are loaded in via lsb, but the bits
 * are loaded in left to right as well, making them bitwise msb.
 * This was done for reader clarity to make it look like a
 * bitstream
 * EX:
 * load_mask = 0xa2f5
 * load_bits = 12
 * bits_loaded = 0xa2f
 *
 * load_mask = 0x10
 * load_bits = 4
 * bits_loaded = 0b0001
 */
static uint8_t        select_mask_buffer[2u] = {0u};
static struct BitSpan select_mask            = {select_mask_buffer, 12u};

static struct SelectCommandArgs select_args_001 = {
    .target      = SelectedFlag,
    .action      = Action001,
    .memory_bank = SelectEPC,
    .bit_pointer = 0,
    .bit_count   = 12,
    .mask        = &select_mask,
    .truncate    = false,
};

static struct SelectCommandArgs select_args_101 = {
    .target      = SelectedFlag,
    .action      = Action101,
    .memory_bank = SelectEPC,
    .bit_pointer = 0,
    .bit_count   = 12,
    .mask        = &select_mask,
    .truncate    = false,
};

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
        .halt_on_all_tags     = halt_on_all_tags,
        .tag_focus_enable     = tag_focus_enable,
        .fast_id_enable       = fast_id_enable,
    };

    struct InventoryRoundControl_2Fields inventory_config_2 = {
        .max_queries_since_valid_epc = 0};

    struct InventoryHelperParams ihp = {
        .antenna               = antenna,
        .rf_mode               = rf_mode,
        .tx_power_cdbm         = transmit_power_cdbm,
        .inventory_config      = &inventory_config,
        .inventory_config_2    = &inventory_config_2,
        .send_selects          = (select_type == SelectAll) ? false : true,
        .remain_on             = false,
        .dual_target           = dual_target,
        .inventory_duration_ms = inventory_duration_ms,
        .packet_info           = &packet_info,
        .verbose               = true,
    };

    if (ex10_iface.helpers->simple_inventory(&ihp))
    {
        return -1;
    };

    return 0;
}

static int select_command_example(void)
{
    // NOTE: This example assumes a single tag is used
    printf("Starting Select command example\n");

    printf("\nInventory with Sel=ALL, no Select command\n");
    run_inventory_rounds(SelectAll);
    printf("Done\n");

    uint16_t crc_of_interest = 0;
    if (packet_info.total_singulations)
    {
        crc_of_interest = packet_info.access_tag.stored_crc;
        printf("The Select mask will be 0x%02x - the CRC of tag with EPC=0x",
               ex10_iface.helpers->swap_bytes(crc_of_interest));
        for (size_t idx = 0u; idx < packet_info.access_tag.epc_length; idx++)
        {
            printf("%02x", packet_info.access_tag.epc[idx]);
        }
    }
    else
    {
        fprintf(stderr, "No tags found in inventory\n");
        return -1;
    }

    // Change the mask based on the tag recieved
    select_mask.data[0u] = (uint8_t)(crc_of_interest);
    select_mask.data[1u] = (uint8_t)(crc_of_interest >> 8u);
    select_args_001.mask = &select_mask;
    select_args_101.mask = &select_mask;

    // Create select commands
    struct Gen2CommandSpec select_cmd = {
        .command = Gen2Select,
        .args    = &select_args_001,
    };
    struct Gen2CommandSpec select_cmd2 = {
        .command = Gen2Select,
        .args    = &select_args_101,
    };

    // Add our new selects
    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();

    struct Gen2TxCommandManagerError error_1 =
        g2tcm->encode_and_append_command(&select_cmd, 0);
    assert(false == error_1.error_occurred);
    struct Gen2TxCommandManagerError error_2 =
        g2tcm->encode_and_append_command(&select_cmd2, 1);
    assert(false == error_2.error_occurred);
    // set the buffer to the device
    struct Gen2TxCommandManagerError error_3 = g2tcm->write_sequence();
    assert(false == error_3.error_occurred);

    // enable the first select command
    bool select_enables[MaxTxCommandCount] = {0u};
    select_enables[error_1.current_index]  = true;
    g2tcm->write_select_enables(select_enables, MaxTxCommandCount);

    printf("\nInventory with Sel=SL, sending Select with Action001\n");
    run_inventory_rounds(SelectAsserted);

    if (packet_info.total_singulations == 0u)
    {
        fprintf(stderr, "Selected tag did not reply\n");
        return -1;
    }
    else
    {
        printf("Done\n");
    }

    // enable the second select
    select_enables[error_1.current_index] = false;
    select_enables[error_2.current_index] = true;
    g2tcm->write_select_enables(select_enables, MaxTxCommandCount);

    printf("\nInventory with Sel=~SL, sending Select with Action101\n");
    run_inventory_rounds(SelectNotAsserted);

    if (packet_info.total_singulations == 0u)
    {
        fprintf(stderr, "Selected tag did not reply\n");
        return -1;
    }
    else
    {
        printf("Done\n");
    }

    printf("\nInventory with Sel=SL, sending Select with Action101\n");
    run_inventory_rounds(SelectAsserted);

    if (packet_info.total_singulations > 0u &&
        crc_of_interest == packet_info.access_tag.stored_crc)
    {
        fprintf(stderr, "Tag replied when was not selected\n");
        return -1;
    }
    else
    {
        printf("Done\n");
    }

    printf("Ending Select command example\n");

    return 0;
}

int main(void)
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    ex10_iface = ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        result = select_command_example();
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
