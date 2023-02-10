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
#include <stdio.h>
#include <string.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/command_transactor.h"
#include "ex10_api/event_fifo_printer.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/trace.h"


static void check_padding_bytes(struct EventFifoPacket const* packet)
{
    // Note: If dynamic_data is pointing at padding bytes, then
    // they shouold all be zero and pad out to 32-bit alignment.
    assert(packet->dynamic_data_length < sizeof(uint32_t));
    assert(packet->dynamic_data ==
           &packet->static_data->raw[packet->static_data_length]);
    uint8_t const* iter = packet->dynamic_data;
    for (; iter < packet->dynamic_data + packet->dynamic_data_length; ++iter)
    {
        assert(*iter == 0u);
    }
    // iter now points one byte past the last padding byte,
    // which is the location of the next packet in memory.
    // Check that this location is 32-bit aligned.
    assert((uintptr_t)iter % sizeof(uint32_t) == 0u);
}

static int insert_fifo_example(struct Ex10Interfaces ex10_iface)
{
    // Set the EventFifo threshold to something larger than the size
    // of all the test packets to be inserted. The IRQ will not be
    // triggered until the final InsertFifoEvent command.
    ex10_iface.protocol->set_event_fifo_threshold(2048u);

    // Do not insert a packet, just a request an EventFifo interrupt.
    // This will cause the initial HelloWorld packet to be read from Ex10.
    ex10_iface.reader->insert_fifo_event(true, NULL);

    // Wait for the HelloWorld packet to be read from the Ex10.
    get_ex10_time_helpers()->busy_wait_ms(20);

    // Check HelloWorld, the first packet after a reset:
    struct EventFifoPacket const* packet = ex10_iface.reader->packet_peek();
    assert(packet);
    assert(packet->packet_type == HelloWorld);
    get_ex10_event_fifo_printer()->print_packets(packet);
    ex10_iface.reader->packet_remove();

    // Try to read more packets. The packet queue should be empty.
    packet = ex10_iface.reader->packet_peek();
    assert(packet == NULL);

    union PacketData static_data = {
        .custom.payload_len = 0u,
    };

    // InsertFifoEvent event_packet_0:
    struct EventFifoPacket const event_packet_0 = {
        .packet_type         = Custom,
        .us_counter          = 0u,  // Will be set by Ex10 to Ex10 time.
        .static_data         = &static_data,
        .static_data_length  = sizeof(static_data.custom),
        .dynamic_data        = NULL,
        .dynamic_data_length = 0u,
        .is_valid            = true};

    ex10_iface.reader->insert_fifo_event(false, &event_packet_0);

    // InsertFifoEvent event_packet_1:
    uint8_t const test_pattern_1[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    static_data.custom.payload_len = sizeof(test_pattern_1) / sizeof(uint32_t);

    struct EventFifoPacket const event_packet_1 = {
        .packet_type         = Custom,
        .us_counter          = 0u,  // Will be set by Ex10 to Ex10 time.
        .static_data         = &static_data,
        .static_data_length  = sizeof(static_data.custom),
        .dynamic_data        = test_pattern_1,
        .dynamic_data_length = sizeof(test_pattern_1),
        .is_valid            = true};

    ex10_iface.reader->insert_fifo_event(false, &event_packet_1);

    // InsertFifoEvent event_packet_2:
    // clang-format off
    uint8_t const test_pattern_2[] = {0x12, 0x34, 0x56, 0x78,
                                      0xfe, 0xdc, 0xba, 0x98,
                                      0xf0, 0x00, 0x1b, 0xa1,
                                      0x87, 0x65, 0x43, 0x21};
    // clang-format on
    static_data.custom.payload_len = sizeof(test_pattern_2) / sizeof(uint32_t);

    struct EventFifoPacket const event_packet_2 = {
        .packet_type         = Custom,
        .us_counter          = 0u,  // Will be set by Ex10 to Ex10 time.
        .static_data         = &static_data,
        .static_data_length  = sizeof(static_data.custom),
        .dynamic_data        = test_pattern_2,
        .dynamic_data_length = sizeof(test_pattern_2),
        .is_valid            = true};

    ex10_iface.reader->insert_fifo_event(false, &event_packet_2);

    // Test the ContinuousInventorySummary packet
    struct ContinuousInventorySummary const summary = {
        .duration_us                = 10 * 1000u * 1000u,
        .number_of_inventory_rounds = 0x12345678u,
        .number_of_tags             = 0xABCDEF12u,
        .reason                     = SRMaxDuration,
        .last_op_id                 = StartInventoryRoundOp,
        .last_op_error              = ErrorUnknownError,
    };

    struct EventFifoPacket const summary_packet = {
        .packet_type         = ContinuousInventorySummary,
        .us_counter          = 0u,
        .static_data         = (union PacketData const*)&summary,
        .static_data_length  = sizeof(struct ContinuousInventorySummary),
        .dynamic_data        = NULL,
        .dynamic_data_length = 0u,
        .is_valid            = true,
    };

    // This time request the Ex10 interrupt get triggered.
    ex10_iface.reader->insert_fifo_event(true, &summary_packet);

    // give enough time for the interrupt handler to retrieve any packets
    get_ex10_time_helpers()->busy_wait_ms(20);

    // Check event_packet_0:
    packet = ex10_iface.reader->packet_peek();
    assert(packet);
    get_ex10_event_fifo_printer()->print_packets(packet);

    assert(packet->packet_type == Custom);
    assert(packet->static_data->custom.payload_len == 0u);
    assert(packet->static_data_length == sizeof(static_data.custom));
    assert(packet->dynamic_data_length == 0u);
    assert(packet->dynamic_data ==
           &packet->static_data->raw[packet->static_data_length]);

    ex10_iface.reader->packet_remove();

    // Check event_packet_1:
    packet = ex10_iface.reader->packet_peek();
    get_ex10_event_fifo_printer()->print_packets(packet);
    assert(packet);
    assert(packet->packet_type == Custom);
    assert(packet->static_data->custom.payload_len ==
           sizeof(test_pattern_1) / sizeof(uint32_t));
    assert(packet->static_data_length == sizeof(static_data.custom));
    assert(packet->dynamic_data_length == sizeof(test_pattern_1));
    assert(memcmp(packet->dynamic_data,
                  test_pattern_1,
                  packet->dynamic_data_length) == 0);
    ex10_iface.reader->packet_remove();

    // Check event_packet_2:
    packet = ex10_iface.reader->packet_peek();
    get_ex10_event_fifo_printer()->print_packets(packet);
    assert(packet);
    assert(packet->packet_type == Custom);
    assert(packet->static_data->custom.payload_len ==
           sizeof(test_pattern_2) / sizeof(uint32_t));
    assert(packet->static_data_length == sizeof(static_data.custom));
    assert(packet->dynamic_data_length == sizeof(test_pattern_2));
    assert(memcmp(packet->dynamic_data,
                  test_pattern_2,
                  packet->dynamic_data_length) == 0);
    ex10_iface.reader->packet_remove();

    // Check for the ContinuousInventorySummary packet:
    packet = ex10_iface.reader->packet_peek();
    get_ex10_event_fifo_printer()->print_packets(packet);
    assert(packet);
    assert(packet->packet_type == ContinuousInventorySummary);
    assert(packet->static_data_length ==
           sizeof(static_data.continuous_inventory_summary));
    check_padding_bytes(packet);

    struct ContinuousInventorySummary const* packet_summary =
        &packet->static_data->continuous_inventory_summary;

    assert(packet_summary->duration_us == summary.duration_us);
    assert(packet_summary->number_of_inventory_rounds ==
           summary.number_of_inventory_rounds);
    assert(packet_summary->number_of_tags == summary.number_of_tags);
    assert(packet_summary->reason == summary.reason);
    assert(packet_summary->last_op_id == summary.last_op_id);
    assert(packet_summary->last_op_error == summary.last_op_error);

    ex10_iface.reader->packet_remove();

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
        result = insert_fifo_example(ex10_iface);
    }

    ex10_bootloader_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
