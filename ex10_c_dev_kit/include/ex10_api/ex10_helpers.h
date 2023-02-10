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

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ex10_api/application_register_definitions.h"
#include "ex10_api/board_init.h"
#include "ex10_api/bootloader_registers.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/gen2_commands.h"
#include "ex10_api/gen2_tx_command_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The zero password is the default password of tags. It is also the value
/// to restore the tag to in examples.
#define ZERO_ACCESS_PWD ((uint32_t)0x00000000)

/// The non_zero password is used for all examples to align to one non-zero
/// password.
#define NON_ZERO_ACCESS_PWD ((uint32_t)0x55555555)  // was 0x55555555  56781234  12345678


 /* The maximum EPC length in bytes as contained within the struct TagReadData.
 * The maximim EPC length is calculated as:56781234
 * The PC is read and prepended into the EPC buffer:        0x002 bytes
 * From the Gen2 specification: 0x210 - 0x020 = 0x1F0 bits, 0x03E bytes
 * Allow for 2 XPC words to be backscattered                0x004 bytes
 * Total EPC buffer allocated:                              0x044 bytes
 */
#define EPC_BUFFER_BYTE_LENGTH ((size_t)0x044u)

/**
 * @enum InventoryHelperReturns
 * This enum details the different reasons for return when using
 * 'simple_inventory' or 'continuous_inventory'.
 */
enum InventoryHelperReturns
{
    InvHelperSuccess        = 0,
    InvHelperOpStatusError  = 1,
    InvHelperStopConditions = 2,
    InvHelperTimeout        = 3,
};

/**
 * @struct TagReadData
 * During an inventory, when the EventFifo TagRead packet is received, this
 * structure is used as the destination for copying the data from the EventFifo
 * packet.
 */
struct TagReadData
{
    /// The tag's Protocol-Control word.
    /// @note This matches the value stored in the EPC bytes[0:1].
    uint16_t pc;

    /// Tag's XPC word 1
    uint16_t xpc_w1;
    /// Indicates if XPC W1 was detected (indicated by XI=1 in PC word)
    bool xpc_w1_is_valid;

    /// Tag's XPC word 2
    uint16_t xpc_w2;
    /// Indicates if XPC W1 was detected (indicated by XEB=1 in XPC W1 word)
    bool xpc_w2_is_valid;

    /// The tag PC + EPC data backscattered by the tag.
    uint8_t epc[EPC_BUFFER_BYTE_LENGTH];

    /// The number of bytes captured into the TagReadData.epc[] buffer.
    size_t epc_length;

    /// The Stored CRC value backscattered by the tag.
    uint16_t stored_crc;

    /// Indicates whether a Stored CRC value was backscattered by the tag.
    bool stored_crc_is_valid;

    /// The TID data backscattered by the tag if FastId is enabled.
    uint8_t tid[TID_LENGTH_BYTES];

    /// The number of bytes contained in the TagReadData.tid[] buffer.
    size_t tid_length;
};

// Stores information based on fifo data returns.
struct InfoFromPackets
{
    size_t             gen2_transactions;
    size_t             total_singulations;
    size_t             total_tid_count;
    size_t             times_halted;
    struct TagReadData access_tag;
};

struct InventoryHelperParams
{
    uint8_t const  antenna;
    uint16_t const rf_mode;
    uint16_t const tx_power_cdbm;
    // Config parameters for the inventory control register
    struct InventoryRoundControlFields* inventory_config;
    // Config parameters for the inventory control 2 register
    struct InventoryRoundControl_2Fields const* inventory_config_2;
    // Whether to execute the send select op
    bool const send_selects;
    // If true, the radio does not turn off due to regulatory timing
    bool const remain_on;
    // Automatically flips the target between inventory rounds
    bool const              dual_target;
    uint32_t const          inventory_duration_ms;
    struct InfoFromPackets* packet_info;
    // Prints all incoming fifo packets
    bool const verbose;
};

struct ContInventoryHelperParams
{
    struct InventoryHelperParams*      inventory_params;
    struct StopConditions const*       stop_conditions;
    struct ContinuousInventorySummary* summary_packet;
};

struct Ex10Helpers
{
    /**
     * Check the board initialization status.
     * - Call the function ex10_get_board_init_status() and obtain the
     *   struct Ex10BoardInitStatus indicating if an error occurred and
     *   whether the Impinj Reader Chip is executing in the bootloader or
     *   application context.
     * - If an error occurred then print the errors strings to stderr.
     *
     * @param status_check The expected Impinj Reader Chip mode of operation:
     *                     Bootloader or Application.
     *
     * @return int   An indication of whether the board initialization
     *               completed successfully and as expected.
     * @retval 0     The board was initialized correctly, without any errors,
     *               and the Impinj Reader Chip is operating in the expected
     *               Bootloader or Application context.
     * @retval -1    Either the board initialization failed with an error, or
     *               the Impinj Reader Chip mode of operation does not match
     *               the status_check parameter.
     */
    int (*check_board_init_status)(enum Status status_check);

    /**
     * Prints out OpCompletionStatus struct.
     * @param op_error The OpCompletionStatus struct to print.
     */
    void (*print_op_completion_status)(
        struct OpCompletionStatus const* op_error);

    /// Print the contents of the struct Ex10CommandsHostErrors.
    void (*print_commands_host_errors)(
        struct Ex10CommandsHostErrors const* error);

    /// Print the contents of the struct CommandResultFields.
    void (*print_command_result_fields)(
        struct CommandResultFields const* error);

    /// Checks the Gen2Reply for error and prints error message
    bool (*check_gen2_error)(struct Gen2Reply const* reply);

    /// Prints out useful debug around the aggregate op buffer
    void (*print_aggregate_op_errors)(
        const struct AggregateOpSummary agg_summary);

    /**
     * Purge FIFO of remaining packets. Print summary of each packet
     * @param print_packets Print each packet purged from the EventFifo queue.
     * @param flush_packets Uses the InsertFifofEvent command, with the
     *                      Trigger Interrupt flag set, to trigger an ReadFifo
     *                      command, allowing the host to receive all pending
     *                      EventFifo data held by the Reader Chip.
     * @param debug_aggregate_op Provides detailed information about an
     *                      AggregateOp failure, if an AggregateOpSummary
     *                      packet is encountered in the dumped packet stream.
     * @return size_t       The number of EventFifo packets discarded.
     */
    size_t (*discard_packets)(bool print_packets,
                              bool flush_packets,
                              bool debug_aggregate_op);

    /// Check if inventory round is halted on a tag
    bool (*inventory_halted)(void);

    /// Check if an error occurred in the returned op error as well as if extra
    /// debug is needed for an aggregate op error
    bool (*check_ops_status_errors)(struct OpCompletionStatus op_status);

    /**
     * Clears the state of the passed struct which stores state collected from
     * event fifo packets.
     * @param return_info The accumulated inventory state.
     */
    void (*clear_info_from_packets)(struct InfoFromPackets* return_info);

    /**
     * When iterating through EventFifo packets, this function can be used to
     * accumulate the inventory state stored in the InfoFromPackets struct.
     * @param packet      The EventFifo packet to examine.
     * @param return_info The accumulated inventory state.
     * @note  If the EventFifo packet is of type TagRead then the
     *        return_info->access_tag values are filled in.
     */
    void (*examine_packets)(struct EventFifoPacket const* packet,
                            struct InfoFromPackets*       return_info);

    /**
     * Perform a deep copy of an EventFifo packet from one EventFifoPacket
     * struct to another.
     */
    bool (*deep_copy_packet)(struct EventFifoPacket*       dst,
                             union PacketData*             dst_static_data,
                             struct ByteSpan               dst_dynamic_data,
                             struct EventFifoPacket const* src);

    /// Run inventory rounds until a timeout or halted
    enum InventoryHelperReturns (*simple_inventory)(
        struct InventoryHelperParams* ihp);

    /// Run continuous inventory until a stop condition is met or halted
    enum InventoryHelperReturns (*continuous_inventory)(
        struct ContInventoryHelperParams* cihp);

    /**
     * Copy the EventFifo TagRead extracted data into the destination type
     * struct TagReadData.
     *
     * @details
     * The struct TagReadFields contains data pointers into the relevant fields
     * of the EventFifo TagRead packet, allowing for easy data extraction.
     * The EventFifo packet will be deleted when the function
     * Ex10Reader.packet_remove() function is called, which releases the
     * EventFifo memory back to the pool for reuse.
     * The struct TagRead provides a destination for the TagRead data, allowing
     * the client to use the tag data beyond the lifetime of the EventFifo
     * TagRead packet.
     *
     * @return bool Indicates whether the copy operation was successful.
     * @retval true indicates that the tag was copied without error.
     * @retval false indicates that either the EPC or the TID length was
     *         larger than the struct TagReadData allocation. To determine which
     *         copy failed, the destination length fields should be compared
     *         with the source length fields.
     */
    bool (*copy_tag_read_data)(struct TagReadData*         dst,
                               struct TagReadFields const* src);

    /// Translate from RemainReason enum to a human-readable string
    const char* (*get_remain_reason_string)(enum RemainReason remain_reason);

    /// Swaps the two bytes of a uint16_t. Useful for correcting endian-ness
    /// of uint16_t values reported from the (big-endian) Gen2 tag.
    uint16_t (*swap_bytes)(uint16_t value);

    /// Waits for an RSSI measurement event fifo. This assumes the FIFO has
    /// already been sent from the device or will be with no further
    /// intervention from the user.
    uint16_t (*get_rssi_from_fifo_packet)(void);

    /// Runs the MeasureRssiOp, waits for the event fifo corresponding to the op
    /// finishing, and returns the log2 Rssi reported back.
    uint16_t (*read_rssi_value_from_op)(uint8_t rssi_count);

    /// Clears the gen2 buffer, adds a new command, and immediately sends it.
    struct Gen2TxCommandManagerError (*send_single_halted_command)(
        struct Gen2CommandSpec* cmd_spec);
};

const struct Ex10Helpers* get_ex10_helpers(void);

#ifdef __cplusplus
}
#endif
