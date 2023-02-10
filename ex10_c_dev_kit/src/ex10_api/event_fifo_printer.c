/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2022 Impinj, Inc. All rights reserved.               *
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
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/print_data.h"

static size_t print_event_tx_ramp_up(FILE*                         file,
                                     struct EventFifoPacket const* packet)
{
    return fprintf(file,
                   "[%10u us] Tx ramp up on channel %u kHz\n",
                   packet->us_counter,
                   packet->static_data->tx_ramp_up.carrier_frequency);
}

static size_t print_event_tx_ramp_down(FILE*                         file,
                                       struct EventFifoPacket const* packet)
{
    char const* reason = NULL;
    switch (packet->static_data->tx_ramp_down.reason)
    {
        case RampDownHost:
            reason = "user";
            break;
        case RampDownRegulatory:
            reason = "regulatory";
            break;
        default:
            reason = "unknown";
            assert(0);
            break;
    }
    return fprintf(file,
                   "[%10u us] Tx ramp down, reason %s\n",
                   packet->us_counter,
                   reason);
}

static size_t print_event_inventory_round_summary(
    FILE*                         file,
    struct EventFifoPacket const* packet)
{
    char const* reason = NULL;
    switch (packet->static_data->inventory_round_summary.reason)
    {
        case InventorySummaryDone:
            reason = "done";
            break;
        case InventorySummaryHost:
            reason = "host";
            break;
        case InventorySummaryRegulatory:
            reason = "regulatory";
            break;
        case InventorySummaryEventFifoFull:
            reason = "event_fifo_full";
            break;
        case InventorySummaryTxNotRampedUp:
            reason = "tx_not_ramped_up";
            break;
        case InventorySummaryInvalidParam:
            reason = "invalid_param";
            break;
        case InventorySummaryLmacOverload:
            reason = "lmac_overload";
            break;
        default:
            reason = "unknown";
            assert(0);
            break;
    }

    return fprintf(
        file,
        "[%10u us] Inventory round stopped - reason: %s, duration_us: %u, "
        "total_slots: %u, num_slots: %u, empty_slots: %u, "
        "single_slots: %u, collided_slots: %u\n",
        packet->us_counter,
        reason,
        packet->static_data->inventory_round_summary.duration_us,
        packet->static_data->inventory_round_summary.total_slots,
        packet->static_data->inventory_round_summary.num_slots,
        packet->static_data->inventory_round_summary.empty_slots,
        packet->static_data->inventory_round_summary.single_slots,
        packet->static_data->inventory_round_summary.collided_slots);
}

static size_t print_event_q_changed(FILE*                         file,
                                    struct EventFifoPacket const* packet)
{
    struct QChanged const q_packet = packet->static_data->q_changed;
    return fprintf(
        file,
        "[%10u us] Q changed - number of slots: %u, empty slots: %u, single "
        "slots: %u, collided slots: %u, q value: %u, query sent: %u\n",
        packet->us_counter,
        q_packet.num_slots,
        q_packet.empty_slots,
        q_packet.single_slots,
        q_packet.collided_slots,
        q_packet.q_value,
        q_packet.sent_query);
}

static size_t print_event_tag_read(FILE*                         file,
                                   struct EventFifoPacket const* packet)
{
    struct Ex10Reader const*  reader  = get_ex10_reader();
    struct Ex10Helpers const* helpers = get_ex10_helpers();
    size_t                    n_write = 0u;
    struct TagReadFields      tag_read =
        get_ex10_event_parser()->get_tag_read_fields(
            packet->dynamic_data,
            packet->dynamic_data_length,
            packet->static_data->tag_read.type,
            packet->static_data->tag_read.tid_offset);

    n_write += fprintf(file,
                       "[%10u us] PC: 0x%04X",
                       packet->us_counter,
                       helpers->swap_bytes(*(tag_read.pc)));
    if (tag_read.xpc_w1)
    {
        n_write += fprintf(
            file, ", XPC W1: 0x%04X", helpers->swap_bytes(*(tag_read.xpc_w1)));
    }
    if (tag_read.xpc_w2)
    {
        n_write += fprintf(
            file, ", XPC W2: 0x%04X", helpers->swap_bytes(*(tag_read.xpc_w2)));
    }
    n_write += fprintf(file, ", EPC: 0x ");
    n_write += ex10_print_data_line(file, tag_read.epc, tag_read.epc_length);
    if (tag_read.stored_crc)
    {
        n_write += fprintf(
            file, ", CRC: 0x%04X", helpers->swap_bytes(*(tag_read.stored_crc)));
    }
    int16_t compensated_rssi = reader->get_current_compensated_rssi(
        packet->static_data->tag_read.rssi);
    n_write += fprintf(file, ", RSSI: %d", compensated_rssi);
    n_write += fprintf(file,
                       ", Halted Status: %d",
                       packet->static_data->tag_read.halted_on_tag);

    if ((packet->static_data->tag_read.type == TagReadTypeEpcWithTid) ||
        (packet->static_data->tag_read.type == TagReadTypeEpcWithFastIdTid))
    {
        n_write += fprintf(file, ", TID: ");
        for (size_t idx = 0; idx < tag_read.tid_length; idx++)
        {
            n_write += fprintf(file, "%02X", tag_read.tid[idx]);
        }
    }
    n_write += fprintf(file, "\n");

    return n_write;
}

static size_t print_event_gen2_transaction(FILE*                         file,
                                           struct EventFifoPacket const* packet)
{
    size_t      n_write = 0u;
    char const* status  = NULL;
    switch (packet->static_data->gen2_transaction.status)
    {
        case Gen2TransactionStatusOk:
            status = "ok";
            break;
        case Gen2TransactionStatusBadCrc:
            status = "bad_crc";
            break;
        case Gen2TransactionStatusNoReply:
            status = "no_reply";
            break;
        case Gen2TransactionStatusInvalidReplyType:
            status = "invalid_reply_type";
            break;
        default:
            status = "unknown";
            assert(0);
            break;
    }
    n_write +=
        fprintf(file,
                "[%10u us] Gen2Transaction - transaction id: %u, status: %s, "
                "num_bits: %u, data: ",
                packet->us_counter,
                packet->static_data->gen2_transaction.transaction_id,
                status,
                packet->static_data->gen2_transaction.num_bits);

    uint8_t const* reply_data =
        (uint8_t const*)packet->static_data + sizeof(struct Gen2Transaction);
    size_t const reply_data_length =
        packet->static_data->gen2_transaction.num_bits / 8u;

    n_write += ex10_print_data_line(file, reply_data, reply_data_length);
    n_write += fprintf(file, "\n");
    return n_write;
}

static size_t print_event_continuous_inventory_summary(
    FILE*                         file,
    struct EventFifoPacket const* packet)
{
    char const* reason = NULL;
    switch (packet->static_data->continuous_inventory_summary.reason)
    {
        case SRNone:
            reason = "none";
            break;
        case SRHost:
            reason = "host";
            break;
        case SRMaxNumberOfRounds:
            reason = "max rounds hit";
            break;
        case SRMaxNumberOfTags:
            reason = "max tags hit";
            break;
        case SRMaxDuration:
            reason = "max duration_us hit";
            break;
        default:
            reason = "unknown";
            assert(0);
            break;
    }

    return fprintf(
        file,
        "[%10u us] Continuous inventory stopped - reason: %s, duration_us: %u, "
        "number_of_inventory_rounds: %u, number_of_tags: %u\n",
        packet->us_counter,
        reason,
        packet->static_data->continuous_inventory_summary.duration_us,
        packet->static_data->continuous_inventory_summary
            .number_of_inventory_rounds,
        packet->static_data->continuous_inventory_summary.number_of_tags);
}

static size_t print_event_hello_world(FILE*                         file,
                                      struct EventFifoPacket const* packet)
{
    return fprintf(
        file,
        "[%10u us] Hello from E%3x, Reset reason: 0x%02x, cond: %u\n",
        packet->us_counter,
        packet->static_data->hello_world.sku,
        packet->static_data->hello_world.reset_reason,
        packet->static_data->hello_world.crash_info_conditional);
}

static size_t print_event_custom(FILE*                         file,
                                 struct EventFifoPacket const* packet)
{
    size_t n_write = fprintf(file,
                             "[%10u us] Custom packet - length: %u, data: ",
                             packet->us_counter,
                             packet->static_data->custom.payload_len);
    if (packet->dynamic_data_length > 16u)
    {
        n_write += fprintf(file, "\n");
        n_write += ex10_print_data(file,
                                   packet->dynamic_data,
                                   packet->dynamic_data_length,
                                   DataPrefixIndex);
    }
    else
    {
        n_write += ex10_print_data_line(
            file, packet->dynamic_data, packet->dynamic_data_length);
        n_write += fprintf(file, "\n");
    }
    return n_write;
}

static size_t print_event_power_control_loop_summary(
    FILE*                         file,
    struct EventFifoPacket const* packet)
{
    return fprintf(
        file,
        "[%10u us] PowerControl - iterations_taken: %u, "
        "final_error: %d, final_tx_fine_gain: %d\n",
        packet->us_counter,
        packet->static_data->power_control_loop_summary.iterations_taken,
        packet->static_data->power_control_loop_summary.final_error,
        packet->static_data->power_control_loop_summary.final_tx_fine_gain);
}

static size_t print_rssi_summary(FILE*                         file,
                                 struct EventFifoPacket const* packet)
{
    return fprintf(
        file,
        "[%10u us] RSSI - RSSI_linear: %d, RSSI_log2: %d, RSSI_count: %d\n",
        packet->us_counter,
        packet->static_data->measure_rssi_summary.rssi_linear,
        packet->static_data->measure_rssi_summary.rssi_log2,
        packet->static_data->measure_rssi_summary.rssi_count);
}

static size_t print_aggregate_op_summary(FILE*                         file,
                                         struct EventFifoPacket const* packet)
{
    return fprintf(
        file,
        "[%10u us] Aggregate op - op_run_count: %u, write_count: %u, "
        "insert_fifo_count: %u, final_buffer_byte_index: %u, "
        "total_jump_count: %u, last_inner_op: run: 0x%02x, error: 0x%02x, "
        "identifier: 0x%04x\n",
        packet->us_counter,
        packet->static_data->aggregate_op_summary.op_run_count,
        packet->static_data->aggregate_op_summary.write_count,
        packet->static_data->aggregate_op_summary.insert_fifo_count,
        packet->static_data->aggregate_op_summary.final_buffer_byte_index,
        packet->static_data->aggregate_op_summary.total_jump_count,
        packet->static_data->aggregate_op_summary.last_inner_op_run,
        packet->static_data->aggregate_op_summary.last_inner_op_error,
        packet->static_data->aggregate_op_summary.identifier);
}

static size_t print_halted(FILE* file, struct EventFifoPacket const* packet)
{
    return fprintf(file,
                   "[%10u us] Lmac Halted with handle %u\n",
                   packet->us_counter,
                   packet->static_data->halted.halted_handle);
}

static int32_t residue_magnitude(int32_t residue_i, int32_t residue_q)
{
    double const  root_2 = sqrt(2.0);
    int32_t const mag_2  = (residue_i * residue_i) + (residue_q * residue_q);
    return lround(sqrt(mag_2) / root_2);
}

static size_t print_event_sjc_measurement(FILE*                         file,
                                          struct EventFifoPacket const* packet)
{
    int32_t const residue_mag =
        residue_magnitude(packet->static_data->sjc_measurement.residue_i,
                          packet->static_data->sjc_measurement.residue_q);

    return fprintf(
        file,
        "[%10u us] SJC c: (%5d, %5d), a: %u, f: %u, r: (%9d, %9d), %10d\n",
        packet->us_counter,
        packet->static_data->sjc_measurement.cdac_i,
        packet->static_data->sjc_measurement.cdac_q,
        packet->static_data->sjc_measurement.rx_atten,
        packet->static_data->sjc_measurement.flags,
        packet->static_data->sjc_measurement.residue_i,
        packet->static_data->sjc_measurement.residue_q,
        residue_mag);
}

static size_t print_event_write_profile_data(
    FILE*                         file,
    struct EventFifoPacket const* packet)
{
    size_t n_write = fprintf(file,
                             "[%10u us] file_no: %d '",
                             packet->us_counter,
                             packet->static_data->write_profile_data.file_no);

    uint8_t const* const endp =
        packet->dynamic_data +
        packet->static_data->write_profile_data.payload_length;
    for (uint8_t const* iter = packet->dynamic_data; iter < endp; ++iter)
    {
        if (isprint(*iter))
        {
            fputc(*iter, file);
            n_write += 1u;
        }
        else
        {
            n_write += fprintf(file, "\\x%02x", *iter);
        }
    }
    n_write += fprintf(file, "'\n");
    return n_write;
}

static size_t print_event_debug(FILE*                         file,
                                struct EventFifoPacket const* packet)
{
    size_t n_write = 0u;
    // Debug data has no one interpretation
    n_write += fprintf(file,
                       "[%10u us] Debug packet: length: %u, data: ",
                       packet->us_counter,
                       packet->static_data->debug.payload_len);

    if (packet->static_data->debug.payload_len > 32u)
    {
        n_write += fprintf(file, "\n");
        n_write += ex10_print_data(file,
                                   packet->dynamic_data,
                                   packet->dynamic_data_length,
                                   DataPrefixIndex);
    }
    else
    {
        n_write += ex10_print_data_line(
            file, packet->dynamic_data, packet->dynamic_data_length);
        n_write += fprintf(file, "\n");
    }

    return n_write;
}

static size_t print_event_mystery(FILE*                         file,
                                  struct EventFifoPacket const* packet)
{
    size_t n_write = 0u;
    // Unknown packets will have their static data set to zero
    // and the dynamic data will span all data following the header.
    n_write += fprintf(file,
                       "Mystery packet %u - length: %zu, data:\n",
                       packet->packet_type,
                       packet->dynamic_data_length);
    n_write += ex10_print_data(file,
                               packet->dynamic_data,
                               packet->dynamic_data_length,
                               DataPrefixIndex);
    return n_write;
}

static size_t print_invalid_packet(FILE*                         file,
                                   struct EventFifoPacket const* packet)
{
    (void)packet;
    size_t n_write = 0u;
    n_write += fprintf(file, "Invalid packet occurred with no known cause");
    return n_write;
}

static size_t print_packets(struct EventFifoPacket const* packet)
{
    assert(packet);
    FILE* file = stdout;
    switch (packet->packet_type)
    {
        case TxRampUp:
            return print_event_tx_ramp_up(file, packet);
        case TxRampDown:
            return print_event_tx_ramp_down(file, packet);
        case InventoryRoundSummary:
            return print_event_inventory_round_summary(file, packet);
        case QChanged:
            return print_event_q_changed(file, packet);
        case TagRead:
            return print_event_tag_read(file, packet);
        case Gen2Transaction:
            return print_event_gen2_transaction(file, packet);
        case ContinuousInventorySummary:
            return print_event_continuous_inventory_summary(file, packet);
        case HelloWorld:
            return print_event_hello_world(file, packet);
        case Custom:
            return print_event_custom(file, packet);
        case PowerControlLoopSummary:
            return print_event_power_control_loop_summary(file, packet);
        case WriteProfileData:
            return print_event_write_profile_data(file, packet);
        case SjcMeasurement:
            return print_event_sjc_measurement(file, packet);
        case MeasureRssiSummary:
            return print_rssi_summary(file, packet);
        case AggregateOpSummary:
            return print_aggregate_op_summary(file, packet);
        case Halted:
            return print_halted(file, packet);
        case Debug:
            return print_event_debug(file, packet);
        case InvalidPacket:
            print_invalid_packet(file, packet);
            return 0;
        default:
            return print_event_mystery(file, packet);
    }
}

static size_t print_tag_read_data(FILE*                     fp,
                                  struct TagReadData const* tag_read_data)
{
    assert(tag_read_data);
    size_t n_write = 0u;

    n_write += fprintf(fp, "PC: 0x%02x ", tag_read_data->pc);
    if (tag_read_data->xpc_w1_is_valid)
    {
        n_write += fprintf(fp, " XPC W1: 0x%04x", tag_read_data->xpc_w1);
    }
    if (tag_read_data->xpc_w2_is_valid)
    {
        n_write += fprintf(fp, " XPC W2: 0x%04x", tag_read_data->xpc_w2);
    }
    n_write += fprintf(fp, " EPC: 0x");
    for (size_t index = 0u; index < tag_read_data->epc_length; ++index)
    {
        n_write += fprintf(fp, "%02x", tag_read_data->epc[index]);
    }

    if (tag_read_data->tid_length > 0u)
    {
        n_write += fprintf(fp, " TID: 0x");
        for (size_t index = 0u; index < tag_read_data->tid_length; ++index)
        {
            n_write += fprintf(fp, "%02x'", tag_read_data->tid[index]);
        }
    }

    n_write += fprintf(fp, ", CRC: 0x");
    if (tag_read_data->stored_crc_is_valid)
    {
        n_write += fprintf(fp, "%04x", tag_read_data->stored_crc);
    }
    else
    {
        n_write += fprintf(fp, "none");
    }

    n_write += fprintf(fp, "\n");
    return n_write;
}

static size_t print_tag_read_data_stdout(
    struct TagReadData const* tag_read_data)
{
    return print_tag_read_data(stdout, tag_read_data);
}


static const struct Ex10EventFifoPrinter ex10_event_fifo_printer = {
    .print_packets       = print_packets,
    .print_tag_read_data = print_tag_read_data_stdout,
};

const struct Ex10EventFifoPrinter* get_ex10_event_fifo_printer(void)
{
    return &ex10_event_fifo_printer;
}
