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
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board/time_helpers.h"
#include "ex10_api/aggregate_op_builder.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_tx_command_manager.h"
#include "ex10_api/print_data.h"


static const char* const gen2_transaction_error[] = {
    "Other",
    "Not supported",
    "Insufficient privileges",
    "Memory overrun",
    "Memory locked",
    "Crypto suite",
    "Command not encapsulated",
    "Response buffer overflow",
    "Security timeout",
    "",
    "",
    "Insufficient power",
    "",
    "",
    "",
    "Non-specific",
};

static uint16_t swap_bytes(const uint16_t value)
{
    return ((value & 0x00FF) << 8) + ((value & 0xFF00) >> 8);
}

static bool check_gen2_error(struct Gen2Reply const* reply)
{
    if (reply->error_code != NoError)
    {
        fprintf(stderr,
                "Gen2Transaction Error: %s.\n ",
                gen2_transaction_error[reply->error_code]);
        return true;
    }
    return false;
}

static bool inventory_halted(void)
{
    struct HaltedStatusFields halted_status;
    get_ex10_protocol()->read(&halted_status_reg, &halted_status);
    return halted_status.halted;
}

static uint16_t get_rssi_from_fifo_packet(void)
{
    struct Ex10TimeHelpers const* time_helpers = get_ex10_time_helpers();

    // Wait for the RSSI summary from the op and return the log2 RSSI value
    struct Ex10Reader const* reader                = get_ex10_reader();
    uint16_t                 rssi_log2             = 0;
    bool                     rssi_summary_received = false;

    uint32_t start_time = time_helpers->time_now();
    uint32_t timeout_ms = 5000;
    while (rssi_summary_received == false &&
           time_helpers->time_elapsed(start_time) < timeout_ms)
    {
        struct EventFifoPacket const* packet = reader->packet_peek();
        if (packet)
        {
            if (packet->packet_type == MeasureRssiSummary)
            {
                rssi_log2 = packet->static_data->measure_rssi_summary.rssi_log2;
                rssi_summary_received = true;
            }
            reader->packet_remove();
        }
    }

    return rssi_log2;
}

static uint16_t read_rssi_value_from_op(uint8_t rssi_count)
{
    struct Ex10Ops const* ops = get_ex10_ops();

    // We first start the rssi op
    ops->wait_op_completion();
    ops->measure_rssi(rssi_count);
    struct OpCompletionStatus const op_error = ops->wait_op_completion();
    if (op_error.error_occurred)
    {
        return 0;
    }

    return get_rssi_from_fifo_packet();
}

static void handle_invalid_packet(struct EventFifoPacket const* packet)
{
    // This should not occur, thus the cause and recovery are unknown.
    assert(packet->packet_type != InvalidPacket &&
           "Invalid packet occurred with no known cause");
}

static bool deep_copy_packet(struct EventFifoPacket*       dst,
                             union PacketData*             dst_static_data,
                             struct ByteSpan               dst_dynamic_data,
                             struct EventFifoPacket const* src)
{
    assert(dst);
    assert(dst_static_data);
    assert(dst_dynamic_data.data);
    assert(src);

    dst->packet_type = src->packet_type;
    dst->us_counter  = src->us_counter;
    dst->is_valid    = src->is_valid;

    memcpy(dst_static_data, src->static_data, src->static_data_length);
    dst->static_data        = dst_static_data;
    dst->static_data_length = src->static_data_length;

    const size_t num_bytes = dst_dynamic_data.length < src->dynamic_data_length
                                 ? dst->dynamic_data_length
                                 : src->dynamic_data_length;

    memcpy(dst_dynamic_data.data, src->dynamic_data, num_bytes);
    dst->dynamic_data        = dst_dynamic_data.data;
    dst->dynamic_data_length = num_bytes;

    return num_bytes == src->dynamic_data_length;
}

static int check_board_init_status(enum Status status_check)
{
    bool                             ret_val     = 0;
    struct Ex10BoardInitStatus const init_status = ex10_get_board_init_status();

    if (init_status.power_up_status != (int)status_check)
    {
        if (init_status.power_up_status > 0)
        {
            fprintf(stderr,
                    "error: power_up reset to %d: expected %d\n",
                    init_status.power_up_status,
                    status_check);
        }
        else
        {
            fprintf(stderr,
                    "error: power_up error: %d\n",
                    init_status.power_up_status);
        }
        ret_val = -1;
    }

    if (init_status.protocol_error != 0)
    {
        fprintf(stderr,
                "error: board_init protocol error: %d\n",
                init_status.protocol_error);
        ret_val = -1;
    }

    if (init_status.op_status.error_occurred)
    {
        fprintf(stderr, "error: board_init ex10 Ops error:\n");
        get_ex10_helpers()->print_op_completion_status(&init_status.op_status);
        ret_val = -1;
    }

    return ret_val;
}

static void print_op_completion_status(
    struct OpCompletionStatus const* op_error)
{
    FILE* const fp = stdout;

    fprintf(fp, "command_error: 0x%02x\n", op_error->command_error);
    fprintf(fp,
            "Op: Id: 0x%02x, status: 0x%02x, busy: %u\n",
            op_error->ops_status.op_id,
            op_error->ops_status.error,
            op_error->ops_status.busy);
    fprintf(fp, "timeout_error: %u\n", op_error->timeout_error);
    fprintf(fp,
            "aggregate_buffer_overflow: %u\n",
            op_error->aggregate_buffer_overflow);
    fflush(fp);
}

void print_commands_host_errors(struct Ex10CommandsHostErrors const* error)
{
    FILE* const fp = stdout;
    fprintf(fp, "error_occurred : %u\n", error->error_occurred);
    fprintf(fp, "device_response: 0x%02x\n", error->device_response);
    fprintf(fp, "host_result    : %u\n", error->host_result);
    fflush(fp);
}

void print_command_result_fields(struct CommandResultFields const* error)
{
    FILE* const fp = stdout;
    fprintf(
        fp, "failed_result_code        : 0x%02x\n", error->failed_result_code);
    fprintf(
        fp, "failed_command_code       : 0x%02x\n", error->failed_command_code);
    fprintf(fp,
            "commands_since_first_error: %u\n",
            error->commands_since_first_error);
    fflush(fp);
}

static void print_aggregate_op_errors(
    const struct AggregateOpSummary agg_summary)
{
    // Print out any  errors related to ops for clarity
    if (agg_summary.last_inner_op_error != ErrorNone)
    {
        fprintf(stderr,
                "The last op run in the aggregate op was 0x%X, "
                "which ended with an error code of %d\n",
                agg_summary.last_inner_op_run,
                agg_summary.last_inner_op_error);
        fprintf(stderr,
                "For meaning of the error code, reference enum OpsStatus\n");
        fprintf(
            stderr,
            "The number of ops that ran including the one that failed is %d\n",
            agg_summary.op_run_count);
    }

    // Grab the aggregate op buffer from the device
    uint8_t         aggregate_buffer[aggregate_op_buffer_reg.length];
    struct ByteSpan agg_op_span = {.data = aggregate_buffer,
                                   aggregate_op_buffer_reg.length};
    get_ex10_protocol()->read(&aggregate_op_buffer_reg, aggregate_buffer);

    // Parse the buffer
    struct Ex10AggregateOpBuilder const* agg_builder =
        get_ex10_aggregate_op_builder();
    struct AggregateOpInstruction  instruction_at_index;
    union AggregateInstructionData instruction_data;
    instruction_at_index.instruction_data = &instruction_data;

    ssize_t err_inst = agg_builder->get_instruction_from_index(
        agg_summary.final_buffer_byte_index,
        &agg_op_span,
        &instruction_at_index);
    if (err_inst == -1)
    {
        fprintf(stderr,
                "There is no valid instruction at the index which was reported "
                "to cause the error\n");
    }
    else
    {
        fprintf(stderr,
                "The error occurred at instruction %zd in the buffer\n",
                err_inst);
    }

    fprintf(stderr, "Dumping the contents of the buffer for debug\n");
    agg_builder->print_buffer(&agg_op_span);
}

static size_t discard_packets(bool print_packets,
                              bool flush_packets,
                              bool debug_aggregate_op)
{
    size_t                        packet_count = 0u;
    struct Ex10Reader const*      reader       = get_ex10_reader();
    struct EventFifoPacket const* packet       = reader->packet_peek();

    // Flush all packets from the device event fifo buffer
    if (flush_packets)
    {
        reader->insert_fifo_event(true, NULL);
    }

    while (packet)
    {
        if (print_packets)
        {
            get_ex10_event_fifo_printer()->print_packets(packet);
        }
        if (packet->packet_type == InvalidPacket)
        {
            handle_invalid_packet(packet);
        }
        else if (packet->packet_type == AggregateOpSummary &&
                 debug_aggregate_op)
        {
            print_aggregate_op_errors(
                packet->static_data->aggregate_op_summary);
        }
        packet_count += 1u;
        reader->packet_remove();
        packet = reader->packet_peek();
    }

    return packet_count;
}

static bool copy_tag_read_data(struct TagReadData*         dst,
                               struct TagReadFields const* src)
{
    assert(dst);
    assert(src);

    dst->pc = *src->pc;

    if (src->xpc_w1)
    {
        dst->xpc_w1          = *src->xpc_w1;
        dst->xpc_w1_is_valid = true;
    }
    else
    {
        dst->xpc_w1          = 0;
        dst->xpc_w1_is_valid = false;
    }

    if (src->xpc_w2)
    {
        dst->xpc_w2          = *src->xpc_w2;
        dst->xpc_w2_is_valid = true;
    }
    else
    {
        dst->xpc_w2          = 0;
        dst->xpc_w2_is_valid = false;
    }

    memset(dst->epc, 0u, sizeof(dst->epc));
    size_t const epc_length = (sizeof(dst->epc) < src->epc_length)
                                  ? sizeof(dst->epc)
                                  : src->epc_length;
    memcpy(dst->epc, src->epc, epc_length);
    dst->epc_length = epc_length;

    if (src->stored_crc)
    {
        dst->stored_crc          = *src->stored_crc;
        dst->stored_crc_is_valid = true;
    }
    else
    {
        dst->stored_crc          = 0u;
        dst->stored_crc_is_valid = false;
    }

    memset(dst->tid, 0u, sizeof(dst->tid));
    dst->tid_length = 0u;
    if (src->tid)
    {
        size_t const tid_length = (sizeof(dst->tid) < src->tid_length)
                                      ? sizeof(dst->tid)
                                      : src->tid_length;
        memcpy(dst->tid, src->tid, tid_length);
        dst->tid_length = tid_length;
    }

    bool success = true;

    if (dst->epc_length != src->epc_length)
    {
        success = false;
        fprintf(stderr,
                "error: EPC copy failed, length: %zu != %zu\n ",
                dst->epc_length,
                src->epc_length);
    }

    if (dst->tid_length != src->tid_length)
    {
        success = false;
        fprintf(stderr,
                "error: TID copy failed, length: %zu != %zu\n ",
                dst->tid_length,
                src->tid_length);
    }

    return success;
}

static bool check_ops_status_errors(struct OpCompletionStatus op_status)
{
    if (op_status.error_occurred)
    {
        if (op_status.ops_status.error != ErrorNone)
        {
            fprintf(stderr, "Op error occurred\n");
            discard_packets(true, true, true);
        }
        if (op_status.command_error != Success ||
            op_status.timeout_error != NoTimeout)
        {
            fprintf(stderr, "Error in sending command\n");
        }
        discard_packets(true, true, false);
        return false;
    }
    return true;
}

static void clear_info_from_packets(struct InfoFromPackets* return_info)
{
    return_info->gen2_transactions     = 0;
    return_info->total_singulations    = 0;
    return_info->total_tid_count       = 0;
    return_info->times_halted          = 0;
    return_info->access_tag.epc_length = 0;
    return_info->access_tag.tid_length = 0;
}

/* Print information based on the EventFifo contents. */
static void examine_packets(struct EventFifoPacket const* packet,
                            struct InfoFromPackets*       return_info)
{
    assert(packet);
    if (packet->packet_type == TagRead)
    {
        struct TagReadFields const tag_read =
            get_ex10_event_parser()->get_tag_read_fields(
                packet->dynamic_data,
                packet->dynamic_data_length,
                packet->static_data->tag_read.type,
                packet->static_data->tag_read.tid_offset);

        copy_tag_read_data(&(return_info->access_tag), &(tag_read));
        return_info->total_singulations += 1;
        return_info->times_halted +=
            packet->static_data->tag_read.halted_on_tag;

        if ((packet->static_data->tag_read.type == TagReadTypeEpcWithTid) ||
            (packet->static_data->tag_read.type == TagReadTypeEpcWithFastIdTid))
        {
            return_info->total_tid_count += 1;
        }
    }
    else if (packet->packet_type == Gen2Transaction)
    {
        return_info->gen2_transactions += 1;
    }
}

static enum InventoryHelperReturns simple_inventory(
    struct InventoryHelperParams* ihp)
{
    struct Ex10Reader const* reader     = get_ex10_reader();
    bool                     round_done = true;
    uint32_t                 start_time = get_ex10_time_helpers()->time_now();

    // Clear the number of tags found so that if we halt, we can return
    clear_info_from_packets(ihp->packet_info);
    discard_packets(ihp->verbose, true, false);

    while (get_ex10_time_helpers()->time_elapsed(start_time) <
           ihp->inventory_duration_ms)
    {
        if (ihp->packet_info->times_halted > 0)
        {
            break;
        }
        if (round_done)
        {
            round_done = false;
            struct OpCompletionStatus const op_error =
                reader->inventory(ihp->antenna,
                                  ihp->rf_mode,
                                  ihp->tx_power_cdbm,
                                  ihp->inventory_config,
                                  ihp->inventory_config_2,
                                  ihp->send_selects,
                                  0u,
                                  ihp->remain_on);
            if (!check_ops_status_errors(op_error))
            {
                return InvHelperOpStatusError;
            }
            if (ihp->dual_target)
            {
                ihp->inventory_config->target = !ihp->inventory_config->target;
            }
        }

        struct EventFifoPacket const* packet = reader->packet_peek();

        while (packet)
        {
            examine_packets(packet, ihp->packet_info);
            if (get_ex10_time_helpers()->time_elapsed(start_time) >
                ihp->inventory_duration_ms)
            {
                break;
            }
            if (ihp->verbose)
            {
                get_ex10_event_fifo_printer()->print_packets(packet);
            }
            if (packet->packet_type == InvalidPacket)
            {
                handle_invalid_packet(packet);
            }
            else if (packet->packet_type == InventoryRoundSummary)
            {
                round_done = true;
            }
            else if (packet->packet_type == TxRampDown)
            {
                // Note that session 0 is used and thus on a transmit power
                // down the tag state is reverted to A. If one chose to use
                // a session with persistence between power cycles, this
                // could go away.
                ihp->inventory_config->target = 0;
                round_done                    = true;
            }

            reader->packet_remove();
            packet = reader->packet_peek();
        }
    }

    // If we are told to halt on tags we return to the user after halting, and
    // thus don't clean up
    if (false == ihp->inventory_config->halt_on_all_tags)
    {
        // Regulatory timers will automatically ramp us down, but we are being
        // explicit here.
        reader->stop_transmitting();

        while (false == round_done)
        {
            struct EventFifoPacket const* packet = reader->packet_peek();
            while (packet != NULL)
            {
                examine_packets(packet, ihp->packet_info);
                if (ihp->verbose)
                {
                    get_ex10_event_fifo_printer()->print_packets(packet);
                }
                if (packet->packet_type == InvalidPacket)
                {
                    handle_invalid_packet(packet);
                }
                else if (packet->packet_type == InventoryRoundSummary)
                {
                    round_done = true;
                }
                reader->packet_remove();
                packet = reader->packet_peek();
            }
        }
    }

    if (ihp->verbose)
    {
        fprintf(stdout,
                "Time elapsed: %d ms\n",
                get_ex10_time_helpers()->time_elapsed(start_time));
    }

    return InvHelperSuccess;
}

static enum InventoryHelperReturns continuous_inventory(
    struct ContInventoryHelperParams* cihp)
{
    struct Ex10Reader const*      reader         = get_ex10_reader();
    bool                          inventory_done = false;
    struct InventoryHelperParams* ihp            = cihp->inventory_params;

    if ((cihp->stop_conditions->max_number_of_rounds == 0) &&
        (cihp->stop_conditions->max_number_of_tags == 0) &&
        (cihp->stop_conditions->max_duration_us == 0))
    {
        return InvHelperStopConditions;
    }

    // Clear the number of tags found so that if we halt, we can return
    clear_info_from_packets(ihp->packet_info);
    discard_packets(false, true, false);

    // Recording the start time
    uint32_t const start_time_ms = get_ex10_time_helpers()->time_now();

    // Starting continuous inventory
    struct OpCompletionStatus const op_status =
        reader->continuous_inventory(ihp->antenna,
                                     ihp->rf_mode,
                                     ihp->tx_power_cdbm,
                                     ihp->inventory_config,
                                     ihp->inventory_config_2,
                                     ihp->send_selects,
                                     cihp->stop_conditions,
                                     ihp->dual_target,
                                     0u,
                                     false);
    if (!check_ops_status_errors(op_status))
    {
        return InvHelperOpStatusError;
    }

    // Adding a failsafe timer to ensure that this function completes
    uint64_t const failsafe_ms_bounds =
        500u * (cihp->stop_conditions->max_duration_us / 1000);
    uint32_t const failsafe_timeout_ms =
        (failsafe_ms_bounds > UINT32_MAX) ? UINT32_MAX : failsafe_ms_bounds;

    struct EventFifoPacket const* packet = NULL;

    while (false == inventory_done)
    {
        // If we are halting on tags, there is no reason to continue waiting for
        // inventory summary packet
        if (ihp->packet_info->total_singulations &&
            ihp->inventory_config->halt_on_all_tags)
        {
            break;
        }

        if ((failsafe_timeout_ms) && (get_ex10_time_helpers()->time_elapsed(
                                          start_time_ms) > failsafe_timeout_ms))
        {
            return InvHelperTimeout;
        }

        packet = reader->packet_peek();
        if (packet != NULL)
        {
            examine_packets(packet, ihp->packet_info);
            if (ihp->verbose)
            {
                get_ex10_event_fifo_printer()->print_packets(packet);
            }
            if (packet->packet_type == InvalidPacket)
            {
                handle_invalid_packet(packet);
            }
            else if (packet->packet_type == ContinuousInventorySummary)
            {
                inventory_done = true;

                if (cihp->summary_packet)
                {
                    memcpy(cihp->summary_packet,
                           &(packet->static_data->continuous_inventory_summary),
                           sizeof(struct ContinuousInventorySummary));
                }
            }
            reader->packet_remove();
        }
    }

    if (ihp->verbose)
    {
        fprintf(stderr,
                "Time elapsed: %d ms\n",
                get_ex10_time_helpers()->time_elapsed(start_time_ms));
    }

    return InvHelperSuccess;
}

static const char* get_remain_reason_string(enum RemainReason remain_reason)
{
    switch (remain_reason)
    {
        case RemainReasonNoReason:
            return "NoReason";
        case RemainReasonReadyNAsserted:
            return "ReadyNAsserted";
        case RemainReasonApplicationImageInvalid:
            return "ApplicationImageInvalid";
        case RemainReasonResetCommand:
            return "ResetCommand";
        case RemainReasonCrash:
            return "Crash";
        case RemainReasonWatchdog:
            return "Watchdog";
        case RemainReasonLockup:
            return "Lockup";
        default:
            return "UNKNOWN";
    }
}

static struct Gen2TxCommandManagerError send_single_halted_command(
    struct Gen2CommandSpec* cmd_spec)
{
    struct Ex10Gen2TxCommandManager const* g2tcm =
        get_ex10_gen2_tx_command_manager();
    // Clear the local buffer
    g2tcm->clear_local_sequence();
    // add a single command and enable
    struct Gen2TxCommandManagerError error =
        g2tcm->encode_and_append_command(cmd_spec, 0);
    if (false == error.error_occurred)
    {
        bool halted_enables[MaxTxCommandCount];
        memset(&halted_enables, 0u, sizeof(halted_enables));

        halted_enables[error.current_index] = true;

        g2tcm->write_sequence();
        g2tcm->write_halted_enables(halted_enables, MaxTxCommandCount);
        // Send the command
        get_ex10_ops()->send_gen2_halted_sequence();
    }
    return error;
}

static const struct Ex10Helpers ex10_helpers = {
    .check_board_init_status     = check_board_init_status,
    .print_op_completion_status  = print_op_completion_status,
    .print_commands_host_errors  = print_commands_host_errors,
    .print_command_result_fields = print_command_result_fields,
    .check_gen2_error            = check_gen2_error,
    .discard_packets             = discard_packets,
    .print_aggregate_op_errors   = print_aggregate_op_errors,
    .inventory_halted            = inventory_halted,
    .deep_copy_packet            = deep_copy_packet,
    .check_ops_status_errors     = check_ops_status_errors,
    .clear_info_from_packets     = clear_info_from_packets,
    .examine_packets             = examine_packets,
    .simple_inventory            = simple_inventory,
    .continuous_inventory        = continuous_inventory,
    .copy_tag_read_data          = copy_tag_read_data,
    .get_remain_reason_string    = get_remain_reason_string,
    .swap_bytes                  = swap_bytes,
    .get_rssi_from_fifo_packet   = get_rssi_from_fifo_packet,
    .read_rssi_value_from_op     = read_rssi_value_from_op,
    .send_single_halted_command  = send_single_halted_command,
};

const struct Ex10Helpers* get_ex10_helpers(void)
{
    return &ex10_helpers;
}
