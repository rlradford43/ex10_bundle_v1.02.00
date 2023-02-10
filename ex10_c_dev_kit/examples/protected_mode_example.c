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

// Note: This example assumes a single tag on antenna port one.
// Note: This example assumes a starting password of all 0s

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"


/* Settings used when running this example */
static const uint32_t inventory_duration_ms = 5000;  // Duration in milliseconds
static const uint8_t  antenna               = 1;
static const uint16_t rf_mode               = mode_148;
static const uint16_t transmit_power_cdbm   = 2500;
static const uint8_t  initial_q             = 4;
static const uint8_t  session               = 0;

static struct InfoFromPackets packet_info = {0u, 0u, 0u, 0u, {0u}};

uint32_t zero_access_pwd     = ZERO_ACCESS_PWD;
uint32_t non_zero_access_pwd = NON_ZERO_ACCESS_PWD;
// Allows for testing the short range functionality of Impinj tags.
// This is not enabled for general testing, but if enabled, the power
// should be increased.
static bool test_sr_bit = false;

enum PROTECTED_MODE_ERROR
{
    PROTECTED_NO_ERROR = 0,
    PROTECTED_HALT_ERROR,
    PROTECTED_ENTER_ACCESS_ERROR,
    PROTECTED_CHANGE_PWD_ERROR,
    PROTECTED_SET_MODE_ERROR,
};

// Used to return information from a tag read specific to this example
struct ProtectedExampleInfo
{
    bool     protected_mode_enabled;
    bool     short_range_enabled;
    uint16_t page_info;
};

struct TagFoundInfo
{
    uint8_t epc_array[20];
    size_t  epc_length;
    bool    first_tag_found;
};

struct TagFoundInfo tag_info = {{0}, 0, false};

/**
 * Send Gen2 command, wait for reply, and decode
 */
static int send_gen2_command_wait(struct Ex10Interfaces   ex10_iface,
                                  struct Gen2CommandSpec* cmd_spec,
                                  struct Gen2Reply*       reply)
{
    const uint32_t                timeout = 1000;
    struct EventFifoPacket const* packet  = NULL;

    // Ensure we are not still in the previous transaction
    struct HaltedStatusFields halt_fields = {.halted = true, .busy = true};

    // Ensure any previous command is done first
    uint32_t start_time = get_ex10_time_helpers()->time_now();
    while (halt_fields.busy == true &&
           get_ex10_time_helpers()->time_elapsed(start_time) < timeout)
    {
        ex10_iface.protocol->read(&halted_status_reg, &halt_fields);
    }
    // Ensure we are still halted and not busy any more
    if (halt_fields.halted == false || halt_fields.busy == true)
    {
        return -1;
    }

    packet_info.gen2_transactions = 0;

    // Overwrite the buffer and send a single command
    struct Gen2TxCommandManagerError curr_error =
        ex10_iface.helpers->send_single_halted_command(cmd_spec);
    assert(false == curr_error.error_occurred);

    start_time = get_ex10_time_helpers()->time_now();
    while (get_ex10_time_helpers()->time_elapsed(start_time) < timeout &&
           !packet_info.gen2_transactions)
    {
        packet = ex10_iface.reader->packet_peek();
        while (packet != NULL)
        {
            ex10_iface.helpers->examine_packets(packet, &packet_info);
            if (packet->packet_type == Gen2Transaction)
            {
                ex10_iface.gen2_commands->decode_reply(
                    cmd_spec->command, packet, reply);
            }
            assert(packet->packet_type != InventoryRoundSummary &&
                   "Inventory ended while waiting for Gen2 transaction");
            ex10_iface.reader->packet_remove();
            packet = ex10_iface.reader->packet_peek();
        }
    }

    if (reply->error_code != NoError || !packet_info.gen2_transactions)
    {
        return -1;
    }
    return 0;
}

static int inventory_and_halt(struct Ex10Interfaces   ex10_iface,
                              struct Gen2CommandSpec* select_config,
                              bool                    expecting_tag)
{
    struct InventoryRoundControlFields inventory_config = {
        .initial_q        = initial_q,
        .session          = session,
        .select           = SelectAll,
        .target           = 0,
        .halt_on_all_tags = true,
        .tag_focus_enable = false,
        .fast_id_enable   = false,
    };

    struct InventoryRoundControl_2Fields inventory_config_2 = {
        .max_queries_since_valid_epc = 0};

    bool     round_done = true;
    uint32_t start_time = get_ex10_time_helpers()->time_now();

    // Clear the number of tags found so that if we halt, we can return
    packet_info.total_singulations = 0u;
    packet_info.gen2_transactions  = 0u;
    packet_info.total_tid_count    = 0u;

    ex10_iface.helpers->discard_packets(false, true, false);

    // Add the select command of interest
    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    bool select_enables[MaxTxCommandCount] = {0u};

    if (select_config)
    {
        struct Gen2TxCommandManagerError curr_error =
            g2tcm->encode_and_append_command(select_config, 0);
        assert(false == curr_error.error_occurred);
        select_enables[curr_error.current_index] = true;
        curr_error                               = g2tcm->write_sequence();
        assert(false == curr_error.error_occurred);
        g2tcm->write_select_enables(select_enables, MaxTxCommandCount);
    }

    while (packet_info.total_singulations == 0)
    {
        if (get_ex10_time_helpers()->time_elapsed(start_time) >
            inventory_duration_ms)
        {
            break;
        }
        if (round_done)
        {
            round_done = false;
            struct OpCompletionStatus op_error =
                ex10_iface.reader->inventory(antenna,
                                             rf_mode,
                                             transmit_power_cdbm,
                                             &inventory_config,
                                             &inventory_config_2,
                                             (select_config) ? true : false,
                                             0u,
                                             true);
            if (!ex10_iface.helpers->check_ops_status_errors(op_error))
            {
                return -1;
            }
        }

        struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
        while (packet)
        {
            ex10_iface.helpers->examine_packets(packet, &packet_info);
            if (packet->packet_type == InventoryRoundSummary)
            {
                round_done = true;
            }
            else if (packet->packet_type == TagRead)
            {
                printf("Tag found with epc: ");
                for (size_t i = 0; i < packet_info.access_tag.epc_length; i++)
                {
                    printf("%d, ", packet_info.access_tag.epc[i]);
                }
                printf("\n");
                if (tag_info.first_tag_found == 0)
                {
                    // Save the epc of the first tag to ensure we only use one
                    // tag for this test
                    memcpy(tag_info.epc_array,
                           packet_info.access_tag.epc,
                           packet_info.access_tag.epc_length);
                    tag_info.first_tag_found = true;
                }
                else
                {
                    // Check if the found tag matches the one previously found
                    if (0 != memcpy(tag_info.epc_array,
                                    packet_info.access_tag.epc,
                                    packet_info.access_tag.epc_length))
                    {
                        // If this is a new tag, it does not count for the test
                        packet_info.total_singulations--;
                    }
                }
                ex10_iface.helpers->examine_packets(packet, &packet_info);
            }
            ex10_iface.reader->packet_remove();
            packet = ex10_iface.reader->packet_peek();
        }
    }

    if (expecting_tag)
    {
        // expecting tag - return -1 if no tag found
        if (!packet_info.total_singulations)
        {
            printf("No tag found when expected 1 = %d, packetinfo %d\n",expecting_tag,packet_info.total_singulations);
            return -1;
        }
        return 0;
    }
    else
    {
        // not expecting tag - return -1 if tag found
        if (packet_info.total_singulations)
        {
            printf("Tag found when not expected 2 = %d packetinfo %d\n",expecting_tag, packet_info.total_singulations);
            return -1;
        }
        return 0;
    }
}

static int write_to_reserved(struct Ex10Interfaces ex10_iface,
                             uint16_t              page_to_write,
                             uint16_t              page_data)
{
    uint16_t         reply_words[10u] = {0};
    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};

    struct WriteCommandArgs write_args = {
        .memory_bank  = Reserved,
        .word_pointer = page_to_write,
        .data         = page_data,
    };
    struct Gen2CommandSpec write_cmd = {
        .command = Gen2Write,
        .args    = &write_args,
    };

    if (send_gen2_command_wait(ex10_iface, &write_cmd, &reply) ||
        reply.error_code != NoError)
    {
        return -1;
    }
    return 0;
}

static void read_reserved_memory(struct Ex10Interfaces ex10_iface,
                                 uint16_t              word_pointer,
                                 uint16_t              word_count,
                                 struct Gen2Reply*     reply)
{
    struct ReadCommandArgs read_args = {
        .memory_bank  = Reserved,
        .word_pointer = word_pointer,
        .word_count   = word_count,
    };

    struct Gen2CommandSpec read_cmd = {
        .command = Gen2Read,
        .args    = &read_args,
    };

    send_gen2_command_wait(ex10_iface, &read_cmd, reply);
    if (reply->error_code != NoError ||
        reply->transaction_status != Gen2TransactionStatusOk ||
        reply->reply != Gen2Read)
    {
        printf(
            "Reserved memory read returned - reply: %d, error_code: %d, "
            "transaction_status: %d",
            reply->reply,
            reply->error_code,
            reply->transaction_status);
    }
}

static struct ProtectedExampleInfo read_settings(
    struct Ex10Interfaces ex10_iface)
{
    uint16_t         reply_words[10u] = {0};
    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};

    // Read the settings from word 4 of reserved memory
    read_reserved_memory(ex10_iface, 4, 1, &reply);
    uint16_t settings_data = reply.data[0];

    printf("Read Back: %d\n", settings_data);
    // base 0: bit 1 is P and bit 4 is SR
    bool protected   = (settings_data >> 1) & 1;
    bool short_range = (settings_data >> 4) & 1;

    struct ProtectedExampleInfo read_info = {
        .protected_mode_enabled = protected,
        .short_range_enabled    = short_range,
        .page_info              = settings_data,
    };
    return read_info;
}

static struct SelectCommandArgs create_select_args(
    struct BitSpan* protected_mode_pin)
{
    // We pass in the protected mode pin to use as
    // the mask. This allows the tag to respond
    // when in protected mode.
    struct SelectCommandArgs select_args = {0};

    select_args.target      = Session0;
    select_args.action      = Action001;
    select_args.memory_bank = SelectFile0;
    select_args.bit_pointer = 0;
    select_args.bit_count   = 32;
    select_args.mask        = protected_mode_pin;
    select_args.truncate    = false;

    return select_args;
}

static int enter_access_mode(struct Ex10Interfaces ex10_iface,
                             uint32_t              access_password)
{
    /* Should be halted on a tag already */
    uint16_t         reply_words[10u] = {0};
    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};

    // Create structs to write the assess pwd in two steps
    uint16_t msb_pwd_value = access_password & 0xFFFF;
    uint16_t lsb_pwd_value = access_password >> 16;

    struct AccessCommandArgs msb_pwd_args = {
        .password = msb_pwd_value,
    };
    struct Gen2CommandSpec msb_pwd_cmd = {
        .command = Gen2Access,
        .args    = &msb_pwd_args,
    };
    struct AccessCommandArgs lsb_pwd_args = {
        .password = lsb_pwd_value,
    };
    struct Gen2CommandSpec lsb_pwd_cmd = {
        .command = Gen2Access,
        .args    = &lsb_pwd_args,
    };

    // We can not know if the response back will contain an error
    // or not, thus we will trust CRC and the tag handle in this test.
    printf("Access command 1 sent\n");
    if (send_gen2_command_wait(ex10_iface, &msb_pwd_cmd, &reply) ||
        reply.error_code != NoError)
    {
        return -1;
    }
    // We get a handle back from this first access command.
    // We will use this to ensure that the next commands are proper responses
    // rather than noise. This is important as an incorrect password means
    // the tag will not respond at all for a given security timeout. An error
    // in the recieved handle thus likely means a bad password was given here.
    struct AccessCommandReply* access_resp =
        (struct AccessCommandReply*)reply.data;
    uint16_t proper_tag_handle = access_resp->tag_handle;
    memset(reply_words, 0x00, sizeof(reply_words));

    printf("Access command 2 sent\n");
    if (send_gen2_command_wait(ex10_iface, &lsb_pwd_cmd, &reply) ||
        reply.error_code != NoError)
    {
        return -1;
    }
    // Check response from tag for proper handle
    access_resp = (struct AccessCommandReply*)reply.data;
    if (proper_tag_handle != access_resp->tag_handle)
    {
        return -1;
    }

    return 0;
}

static int change_access_pwd(struct Ex10Interfaces ex10_iface,
                             uint32_t              ending_pwd)
{
    printf("Changing access pwd\n");
    uint16_t pwd_word_0 = ending_pwd & 0xFFFF;
    uint16_t pwd_word_1 = ending_pwd >> 16;

    // Write the new password to memory
    if (write_to_reserved(ex10_iface, 2, pwd_word_0) ||
        write_to_reserved(ex10_iface, 3, pwd_word_1))
    {
        return -1;
    }
    printf("Password changed\n");
    return 0;
}

static int set_mode_state(struct Ex10Interfaces ex10_iface, bool enable)
{
    (enable) ? printf("Entering protected mode\n")
             : printf("Leaving protected mode\n");

    // Read reserved memory for modify write
    struct ProtectedExampleInfo read_info = read_settings(ex10_iface);

    // Modify the memory page to write in
    read_info.page_info = read_info.page_info & ~(1 << 1);
    read_info.page_info = read_info.page_info & ~(1 << 4);
    // To enable protected mode
    read_info.page_info = read_info.page_info | (enable << 1);
    // To enable short range mode - this should be done
    // when the output power and tag distance is known
    read_info.page_info = (test_sr_bit) ? read_info.page_info | (enable << 4)
                                        : read_info.page_info | (0 << 4);

    // Write memory controlling protected mode and short range control
    // We change both together here though not necessary
    printf("Writing to change protected and sr bit\n");
    if (write_to_reserved(ex10_iface, 4, read_info.page_info))
    {
        return -1;
    }

    // Read back memory to ensure the data was written
    printf("Read back memory to ensure it was set properly\n");
    read_info = read_settings(ex10_iface);
    if (read_info.protected_mode_enabled != enable ||
        read_info.short_range_enabled != (enable & test_sr_bit))
    {
        return -1;
    }

    (enable) ? printf("Entered protected mode successfully\n")
             : printf("Left protected mode successfully\n");

    return 0;
}

static uint32_t run_protected_mode_example(struct Ex10Interfaces ex10_iface)
{
    // Note: This example assumes a single tag in field of view.
    // Note: This example assumes a starting password of all 0s

    // Create a select based on the password of the tag
    printf("Original PWD before shift 0x%x \n",non_zero_access_pwd);
    uint8_t select_mask_buffer[4u] = {
        (uint8_t) (non_zero_access_pwd>>8),    //Lo 16-bit -> Hi Byte
        (uint8_t) (non_zero_access_pwd>>0),    //Lo 16-bit -> Lo Byte
        (uint8_t) (non_zero_access_pwd>>24),   //Hi 16-bit -> Hi Byte
        (uint8_t) (non_zero_access_pwd>>16) };   //Hi 16-bit -> Lo Byte
    struct BitSpan non_zero_access_info = {select_mask_buffer, 32};

    printf("Modified Mask of PWD 0x%x,0x%x,0x%x,0x%x\n",
        select_mask_buffer[0],
        select_mask_buffer[1],
        select_mask_buffer[2],
        select_mask_buffer[3]);

    // was struct BitSpan non_zero_access_info = {(uint8_t*)&non_zero_access_pwd, 32};

    struct SelectCommandArgs non_zero_select_args =
        create_select_args(&non_zero_access_info);

    // The non zero select is must be sent to force a protected mode tag to
    // reveal itself.
    struct Gen2CommandSpec non_zero_select = {
        .command = Gen2Select,
        .args    = &non_zero_select_args,
    };

    ex10_iface.protocol->set_event_fifo_threshold(0);

    // clang-format off
    printf("Find the tag and change the password\n");
    if(inventory_and_halt(ex10_iface, NULL, true)) { 
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR);
        return PROTECTED_HALT_ERROR; }
    if(change_access_pwd(ex10_iface, non_zero_access_pwd)) { return PROTECTED_CHANGE_PWD_ERROR; }
    ex10_iface.reader->stop_transmitting();
    // Note the sleep after all stop_transmitting to ensure the tag state
    // settles.
    sleep(2);

    printf("Enter the password, then set protected mode\n");
    if(inventory_and_halt(ex10_iface, NULL, true)) 
        {
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR);
        return PROTECTED_HALT_ERROR; 
        }
    if(enter_access_mode(ex10_iface, non_zero_access_pwd)) { return PROTECTED_ENTER_ACCESS_ERROR; }
    if(set_mode_state(ex10_iface, true)) { return PROTECTED_SET_MODE_ERROR; }
    ex10_iface.reader->stop_transmitting();
    sleep(2);

    printf("Ensure we can not see the tag\n");
    if(inventory_and_halt(ex10_iface, NULL, false)) { return PROTECTED_HALT_ERROR; }
    ex10_iface.reader->stop_transmitting();
    sleep(2);

    printf("Show how to find it again and return it to not-protected mode\n");
    if(inventory_and_halt(ex10_iface, &non_zero_select, true)) {    
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR);
        return PROTECTED_HALT_ERROR; }
    if(enter_access_mode(ex10_iface, non_zero_access_pwd)) { return PROTECTED_ENTER_ACCESS_ERROR; }
    if(set_mode_state(ex10_iface, false)) { return PROTECTED_SET_MODE_ERROR; }
    if(change_access_pwd(ex10_iface, zero_access_pwd)) { return PROTECTED_CHANGE_PWD_ERROR; }
    ex10_iface.reader->stop_transmitting();
    sleep(2);

    printf("Show we can find the tag normally now\n");
    if(inventory_and_halt(ex10_iface, NULL, true)) {    
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR);
        return PROTECTED_HALT_ERROR; }
    ex10_iface.reader->stop_transmitting();
    sleep(2);
    // clang-format on

    return PROTECTED_NO_ERROR;
}

static uint32_t protected_bit_recovery(struct Ex10Interfaces ex10_iface)
{
    // Create selects to use based on the password of the tag
    struct BitSpan non_zero_access_info = {(uint8_t*)&non_zero_access_pwd, 32};

    struct SelectCommandArgs non_zero_select_args =
        create_select_args(&non_zero_access_info);

    // The non zero select is must be sent to force a protected mode tag to
    // reveal itself.
    struct Gen2CommandSpec non_zero_select = {
        .command = Gen2Select,
        .args    = &non_zero_select_args,
    };

    // Need to find the tag with the non-zero pwd protected select
    // clang-format off
    printf("Enter with non zero pwd and return to not-protected mode\n");
    if (inventory_and_halt(ex10_iface, &non_zero_select, true)) {    
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR);
        return PROTECTED_HALT_ERROR; }
    if (enter_access_mode(ex10_iface, non_zero_access_pwd)) { return PROTECTED_ENTER_ACCESS_ERROR; }
    if (set_mode_state(ex10_iface, false)) { return PROTECTED_SET_MODE_ERROR; }
    // clang-format on
    ex10_iface.reader->stop_transmitting();
    sleep(2);

    return PROTECTED_NO_ERROR;
}

static uint32_t access_password_recovery(struct Ex10Interfaces ex10_iface)
{
    // Assumes we are not in protected mode
    // clang-format off
    printf("Enter with non zero pwd and change back\n");
    if (inventory_and_halt(ex10_iface, NULL, true)) {    
        printf("Inventory and Halt return value %d \n",PROTECTED_HALT_ERROR); 
        return PROTECTED_HALT_ERROR; }
    if (enter_access_mode(ex10_iface, non_zero_access_pwd)) { return PROTECTED_ENTER_ACCESS_ERROR; }
    if (change_access_pwd(ex10_iface, zero_access_pwd)) { return PROTECTED_CHANGE_PWD_ERROR; }
    // clang-format on
    ex10_iface.reader->stop_transmitting();
    sleep(2);

    return PROTECTED_NO_ERROR;
}

int main(int argc, char* argv[])
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);

    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");
    ex10_iface.helpers->check_board_init_status(Application);

    assert(argc > 1 && "You must pass an appropriate command");
     char     arg_in      = *(argv[1]);     // tried, failes      char     arg_in[2]      = *(argv[2]);   
     uint32_t test_return = PROTECTED_NO_ERROR;

    // Used to recover from a bad script exit
    // Ended the script with the protected mode bit set, which must have a
    // non-zero access password
    
    if (arg_in == 'l')
    {
        test_return = protected_bit_recovery(ex10_iface);
        if (test_return != PROTECTED_NO_ERROR)
        {
            printf("Recovery not needed or failed with code 1st = %d\n", test_return);
        }
        test_return = PROTECTED_NO_ERROR;
    }
    // Used to recover from a bad script exit
    // Ended the script with a non-zero access password
    else if (arg_in == 'a')
    {
        test_return = access_password_recovery(ex10_iface);
        if (test_return != PROTECTED_NO_ERROR)
        {
            printf("Recovery not needed or failed with code 2nd = %d\n", test_return);
        }
        test_return = PROTECTED_NO_ERROR;
    }
    else if (arg_in == 't')
    {
        test_return = run_protected_mode_example(ex10_iface);
    }
    else
    {
        printf("No valid arg passed in.\n");
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return test_return;
}
