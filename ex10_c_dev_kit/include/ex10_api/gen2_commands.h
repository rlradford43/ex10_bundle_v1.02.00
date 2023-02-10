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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "commands.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_COMMAND_BYTES ((size_t)0x20u)

/**
 * @enum Gen2Command
 * Gen2 standard commands
 */
enum Gen2Command
{
    Gen2Select = 0,
    Gen2Read,
    Gen2Write,
    Gen2Kill_1,  ///< First step of the kill command, immediate reply.
    Gen2Kill_2,  ///< Second step of the kill command, delayed reply.
    Gen2Lock,
    Gen2Access,
    Gen2BlockWrite,
    Gen2BlockPermalock,
    Gen2Authenticate,
    _COMMAND_MAX,
};

enum BlockPermalockReadLock
{
    Read      = 0,
    Permalock = 1,
};

enum SelectTarget
{
    Session0     = 0,
    Session1     = 1,
    Session2     = 2,
    Session3     = 3,
    SelectedFlag = 4,
    SELECT_TARGET_MAX,
};

enum SelectAction
{
    Action000 = 0,  ///< match:SL=1, inv->A      non-match:SL=0, inv->B
    Action001 = 1,  ///< match:SL=1, inv->A      non-match:do nothing
    Action010 = 2,  ///< match:do nothing        non-match:SL=0, inv->B
    Action011 = 3,  ///< match:SL=!, (A->B,B->A) non-match:do nothing
    Action100 = 4,  ///< match:SL=0, inv->B      non-match:SL=1, inv->A
    Action101 = 5,  ///< match:SL=0, inv->B      non-match:do nothing
    Action110 = 6,  ///< match:do nothing        non-match:SL=1, inv->A
    Action111 = 7,  ///< match:do nothing        non-match:SL=!, (A->B, B->A)
};

enum SelectMemoryBank
{
    SelectFileType = 0,
    SelectEPC      = 1,
    SelectTID      = 2,
    SelectFile0    = 3,
};

enum SelectType
{
    SelectAll         = 0,
    SelectAll2        = 1,
    SelectNotAsserted = 2,
    SelectAsserted    = 3,
};

enum MemoryBank
{
    Reserved = 0,
    EPC      = 1,
    TID      = 2,
    User     = 3,
};

enum TagErrorCode
{
    Other                  = 0,
    NotSupported           = 1,
    InsufficientPrivileges = 2,
    MemoryOverrun          = 3,
    MemoryLocked           = 4,
    CryptoSuite            = 5,
    CommandNotEncapsulated = 6,
    ResponseBufferOverflow = 7,
    SecurityTimeout        = 8,
    InsufficientPower      = 11,
    NonSpecific            = 15,
    NoError                = 16,
};

/**
 * @enum ResponseType
 * Gen2 response types as defined in the Gen2TxnControls register.
 * To support in_process select "Delayed".
 */
enum ResponseType
{
    None_     = 0x0,
    Immediate = 0x1,
    Delayed   = 0x2,
    InProcess = 0x3
};

/**
 * @struct BitSpan
 * Defines a mutable location of contiguous memory, organized as bits.
 * Each byte in data is utilized, therefore 8 bits from length exist in
 * each byte. The bits and bytes are organized LSB first. This means
 * that bit 0 of byte 0 is the first bit, bit 1 of byte 0 is next, etc.
 */
struct BitSpan
{
    uint8_t* data;    ///< The pointer to beginning of the memory location.
    size_t   length;  ///< The number of bits in the span
};

struct Gen2CommandSpec
{
    enum Gen2Command command;
    void*            args;
};

struct Gen2Reply
{
    enum Gen2Command           reply;
    enum TagErrorCode          error_code;
    uint16_t*                  data;
    enum Gen2TransactionStatus transaction_status;
};

/**
 * Gen2 commands which can be sent to tags.
 */
struct SelectCommandArgs
{
    enum SelectTarget     target;
    enum SelectAction     action;
    enum SelectMemoryBank memory_bank;
    uint32_t              bit_pointer;
    uint8_t               bit_count;
    struct BitSpan*       mask;
    bool                  truncate;
} __attribute__((aligned(4)));

struct ReadCommandArgs
{
    enum MemoryBank memory_bank;
    uint32_t        word_pointer;
    uint8_t         word_count;
} __attribute__((aligned(4)));

struct WriteCommandArgs
{
    enum MemoryBank memory_bank;
    uint32_t        word_pointer;
    uint16_t        data;
} __attribute__((aligned(4)));

struct KillCommandArgs
{
    uint16_t password;
} __attribute__((aligned(4)));

struct LockCommandArgs
{
    bool kill_password_read_write_mask;
    bool kill_password_permalock_mask;
    bool access_password_read_write_mask;
    bool access_password_permalock_mask;
    bool epc_memory_write_mask;
    bool epc_memory_permalock_mask;
    bool tid_memory_write_mask;
    bool tid_memory_permalock_mask;
    bool file_0_memory_write_mask;
    bool file_0_memory_permalock_mask;
    bool kill_password_read_write_lock;
    bool kill_password_permalock;
    bool access_password_read_write_lock;
    bool access_password_permalock;
    bool epc_memory_write_lock;
    bool epc_memory_permalock;
    bool tid_memory_write_lock;
    bool tid_memory_permalock;
    bool file_0_memory_write_lock;
    bool file_0_memory_permalock;
} __attribute__((aligned(4)));

struct AccessCommandArgs
{
    uint16_t password;
} __attribute__((aligned(4)));

struct BlockWriteCommandArgs
{
    enum MemoryBank memory_bank;
    uint32_t        word_pointer;
    uint8_t         word_count;
    struct BitSpan* data;
} __attribute__((aligned(4)));

struct BlockPermalockCommandArgs
{
    enum BlockPermalockReadLock read_lock;
    enum MemoryBank             memory_bank;
    uint32_t                    block_pointer;
    uint8_t                     block_range;
    struct BitSpan*             mask;  ///< For the Read case, pass pointer
                                       ///< to a valid BitSpan with length=0
} __attribute__((aligned(4)));

struct AuthenticateCommandArgs
{
    bool            send_rep;
    bool            inc_rep_len;
    uint8_t         csi;
    uint16_t        length;
    struct BitSpan* message;
    uint16_t        rep_len_bits;  ///< if send_rep is True, this is the
                                   ///< expected number of bits in the tag's
                                   ///< response (length in bits of 'Response'
                                   ///< field in the In-process reply packet).
} __attribute__((aligned(4)));

/**
 * Gen2 replies which are received by the device.
 */
struct AccessCommandReply
{
    uint16_t tag_handle;
    uint8_t  response_crc[2];
} __attribute__((aligned(4)));
static_assert(sizeof(struct AccessCommandReply) == 4,
              "Size of AccessCommandResponse type incorrect");

struct KillCommandReply
{
    uint16_t tag_handle;
    uint16_t response_crc;
} __attribute__((aligned(4)));
static_assert(sizeof(struct KillCommandReply) == 4,
              "Size of KillCommandReply type incorrect");

struct DelayedReply
{
    uint16_t tag_handle;
    uint16_t response_crc;
} __attribute__((aligned(4)));
static_assert(sizeof(struct DelayedReply) == 4,
              "Size of DelayedReply type incorrect");

/**
 * An array of Gen2TxnControlsFields structs which are used as initial template
 * information by the transaction_config function.
 */
// clang-format off
static const struct Gen2TxnControlsFields transaction_configs[] = {
    /* Select */
    {.response_type   = None_,
     .has_header_bit  = false,
     .use_cover_code  = false,
     .append_handle   = false,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 0u},
    /* Read */
    {.response_type   = Immediate,
     .has_header_bit  = true,
     .use_cover_code  = false,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 33u},
    /* Write */
    {.response_type   = Delayed,
     .has_header_bit  = true,
     .use_cover_code  = true,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 33u},
    /* Kill_1 */
    {.response_type   = Immediate,
     .has_header_bit  = false,
     .use_cover_code  = false,      // Taken care of by is_kill_command
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = true,
     .Reserved0       = 0u,
     .rx_length       = 32u},
    /* Kill_2 */
     {.response_type   = Delayed,
      .has_header_bit  = true,
      .use_cover_code  = false,     // Taken care of by is_kill_command
      .append_handle   = true,
      .append_crc16    = true,
      .is_kill_command = true,
      .Reserved0       = 0u,
      .rx_length       = 33u},
    /* Lock */
    {.response_type   = Delayed,
     .has_header_bit  = true,
     .use_cover_code  = false,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 33u},
    /* Access */
    {.response_type   = Immediate,
     .has_header_bit  = false,
     .use_cover_code  = true,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 32u},
    /* BlockWrite */
    {.response_type   = Delayed,
     .has_header_bit  = true,
     .use_cover_code  = false,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 33u},
    /* BlockPermalock */
    {.response_type   = Delayed,
     .has_header_bit  = true,
     .use_cover_code  = false,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 33u},
    /* Authenticate */
    {.response_type   = InProcess,
     .has_header_bit  = false,
     .use_cover_code  = false,
     .append_handle   = true,
     .append_crc16    = true,
     .is_kill_command = false,
     .Reserved0       = 0u,
     .rx_length       = 41u},
};
// clang-format on

/**
 * @struct Ex10Gen2Commands
 * Gen2 commands encoder/decoder interface.
 */
struct Ex10Gen2Commands
{
    /**
     * Encodes a Gen2 command for use in the Gen2TxBuffer.
     *
     * @param cmd_spec A struct containing:
     *                 1) command - The command type as an enum  Gen2Command
     *                 2) args - A pointer to an arg struct for the specified
     *                    command, which contains values for the fields in
     *                    the CommandEncoder table.
     * @param [out] encoded_command A bit span containing info about the
     *                              encoded command. This contains the length
     *                              of the command and data for the
     *                              left-justified Gen2 command.
     * @param [return] Returns false if the command type is invalid, otherwise
     * returns true.
     * @note For select mask only: Everything is loaded in left-justified.
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
    bool (*encode_gen2_command)(const struct Gen2CommandSpec* cmd_spec,
                                struct BitSpan*               encoded_command);

    /**
     * Decodes an encoded Gen2 command. Takes in an encoded command via a
     * BitSpan and create populates a Gen2CommandSpec.
     *
     * @param [out] cmd_spec A struct containing:
     *                 1) command - The command type as an enum  Gen2Command
     *                 2) args - A pointer to an arg struct for the specified
     *                    command, which contains values for the fields in
     *                    the CommandEncoder table.
     * @param encoded_command A bit span containing info about the
     *                        encoded command. This contains the length
     *                        of the command and data for the left-justified
     *                        Gen2 command.
     * @param [return] Returns false if the command type is invalid, otherwise
     * returns true.
     */
    bool (*decode_gen2_command)(struct Gen2CommandSpec* cmd_spec,
                                const struct BitSpan*   encoded_command);

    /**
     * Decodes a Gen2 command reply into a Gen2Reply struct.
     *
     * @param command  The Gen2 command that caused this reply.
     * @param gen2_pkt A Gen2Transaction packet parsed using the packet parser.
     * @param decoded_reply A Gen2Reply struct pointer to store decoded output.
     * @return Returns false if the command has no known reply or if an error
     * is parsed in the return (showing the tag reported back and error).
     */
    bool (*decode_reply)(enum Gen2Command              command,
                         const struct EventFifoPacket* gen2_pkt,
                         struct Gen2Reply*             decoded_reply);

    /**
     * Determine the Gen2TxnControls register settings needed for this command.
     * @param cmd_spec A command spec used with 'encode_command'.
     * @return A Gen2TxnControlsFields struct which maps directly to the
     *         Gen2TxnControls register.
     */
    bool (*get_gen2_tx_control_config)(
        const struct Gen2CommandSpec* cmd_spec,
        struct Gen2TxnControlsFields* txn_control);
};

struct Ex10Gen2Commands const* get_ex10_gen2_commands(void);

#ifdef __cplusplus
}
#endif
