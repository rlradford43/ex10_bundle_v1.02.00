/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2022 Impinj, Inc. All rights reserved.                      *
 *                                                                           *
 *****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/print_data.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"

/* Settings used when running this example */
static uint32_t const inventory_duration_ms       = 500;
static uint8_t const  antenna                     = 1;
static uint16_t const rf_mode                     = mode_148;
static uint16_t const transmit_power_cdbm         = 3000;
static uint8_t const  initial_q                   = 2;
static const uint8_t  max_q                       = 15u;
static const uint8_t  min_q                       = 0u;
static const uint8_t  num_min_q_cycles            = 1u;
static const uint16_t max_queries_since_valid_epc = 16u;
static bool const     dual_target                 = true;
static bool const     tag_focus_enable            = false;
static bool const     fast_id_enable              = true;
static const uint8_t  session                     = 0u;


/* Global state */
static const uint8_t   target                         = 0;
struct InfoFromPackets packet_info                    = {0u, 0u, 0u, 0u, {0u}};
struct Gen2CommandSpec access_cmds[MaxTxCommandCount] = {0u};
bool                   enables[MaxTxCommandCount]     = {0u};

/* The expected number of bits in the response from an M775 tag to a properly
 * constructed Authenticate command. For details see setup_gen2_authenticate()
 * below.
 */
static uint16_t const authenticate_rep_len_bits = 128;

/* Tag will reply with In-process reply format with the length field included,
 * so reply will be at least 57 bits long:
 * - Barker code (7 bits)
 * - Done        (1 bit)
 * - Header      (1 bit)
 * - Length      (16 bits)
 * - Response    (variable)
 * - RN          (16 bits)
 * - CRC         (16 bits)
 */
static size_t const min_in_process_reply_len_bits = 57u;

struct AuthenticateCommandReply
{
    uint8_t shortened_tid[8];
    uint8_t tag_response[8];
};

static int get_random_challenge(uint8_t* msg_buffer, ssize_t msg_buffer_size)
{
    assert(msg_buffer_size == 6);

    char const* device_name = "/dev/random";
    int const   fd          = open(device_name, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr,
                "error: open(%s): failed: %d %s\n",
                device_name,
                errno,
                strerror(errno));
        return fd;
    }

    ssize_t const n_read = read(fd, msg_buffer, msg_buffer_size);

    if (0 != close(fd))
    {
        fprintf(stderr,
                "error: close(%s): failed: %d %s\n",
                device_name,
                errno,
                strerror(errno));
    }

    if (n_read != msg_buffer_size)
    {
        fprintf(stderr,
                "error: number of bytes read vs expected: %zu != %zu\n",
                n_read,
                msg_buffer_size);
        return -1;
    }

    return 0;
}

/**
 * Before starting inventory, setup Gen2 Authenticate command in Gen2 buffer.
 *
 * @details
 * This test targets Impinj M775 tags, that support the Authenticate
 * command with the Present-80 Crypto suite as defined by ISO/IEC 29167-11.
 *
 * The authenticate challenge buffer is the message
 * portion of the Gen2v2 Authenticate command.
 *  - AuthMethod  = 00b  (2 bits)
 *  - RFU         = 000b (3 bits)
 *  - Include TID        (1 bit)
 *  - Random Challenge   (42 bits)
 *
 * For additional details please reach out to support@impinj.com.
 *
 * The response is 128 bits long - consists of 64-bit result of the
 * IChallenge computation using PRESENT-80 and prepended with 64 bits of
 * the TID.
 */
static int setup_gen2_authenticate(void)
{
    // Create 6 bytes of random data
    uint8_t auth_message_buffer[6];
    memset(auth_message_buffer, 0u, sizeof(auth_message_buffer));

    if (0 !=
        get_random_challenge(auth_message_buffer, sizeof(auth_message_buffer)))
    {
        fprintf(stderr, "Failed to obtain a random challenge\n");
        return -1;
    }

    // Bits 0-5 on the first byte are 'AuthMethod', 'RFU' and 'Include TID'
    // to correct values.
    auth_message_buffer[0] &= 0x3u;
    auth_message_buffer[0] |= 0x4u;

    struct BitSpan auth_message = {
        .data   = auth_message_buffer,
        .length = sizeof(auth_message_buffer) * 8u,
    };

    struct AuthenticateCommandArgs authenticate_args = {
        .send_rep     = true,
        .inc_rep_len  = true,
        .csi          = 1u,
        .length       = sizeof(auth_message_buffer) * 8u,
        .message      = &auth_message,
        .rep_len_bits = authenticate_rep_len_bits,
    };

    struct Gen2CommandSpec authenticate_cmd = {
        .command = Gen2Authenticate,
        .args    = &authenticate_args,
    };

    // Clear the buffer
    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    g2tcm->clear_local_sequence();

    struct Gen2TxCommandManagerError const curr_error =
        g2tcm->encode_and_append_command(&authenticate_cmd, 0);

    /* First command added must have index 0 */
    assert(curr_error.current_index == 0u);

    enables[curr_error.current_index]     = true;
    access_cmds[curr_error.current_index] = authenticate_cmd;

    /* Enable the access command to be sent when halted. */
    g2tcm->write_sequence();
    g2tcm->write_halted_enables(enables, MaxTxCommandCount);

    printf("Challenge:\t\t0x");
    ex10_print_data(stdout,
                    auth_message_buffer,
                    sizeof(auth_message_buffer),
                    DataPrefixNone);

    return 0;
}

static int run_inventory_rounds(struct Ex10Interfaces ex10_iface,
                                enum SelectType const select_type)
{
    struct InventoryRoundControlFields inventory_config = {
        .initial_q            = initial_q,
        .max_q                = max_q,
        .min_q                = min_q,
        .num_min_q_cycles     = num_min_q_cycles,
        .fixed_q_mode         = false,
        .q_increase_use_query = false,
        .q_decrease_use_query = false,
        .session              = session,
        .select               = select_type,
        .target               = target,
        .halt_on_all_tags     = true,
        .fast_id_enable       = fast_id_enable,
        .tag_focus_enable     = tag_focus_enable,
    };

    struct InventoryRoundControl_2Fields const inventory_config_2 = {
        .max_queries_since_valid_epc = max_queries_since_valid_epc};

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
        if (enables[iter])
        {
            next      = &access_cmds[iter];
            cmd_index = iter + 1u;
            break;
        }
    }
    return next;
}

bool decode_m775_authenticate_reply(uint8_t const* reply_data,
                                    size_t         reply_length_bits,
                                    uint8_t*       m775_auth_response)
{
    if (reply_length_bits < min_in_process_reply_len_bits)
    {
        printf("Expected at least %zd bits, only %zd bits received\n",
               min_in_process_reply_len_bits,
               reply_length_bits);
        return false;
    }

    uint8_t rx_barker_code = (reply_data[0] >> 1) & 0x7E;
    uint8_t rx_done        = (reply_data[0] & 0x1);
    uint8_t rx_header      = (reply_data[1] >> 7) & 0x1;

    uint16_t rx_length =
        (((uint16_t)(reply_data[1] & 0x7E)) << 8) | (reply_data[2]);
    uint8_t rx_even_parity = (reply_data[3] >> 7) & 0x1;

    if (reply_length_bits - min_in_process_reply_len_bits != rx_length)
    {
        fprintf(
            stderr,
            "Expected response length of %zd bits vs calculated length of %d",
            reply_length_bits - min_in_process_reply_len_bits,
            rx_length);

        fprintf(stderr,
                "\nBarker code: 0x%x\nDone: 0x%x\nHeader: 0x%x\nParity: 0x%x\n",
                rx_barker_code,
                rx_done,
                rx_header,
                rx_even_parity);
    }

    uint8_t rx_response_byte = 0;
    for (size_t i = 0; i < (rx_length / 8); i++)
    {
        rx_response_byte = (uint8_t)(reply_data[3 + i] << 1) |
                           ((reply_data[3 + i + 1] >> 7) & 0x1);
        m775_auth_response[i] = rx_response_byte;
    }

    return true;
}

/**
 * Run inventory round, halt on first tag, execute gen2 sequence
 *
 * @param       ex10_iface         The Ex10 interface struct.
 * @param [out] authenticate_reply The M775 tag response to the Authenticate
 *                                 command, parsed into the
 *                                 struct AuthenticateCommandReply.
 *
 * @return int  Zero if successful, non-zero for failures.
 */
static int run_access_authenticate(
    struct Ex10Interfaces            ex10_iface,
    struct AuthenticateCommandReply* authenticate_reply)
{
    if (0 != setup_gen2_authenticate())
    {
        return -1;
    }

    if (run_inventory_rounds(ex10_iface, SelectAll) == -1)
    {
        return -1;
    }

    /* Should be halted on a tag now */
    bool const halted = ex10_iface.helpers->inventory_halted();
    if (halted == false)
    {
        fprintf(stderr, "error: %s: failed to enter halted state\n", __func__);
        return -1;
    }

    /* Trigger stored Gen2 sequence */
    ex10_iface.ops->send_gen2_halted_sequence();

    /* Wait for Gen2Transaction packets to be returned */
    uint32_t const timeout = 1000;
    uint16_t       reply_words[10u];
    memset(reply_words, 0u, sizeof(reply_words));

    struct Gen2Reply reply = {.error_code = NoError, .data = reply_words};
    size_t           gen2_packet_count_expected = 1u;
    uint8_t          m775_auth_response[16];

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

                if (reply.transaction_status != Gen2TransactionStatusOk)
                {
                    fprintf(stderr,
                            "Gen2 transaction status error: %u \n",
                            reply.transaction_status);
                    return -1;
                }

                if (reply.error_code != NoError)
                {
                    fprintf(stderr, "Tag error code: %u \n", reply.error_code);
                    return -1;
                }

                if (!decode_m775_authenticate_reply(
                        packet->dynamic_data,
                        packet->static_data->gen2_transaction.num_bits,
                        m775_auth_response))
                {
                    fprintf(stderr, "Error in decoding authenticate reply");
                    return -1;
                }

                memcpy(authenticate_reply->shortened_tid,
                       m775_auth_response,
                       sizeof(authenticate_reply->shortened_tid));

                memcpy(authenticate_reply->tag_response,
                       m775_auth_response +
                           sizeof(authenticate_reply->shortened_tid),
                       sizeof(authenticate_reply->tag_response));

                printf("Tags Shortened TID:\t0x");
                ex10_print_data(stdout,
                                authenticate_reply->shortened_tid,
                                sizeof(authenticate_reply->shortened_tid),
                                DataPrefixNone);

                printf("Tag Response:\t\t0x");
                ex10_print_data(stdout,
                                authenticate_reply->tag_response,
                                sizeof(authenticate_reply->tag_response),
                                DataPrefixNone);
            }
            ex10_iface.reader->packet_remove();
            packet = ex10_iface.reader->packet_peek();
        }
    }

    if (gen2_packet_count_expected != 0u)
    {
        fprintf(stderr, "Unexpected number of Gen2Transaction packets");
        return -1;
    }

    /* Demonstrate continuing to next tag, not used here. */
    ex10_iface.reader->continue_from_halted(false);
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
        struct AuthenticateCommandReply authenticate_reply;
        memset(&authenticate_reply, 0, sizeof(authenticate_reply));

        result = run_access_authenticate(ex10_iface, &authenticate_reply);
        if (result == -1)
        {
            printf("M775 Authenticate issue.\n");
        }
        ex10_iface.reader->stop_transmitting();
    }

    ex10_typical_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
