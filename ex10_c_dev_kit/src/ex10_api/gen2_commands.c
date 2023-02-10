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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ex10_api/byte_span.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/gen2_commands.h"


static const uint32_t EBV81_MAX = (uint32_t)((1u << 7u) - 1u);
static const uint32_t EBV82_MAX = (uint32_t)((1u << 14u) - 1u);
static const uint32_t EBV83_MAX = (uint32_t)((1u << 21u) - 1u);

static_assert(sizeof(bool) == 1u, "Invalid bool size");

// Packs in bits using least significant byte and least significant bit
// Most fields of a gen2 command utilize least significant bit
static size_t bit_pack(uint8_t* encoded,
                       size_t   bit_offset,
                       uint32_t data,
                       size_t   bit_count)
{
    uint8_t byte_offset        = (bit_offset - (bit_offset % 8)) / 8;
    uint8_t bit_start          = bit_offset % 8;
    uint8_t bits_in_first_byte = 8 - bit_start;
    size_t  total_bits         = bit_offset;

    assert(data <= (1u << bit_count) - 1);

    // more bits remain than fit in the first byte
    // if bits_in_first_byte == 0, this is the same as the while below
    // if > 0, it means that we started in the middle of a byte
    if (bit_count > bits_in_first_byte)
    {
        uint8_t  rshift_bits        = bit_count - bits_in_first_byte;
        uint32_t data_in_first_byte = data >> rshift_bits;
        encoded[byte_offset] |= data_in_first_byte;
        // move forward and denote we added the bits
        bit_count -= bits_in_first_byte;
        byte_offset++;
        bit_start = 0;
        total_bits += bits_in_first_byte;
    }
    // we can fit in a whole byte
    while (bit_count >= 8)
    {
        uint8_t rshift_bits  = bit_count - 8;
        encoded[byte_offset] = data >> rshift_bits;
        // move forward and denote we added the bits
        bit_count -= 8;
        byte_offset++;
        total_bits += 8;
    }
    // shove in the rest of the bits left shifted into the next byte
    if (bit_count > 0)
    {
        uint8_t lshift_bits = (8 - bit_count) - bit_start;
        encoded[byte_offset] |= (data << lshift_bits) & 0xFF;
        total_bits += bit_count;
    }
    return total_bits;
}

// Pack in a gen2 command using most significant bit.
// NOTE: This function is used for a bit-stream packing, aka most significant
// bit. This is therefore only useful for variable length fields in a gen2
// command. All other fields are least significant bit.
// NOTE: This function is only used for the final bit packing of a bit stream
// (the last sub-8 bits). Above that, the bitstream will be packed byte by
// byte. using the lsb bit pack (since no bit shifts are needed).
static size_t bit_pack_msb_bits(uint8_t* encoded,
                                size_t   bit_offset,
                                uint32_t data,
                                size_t   bit_count)
{
    // Function is only to be used for packing most significant bits when under
    // 8 bits. Otherwise pack in by byte.
    assert(bit_count < 8);
    // Ensure the msb bits fit
    assert((data >> (8 - bit_count)) <= (1u << bit_count) - 1);

    uint8_t byte_offset        = (bit_offset - (bit_offset % 8)) / 8;
    uint8_t bit_start          = bit_offset % 8;
    uint8_t bits_in_first_byte = 8 - bit_start;
    size_t  total_bits         = bit_offset;
    uint8_t decoded_bits       = 0;

    // more bits remain than fit in the first byte
    // if bits_in_first_byte == 0, this is the same as the while below
    // if > 0, it means that we started in the middle of a byte
    if (bit_count > bits_in_first_byte)
    {
        uint8_t  rshift_bits        = 8 - bits_in_first_byte;
        uint32_t data_in_first_byte = data >> rshift_bits;

        encoded[byte_offset] |= data_in_first_byte;
        // move forward and denote we added the bits
        bit_count -= bits_in_first_byte;
        byte_offset++;
        bit_start = 0;
        total_bits += bits_in_first_byte;
        decoded_bits += bits_in_first_byte;
    }
    // shove in the rest of the bits starting with most significant bit
    if (bit_count > 0)
    {
        // if bit count is x, create a mask for top x bits
        uint8_t top_bit_mask = ~((1 << (8 - bit_count)) - 1);
        // shift the data first if some bits were placed in the previous encoded
        // byte now with the bits remaining for this byte, mask the top bits
        uint8_t masked_data = (data << decoded_bits) & top_bit_mask;

        // shift the remaining encode data to the right in the case that we are
        // not starting at bit 0
        encoded[byte_offset] |= masked_data >> bit_start;
        total_bits += bit_count;
    }
    return total_bits;
}

static size_t get_ebv_bit_len(uint32_t value, size_t max_value)
{
    assert(value <= max_value);
    uint8_t ebv_counters[3] = {0, 0, 0};
    while (value > 0x7F)
    {
        ebv_counters[1]++;
        value -= 0x80;
        if (ebv_counters[1] > 0x7F)
        {
            ebv_counters[2]++;
            break;
        }
    }
    if (ebv_counters[2] > 0)
    {
        return 24;
    }
    if (ebv_counters[1] > 0)
    {
        return 16;
    }
    return 8;
}

static size_t bit_pack_ebv(uint8_t* encoded_command,
                           size_t   start_length,
                           uint32_t value,
                           size_t   max_value)
{
    assert(value <= max_value);
    uint8_t ebv_counters[3] = {0, 0, 0};
    while (value > 0x7F)
    {
        if (ebv_counters[1] == 0x7F)
        {
            ebv_counters[2]++;
            ebv_counters[1] = 0u;
        }
        else
        {
            ebv_counters[1]++;
        }
        value -= 0x80;
    }
    ebv_counters[0] = value;

    if (ebv_counters[2] > 0)
    {
        ebv_counters[2] |= 0x80;
        ebv_counters[1] |= 0x80;
    }
    else if (ebv_counters[1] > 0)
    {
        ebv_counters[1] |= 0x80;
    }

    if (ebv_counters[2] > 0)
    {
        start_length =
            bit_pack(encoded_command, start_length, ebv_counters[2], 8u);
    }
    if (ebv_counters[1] > 0)
    {
        start_length =
            bit_pack(encoded_command, start_length, ebv_counters[1], 8u);
    }
    start_length = bit_pack(encoded_command, start_length, ebv_counters[0], 8u);

    return start_length;
}

static size_t bit_pack_from_pointer(uint8_t*       encoded_command,
                                    size_t         start_length,
                                    const uint8_t* data,
                                    size_t         bit_len)
{
    uint8_t curr_byte = 0;

    while (bit_len >= 8)
    {
        start_length =
            bit_pack(encoded_command, start_length, data[curr_byte], 8);
        curr_byte++;
        bit_len -= 8;
    }
    if (bit_len > 0)
    {
        // Note: Packing from a pointer is used for packing a variable length
        // field in a gen2 command. Variable length fields operate on a most
        // significant bit principle unlike other fields.
        start_length = bit_pack_msb_bits(
            encoded_command, start_length, data[curr_byte], bit_len);
    }
    return start_length;
}

static uint32_t bit_unpack_ebv(const uint8_t* cmd, size_t byte_len)
{
    uint32_t unpack_ebv = 0;
    if (byte_len > 2)
    {
        unpack_ebv += (cmd[2] & 0x7F) * 16384;
    }
    if (byte_len > 1)
    {
        unpack_ebv += (cmd[1] & 0x7F) * 128;
    }
    unpack_ebv += cmd[0] & 0x7F;
    return unpack_ebv;
}

static uint8_t* bit_unpack_msb(const uint8_t* cmd,
                               size_t         start_length,
                               size_t         bit_len)
{
    // NOTE: this will be overwritten. remember to copy the data
    static uint8_t unpack_buffer[20];
    memset(unpack_buffer, 0, 20);

    uint8_t curr_byte    = 0;
    uint8_t bit_start    = start_length % 8;
    uint8_t decoded_bits = 0;
    uint8_t byte_start   = (start_length - bit_start) / 8;

    while (bit_len > 0)
    {
        if (bit_start > 0)
        {
            uint8_t shifted_data = cmd[byte_start++] << bit_start;
            uint8_t bits_fit     = 8 - bit_start;
            if (bits_fit > bit_len)
            {
                uint8_t bits_over = bits_fit - bit_len;
                // mask data coming after our encode length
                uint8_t data_mask = ~((1 << bits_over) - 1);
                shifted_data &= data_mask;
                bits_fit = bit_len;
            }
            unpack_buffer[curr_byte] = shifted_data;
            bit_len -= bits_fit;
            decoded_bits = bits_fit;
            bit_start    = 0;
        }
        else
        {
            uint8_t shifted_data = cmd[byte_start] >> decoded_bits;
            uint8_t bits_fit     = 8 - decoded_bits;
            if (bits_fit == 8)
            {
                byte_start++;
            }
            if (bits_fit > bit_len)
            {
                uint8_t bits_over = bits_fit - bit_len;
                // mask data coming after our encode length
                uint8_t data_mask = ~((1 << bits_over) - 1);
                shifted_data &= data_mask;
                bits_fit = bit_len;
            }
            unpack_buffer[curr_byte++] |= shifted_data;
            bit_len -= bits_fit;
            bit_start    = bits_fit % 8;
            decoded_bits = 0;
        }
    }
    return unpack_buffer;
}

static uint8_t* bit_unpack(const uint8_t* cmd,
                           size_t         start_length,
                           size_t         bit_len)
{
    // NOTE: this will be overwritten. remember to copy the data
    static uint8_t unpack_buffer[20];
    memset(unpack_buffer, 0, 20);
    uint8_t decoded_bits = 0;

    while (bit_len > 0)
    {
        uint32_t final_bit    = start_length + bit_len;
        uint8_t  bit_end      = final_bit % 8;
        uint8_t  highest_byte = (final_bit - (final_bit % 8)) / 8;
        highest_byte -= ((final_bit % 8) == 0) ? 1 : 0;

        uint8_t decode_idx = (decoded_bits - (decoded_bits % 8)) / 8;

        uint8_t unpack_bits = 0;
        uint8_t bottom_bits = 8 - (decoded_bits % 8);

        if (bit_end != 0)
        {
            unpack_bits = (bit_end >= bit_len) ? bit_len : bit_end;
        }
        else
        {
            unpack_bits = (bottom_bits >= bit_len) ? bit_len : bottom_bits;
        }

        if (bit_end != 0)
        {
            unpack_buffer[decode_idx] |=
                (cmd[highest_byte] >> (8 - bit_end)) & ((1 << unpack_bits) - 1);
        }
        else
        {
            uint8_t data = cmd[highest_byte] & ((1 << unpack_bits) - 1);
            unpack_buffer[decode_idx] |= (data << (decoded_bits % 8));
        }
        bit_len -= unpack_bits;
        decoded_bits += unpack_bits;
    }
    return unpack_buffer;
}

static uint32_t ebv_length_decode(const uint8_t* cmd, uint32_t curr_bit_len)
{
    uint8_t  ebv_bytes     = 1;
    uint8_t* unpacked_data = bit_unpack(cmd, curr_bit_len, 8);
    if (unpacked_data[0] & 0x80)
    {
        ebv_bytes     = 2;
        unpacked_data = bit_unpack(cmd, curr_bit_len + 8, 8);
        if (unpacked_data[1] & 0x80)
        {
            ebv_bytes = 3;
        }
    }
    return ebv_bytes;
}

// clang-format off
// IPJ_autogen | generate_sdk_gen2_encode {

static void select_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;


    // Pull out the byte span data for for the variable length field mask
    const struct SelectCommandArgs* span_args = (const struct SelectCommandArgs*)arg_base;
    assert(span_args->mask);
    const struct BitSpan* span_data = span_args->mask;
    size_t total_span_bits = span_data->length;
    if(total_span_bits > 0)
    {
        assert(span_data->data);
    }

    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    4 + // command
    3 + // target
    3 + // action
    2 + // memory_bank
    get_ebv_bit_len(((const struct SelectCommandArgs*)arg_base)->bit_pointer, EBV82_MAX) + // bit_pointer
    8 + // bit_count
    total_span_bits + // mask
    1 + // truncate
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0x0a, 4);
    // Adding field target
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, target);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 3);
    // Adding field action
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, action);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 3);
    // Adding field memory_bank
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, memory_bank);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 2);
    // Adding field bit_pointer
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, bit_pointer);
    curr_bit_len = bit_pack_ebv(encoded_command, curr_bit_len, ((const uint32_t*)arg_ptr)[0], EBV82_MAX);
    // Adding field bit_count
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, bit_count);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 8);
    // Adding field mask
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, mask);
    curr_bit_len = bit_pack_from_pointer(encoded_command, curr_bit_len, span_data->data, total_span_bits);
    // Adding field truncate
    arg_ptr      = arg_base + offsetof(struct SelectCommandArgs, truncate);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void read_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;



    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    2 + // memory_bank
    get_ebv_bit_len(((const struct ReadCommandArgs*)arg_base)->word_pointer, EBV83_MAX) + // word_pointer
    8 + // word_count
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc2, 8);
    // Adding field memory_bank
    arg_ptr      = arg_base + offsetof(struct ReadCommandArgs, memory_bank);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 2);
    // Adding field word_pointer
    arg_ptr      = arg_base + offsetof(struct ReadCommandArgs, word_pointer);
    curr_bit_len = bit_pack_ebv(encoded_command, curr_bit_len, ((const uint32_t*)arg_ptr)[0], EBV83_MAX);
    // Adding field word_count
    arg_ptr      = arg_base + offsetof(struct ReadCommandArgs, word_count);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 8);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void write_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;



    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    2 + // memory_bank
    get_ebv_bit_len(((const struct WriteCommandArgs*)arg_base)->word_pointer, EBV83_MAX) + // word_pointer
    16 + // data
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc3, 8);
    // Adding field memory_bank
    arg_ptr      = arg_base + offsetof(struct WriteCommandArgs, memory_bank);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 2);
    // Adding field word_pointer
    arg_ptr      = arg_base + offsetof(struct WriteCommandArgs, word_pointer);
    curr_bit_len = bit_pack_ebv(encoded_command, curr_bit_len, ((const uint32_t*)arg_ptr)[0], EBV83_MAX);
    // Adding field data
    arg_ptr      = arg_base + offsetof(struct WriteCommandArgs, data);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *((const uint16_t*)arg_ptr), 16);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void kill_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;



    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    16 + // password
    3 + // rfu
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc4, 8);
    // Adding field password
    arg_ptr      = arg_base + offsetof(struct KillCommandArgs, password);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *((const uint16_t*)arg_ptr), 16);
    // Adding field rfu
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0, 3);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void lock_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;



    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    1 + // kill_password_read_write_mask
    1 + // kill_password_permalock_mask
    1 + // access_password_read_write_mask
    1 + // access_password_permalock_mask
    1 + // epc_memory_write_mask
    1 + // epc_memory_permalock_mask
    1 + // tid_memory_write_mask
    1 + // tid_memory_permalock_mask
    1 + // file_0_memory_write_mask
    1 + // file_0_memory_permalock_mask
    1 + // kill_password_read_write_lock
    1 + // kill_password_permalock
    1 + // access_password_read_write_lock
    1 + // access_password_permalock
    1 + // epc_memory_write_lock
    1 + // epc_memory_permalock
    1 + // tid_memory_write_lock
    1 + // tid_memory_permalock
    1 + // file_0_memory_write_lock
    1 + // file_0_memory_permalock
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc5, 8);
    // Adding field kill_password_read_write_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, kill_password_read_write_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field kill_password_permalock_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, kill_password_permalock_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field access_password_read_write_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, access_password_read_write_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field access_password_permalock_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, access_password_permalock_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field epc_memory_write_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, epc_memory_write_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field epc_memory_permalock_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, epc_memory_permalock_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field tid_memory_write_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, tid_memory_write_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field tid_memory_permalock_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, tid_memory_permalock_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field file_0_memory_write_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, file_0_memory_write_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field file_0_memory_permalock_mask
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, file_0_memory_permalock_mask);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field kill_password_read_write_lock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, kill_password_read_write_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field kill_password_permalock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, kill_password_permalock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field access_password_read_write_lock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, access_password_read_write_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field access_password_permalock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, access_password_permalock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field epc_memory_write_lock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, epc_memory_write_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field epc_memory_permalock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, epc_memory_permalock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field tid_memory_write_lock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, tid_memory_write_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field tid_memory_permalock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, tid_memory_permalock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field file_0_memory_write_lock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, file_0_memory_write_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field file_0_memory_permalock
    arg_ptr      = arg_base + offsetof(struct LockCommandArgs, file_0_memory_permalock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void access_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;



    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    16 + // password
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc6, 8);
    // Adding field password
    arg_ptr      = arg_base + offsetof(struct AccessCommandArgs, password);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *((const uint16_t*)arg_ptr), 16);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void block_write_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;


    // Pull out the byte span data for for the variable length field data
    const struct BlockWriteCommandArgs* span_args = (const struct BlockWriteCommandArgs*)arg_base;
    assert(span_args->data);
    const struct BitSpan* span_data = span_args->data;
    size_t total_span_bits = span_data->length;
    if(total_span_bits > 0)
    {
        assert(span_data->data);
    }

    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    2 + // memory_bank
    get_ebv_bit_len(((const struct BlockWriteCommandArgs*)arg_base)->word_pointer, EBV83_MAX) + // word_pointer
    8 + // word_count
    total_span_bits + // data
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc7, 8);
    // Adding field memory_bank
    arg_ptr      = arg_base + offsetof(struct BlockWriteCommandArgs, memory_bank);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 2);
    // Adding field word_pointer
    arg_ptr      = arg_base + offsetof(struct BlockWriteCommandArgs, word_pointer);
    curr_bit_len = bit_pack_ebv(encoded_command, curr_bit_len, ((const uint32_t*)arg_ptr)[0], EBV83_MAX);
    // Adding field word_count
    arg_ptr      = arg_base + offsetof(struct BlockWriteCommandArgs, word_count);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 8);
    // Adding field data
    arg_ptr      = arg_base + offsetof(struct BlockWriteCommandArgs, data);
    curr_bit_len = bit_pack_from_pointer(encoded_command, curr_bit_len, span_data->data, total_span_bits);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void block_permalock_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;


    // Pull out the byte span data for for the variable length field mask
    const struct BlockPermalockCommandArgs* span_args = (const struct BlockPermalockCommandArgs*)arg_base;
    assert(span_args->mask);
    const struct BitSpan* span_data = span_args->mask;
    size_t total_span_bits = span_data->length;
    if(total_span_bits > 0)
    {
        assert(span_data->data);
    }

    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    8 + // rfu
    1 + // read_lock
    2 + // memory_bank
    get_ebv_bit_len(((const struct BlockPermalockCommandArgs*)arg_base)->block_pointer, EBV81_MAX) + // block_pointer
    8 + // block_range
    total_span_bits + // mask
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xc9, 8);
    // Adding field rfu
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0, 8);
    // Adding field read_lock
    arg_ptr      = arg_base + offsetof(struct BlockPermalockCommandArgs, read_lock);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field memory_bank
    arg_ptr      = arg_base + offsetof(struct BlockPermalockCommandArgs, memory_bank);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 2);
    // Adding field block_pointer
    arg_ptr      = arg_base + offsetof(struct BlockPermalockCommandArgs, block_pointer);
    curr_bit_len = bit_pack_ebv(encoded_command, curr_bit_len, ((const uint32_t*)arg_ptr)[0], EBV81_MAX);
    // Adding field block_range
    arg_ptr      = arg_base + offsetof(struct BlockPermalockCommandArgs, block_range);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 8);
    // Adding field mask
    arg_ptr      = arg_base + offsetof(struct BlockPermalockCommandArgs, mask);
    curr_bit_len = bit_pack_from_pointer(encoded_command, curr_bit_len, span_data->data, total_span_bits);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}

static void authenticate_command_encode(
    const void*      args,
    struct BitSpan* return_command)
{
    assert(args);
    assert(return_command);
    assert(return_command->data);

    const uint8_t* arg_base        = args;
    const uint8_t* arg_ptr         = args;
    uint8_t* encoded_command = return_command->data;


    // Pull out the byte span data for for the variable length field message
    const struct AuthenticateCommandArgs* span_args = (const struct AuthenticateCommandArgs*)arg_base;
    assert(span_args->message);
    const struct BitSpan* span_data = span_args->message;
    size_t total_span_bits = span_data->length;
    if(total_span_bits > 0)
    {
        assert(span_data->data);
    }

    // Get the total length of the command to ensure the encode buffer is completely cleared
    uint32_t final_bit_len =
    8 + // command
    2 + // rfu
    1 + // send_rep
    1 + // inc_rep_len
    8 + // csi
    12 + // length
    total_span_bits + // message
    0;
    uint8_t final_byte_len = (final_bit_len - (final_bit_len % 8))/8;
    final_byte_len += ((final_bit_len % 8) > 0) ? 1 : 0;
    memset(encoded_command, 0, final_byte_len);

    // Begin encoding
    size_t   curr_bit_len    = 0u;

    // Adding field command
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0xd5, 8);
    // Adding field rfu
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, 0, 2);
    // Adding field send_rep
    arg_ptr      = arg_base + offsetof(struct AuthenticateCommandArgs, send_rep);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field inc_rep_len
    arg_ptr      = arg_base + offsetof(struct AuthenticateCommandArgs, inc_rep_len);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 1);
    // Adding field csi
    arg_ptr      = arg_base + offsetof(struct AuthenticateCommandArgs, csi);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *arg_ptr, 8);
    // Adding field length
    arg_ptr      = arg_base + offsetof(struct AuthenticateCommandArgs, length);
    curr_bit_len = bit_pack(encoded_command, curr_bit_len, *((const uint16_t*)arg_ptr), 12);
    // Adding field message
    arg_ptr      = arg_base + offsetof(struct AuthenticateCommandArgs, message);
    curr_bit_len = bit_pack_from_pointer(encoded_command, curr_bit_len, span_data->data, total_span_bits);

    // If the final bit length is not what we expected, set the length to 0
    return_command->length = (curr_bit_len != final_bit_len) ? 0 : curr_bit_len;
}


static enum Gen2Command decode_command_type(const struct BitSpan* return_command)
{
    assert(return_command);
    assert(return_command->data);

    uint8_t four_bit_id = return_command->data[0] >> 4;
    uint8_t eight_bit_id = return_command->data[0];

    // Start by checking for the 4 bit select
    if(four_bit_id == 0xA)
    {
        return Gen2Select;
    }
    else
    {
        switch(eight_bit_id)
        {
            case 0xc2:
                return Gen2Read;
                break;
            case 0xc3:
                return Gen2Write;
                break;
            case 0xc4:
                return Gen2Kill_1;
                break;
            case 0xc5:
                return Gen2Lock;
                break;
            case 0xc6:
                return Gen2Access;
                break;
            case 0xc7:
                return Gen2BlockWrite;
                break;
            case 0xc9:
                return Gen2BlockPermalock;
                break;
            case 0xd5:
                return Gen2Authenticate;
                break;
            default:
                return _COMMAND_MAX;
            break;
        }
    }
    return _COMMAND_MAX;
}

static void select_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct SelectCommandArgs* decoded_command = (struct SelectCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 4;
    // Pulling field target
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 3);
    decoded_command->target = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 3;
    // Pulling field action
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 3);
    decoded_command->action = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 3;
    // Pulling field memory_bank
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 2);
    decoded_command->memory_bank = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 2;
    // Pulling EBV field bit_pointer
    uint8_t ebv_byte_len = ebv_length_decode(encoded_command->data, curr_bit_len);
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, ebv_byte_len*8);
    decoded_command->bit_pointer = bit_unpack_ebv(unpacked_data, ebv_byte_len);
    curr_bit_len += ebv_byte_len*8;
    // Pulling field bit_count
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 8);
    decoded_command->bit_count = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 8;
    // Pulling field mask

    // Variable bit decode uses the rest of the length -1 for truncate
    uint8_t copy_bits = encoded_command->length - curr_bit_len - 1;

    unpacked_data = bit_unpack_msb(encoded_command->data, curr_bit_len, copy_bits);
    // copy data over to the byte span
    uint8_t copy_bytes = (copy_bits - (copy_bits % 8))/8;
    copy_bytes += (copy_bits % 8) ? 1: 0;

    static uint8_t variable_buffer[50];
    memset(&variable_buffer, 0u, sizeof(variable_buffer));

    static struct BitSpan variable_data = {.data=variable_buffer, .length=0};
    // Point the variable data to this local store.
    // Note: This is overridden the next time it is used.
    decoded_command->mask = &variable_data;

    memcpy(variable_data.data, unpacked_data, copy_bytes);
    variable_data.length = copy_bits;
    curr_bit_len += copy_bits;
    // Pulling field truncate
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->truncate = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
}

static void read_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct ReadCommandArgs* decoded_command = (struct ReadCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field memory_bank
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 2);
    decoded_command->memory_bank = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 2;
    // Pulling EBV field word_pointer
    uint8_t ebv_byte_len = ebv_length_decode(encoded_command->data, curr_bit_len);
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, ebv_byte_len*8);
    decoded_command->word_pointer = bit_unpack_ebv(unpacked_data, ebv_byte_len);
    curr_bit_len += ebv_byte_len*8;
    // Pulling field word_count
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 8);
    decoded_command->word_count = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 8;
}

static void write_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct WriteCommandArgs* decoded_command = (struct WriteCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field memory_bank
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 2);
    decoded_command->memory_bank = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 2;
    // Pulling EBV field word_pointer
    uint8_t ebv_byte_len = ebv_length_decode(encoded_command->data, curr_bit_len);
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, ebv_byte_len*8);
    decoded_command->word_pointer = bit_unpack_ebv(unpacked_data, ebv_byte_len);
    curr_bit_len += ebv_byte_len*8;
    // Pulling field data
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 16);
    decoded_command->data = ((uint16_t*)unpacked_data)[0];
    curr_bit_len += 16;
}

static void kill_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct KillCommandArgs* decoded_command = (struct KillCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field password
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 16);
    decoded_command->password = ((uint16_t*)unpacked_data)[0];
    curr_bit_len += 16;
    // Skip rfu in decode
    curr_bit_len += 3;
}

static void lock_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct LockCommandArgs* decoded_command = (struct LockCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field kill_password_read_write_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->kill_password_read_write_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field kill_password_permalock_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->kill_password_permalock_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field access_password_read_write_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->access_password_read_write_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field access_password_permalock_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->access_password_permalock_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field epc_memory_write_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->epc_memory_write_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field epc_memory_permalock_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->epc_memory_permalock_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field tid_memory_write_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->tid_memory_write_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field tid_memory_permalock_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->tid_memory_permalock_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field file_0_memory_write_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->file_0_memory_write_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field file_0_memory_permalock_mask
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->file_0_memory_permalock_mask = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field kill_password_read_write_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->kill_password_read_write_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field kill_password_permalock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->kill_password_permalock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field access_password_read_write_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->access_password_read_write_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field access_password_permalock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->access_password_permalock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field epc_memory_write_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->epc_memory_write_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field epc_memory_permalock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->epc_memory_permalock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field tid_memory_write_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->tid_memory_write_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field tid_memory_permalock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->tid_memory_permalock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field file_0_memory_write_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->file_0_memory_write_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field file_0_memory_permalock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->file_0_memory_permalock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
}

static void access_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct AccessCommandArgs* decoded_command = (struct AccessCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field password
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 16);
    decoded_command->password = ((uint16_t*)unpacked_data)[0];
    curr_bit_len += 16;
}

static void block_write_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct BlockWriteCommandArgs* decoded_command = (struct BlockWriteCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Pulling field memory_bank
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 2);
    decoded_command->memory_bank = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 2;
    // Pulling EBV field word_pointer
    uint8_t ebv_byte_len = ebv_length_decode(encoded_command->data, curr_bit_len);
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, ebv_byte_len*8);
    decoded_command->word_pointer = bit_unpack_ebv(unpacked_data, ebv_byte_len);
    curr_bit_len += ebv_byte_len*8;
    // Pulling field word_count
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 8);
    decoded_command->word_count = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 8;
    // Pulling field data

    // Variable bit decode uses the rest of the length -1 for truncate
    uint8_t copy_bits = encoded_command->length - curr_bit_len;

    unpacked_data = bit_unpack_msb(encoded_command->data, curr_bit_len, copy_bits);
    // copy data over to the byte span
    uint8_t copy_bytes = (copy_bits - (copy_bits % 8))/8;
    copy_bytes += (copy_bits % 8) ? 1: 0;

    static uint8_t variable_buffer[50];
    memset(&variable_buffer, 0u, sizeof(variable_buffer));

    static struct BitSpan variable_data = {.data=variable_buffer, .length=0};
    // Point the variable data to this local store.
    // Note: This is overridden the next time it is used.
    decoded_command->data = &variable_data;

    memcpy(variable_data.data, unpacked_data, copy_bytes);
    variable_data.length = copy_bits;
    curr_bit_len += copy_bits;
}

static void block_permalock_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct BlockPermalockCommandArgs* decoded_command = (struct BlockPermalockCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Skip rfu in decode
    curr_bit_len += 8;
    // Pulling field read_lock
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->read_lock = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field memory_bank
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 2);
    decoded_command->memory_bank = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 2;
    // Pulling EBV field block_pointer
    uint8_t ebv_byte_len = ebv_length_decode(encoded_command->data, curr_bit_len);
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, ebv_byte_len*8);
    decoded_command->block_pointer = bit_unpack_ebv(unpacked_data, ebv_byte_len);
    curr_bit_len += ebv_byte_len*8;
    // Pulling field block_range
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 8);
    decoded_command->block_range = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 8;
    // Pulling field mask

    // Variable bit decode uses the rest of the length -1 for truncate
    uint8_t copy_bits = encoded_command->length - curr_bit_len;

    unpacked_data = bit_unpack_msb(encoded_command->data, curr_bit_len, copy_bits);
    // copy data over to the byte span
    uint8_t copy_bytes = (copy_bits - (copy_bits % 8))/8;
    copy_bytes += (copy_bits % 8) ? 1: 0;

    static uint8_t variable_buffer[50];
    memset(&variable_buffer, 0u, sizeof(variable_buffer));

    static struct BitSpan variable_data = {.data=variable_buffer, .length=0};
    // Point the variable data to this local store.
    // Note: This is overridden the next time it is used.
    decoded_command->mask = &variable_data;

    memcpy(variable_data.data, unpacked_data, copy_bytes);
    variable_data.length = copy_bits;
    curr_bit_len += copy_bits;
}

static void authenticate_command_decode(
    void*      decoded_args,
    const struct BitSpan* encoded_command)
{
    assert(decoded_args);
    assert(encoded_command);
    assert(encoded_command->data);

    struct AuthenticateCommandArgs* decoded_command = (struct AuthenticateCommandArgs*) decoded_args;
    size_t   curr_bit_len    = 0;
    uint8_t* unpacked_data;
    // Skip command ID in decode
    curr_bit_len += 8;
    // Skip rfu in decode
    curr_bit_len += 2;
    // Pulling field send_rep
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->send_rep = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field inc_rep_len
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 1);
    decoded_command->inc_rep_len = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 1;
    // Pulling field csi
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 8);
    decoded_command->csi = ((uint8_t*)unpacked_data)[0];
    curr_bit_len += 8;
    // Pulling field length
    unpacked_data = bit_unpack(encoded_command->data, curr_bit_len, 12);
    decoded_command->length = ((uint16_t*)unpacked_data)[0];
    curr_bit_len += 12;
    // Pulling field message

    // Variable bit decode uses the rest of the length -1 for truncate
    uint8_t copy_bits = encoded_command->length - curr_bit_len;

    unpacked_data = bit_unpack_msb(encoded_command->data, curr_bit_len, copy_bits);
    // copy data over to the byte span
    uint8_t copy_bytes = (copy_bits - (copy_bits % 8))/8;
    copy_bytes += (copy_bits % 8) ? 1: 0;

    static uint8_t variable_buffer[50];
    memset(&variable_buffer, 0u, sizeof(variable_buffer));

    static struct BitSpan variable_data = {.data=variable_buffer, .length=0};
    // Point the variable data to this local store.
    // Note: This is overridden the next time it is used.
    decoded_command->message = &variable_data;

    memcpy(variable_data.data, unpacked_data, copy_bytes);
    variable_data.length = copy_bits;
    curr_bit_len += copy_bits;
}
// IPJ_autogen }
// clang-format on

static void (*gen2_command_encode_table[])(const void*, struct BitSpan*) = {
    select_command_encode,
    read_command_encode,
    write_command_encode,
    kill_command_encode,
    kill_command_encode,
    lock_command_encode,
    access_command_encode,
    block_write_command_encode,
    block_permalock_command_encode,
    authenticate_command_encode,
};

static bool encode_gen2_command(const struct Gen2CommandSpec* cmd_spec,
                                struct BitSpan*               encoded_command)
{
    assert(cmd_spec);
    assert(cmd_spec->args);
    assert(encoded_command);

    // Ensure the command is supported
    if (cmd_spec->command == _COMMAND_MAX)
    {
        return false;
    }
    // The command will map to the encoder function pointer
    gen2_command_encode_table[cmd_spec->command](cmd_spec->args,
                                                 encoded_command);
    return true;
}

static void (*gen2_command_decode_table[])(void*, const struct BitSpan*) = {
    select_command_decode,
    read_command_decode,
    write_command_decode,
    kill_command_decode,
    kill_command_decode,
    lock_command_decode,
    access_command_decode,
    block_write_command_decode,
    block_permalock_command_decode,
    authenticate_command_decode,
};

static bool decode_gen2_command(struct Gen2CommandSpec* cmd_spec,
                                const struct BitSpan*   encoded_command)
{
    assert(cmd_spec);
    assert(cmd_spec->args);
    assert(encoded_command);

    // Find the type of command from the encoded function
    cmd_spec->command = decode_command_type(encoded_command);
    if (cmd_spec->command >= _COMMAND_MAX)
    {
        return false;
    }
    // Pull out the args based on the command type
    gen2_command_decode_table[cmd_spec->command](cmd_spec->args,
                                                 encoded_command);
    return true;
}

static bool get_gen2_tx_control_config(
    const struct Gen2CommandSpec* cmd_spec,
    struct Gen2TxnControlsFields* txn_control)
{
    assert(cmd_spec);
    assert(cmd_spec->args);

    if (cmd_spec->command >= _COMMAND_MAX)
    {
        return false;
    }

    /* Look up in transaction_configs table. */
    struct Gen2TxnControlsFields txn_config =
        transaction_configs[cmd_spec->command];

    /* Special handling for various commands */
    if (cmd_spec->command == Gen2Read)
    {
        /* RxLength changes based on the number of words specified */
        struct ReadCommandArgs const* args =
            (struct ReadCommandArgs const*)(cmd_spec->args);
        txn_config.rx_length += (args->word_count * 16u);
    }
    else if (cmd_spec->command == Gen2BlockPermalock)
    {
        struct BlockPermalockCommandArgs const* args =
            (struct BlockPermalockCommandArgs const*)(cmd_spec->args);

        /* 2 different responses possible based on the read_lock option */
        if (args->read_lock == Read)
        {
            /* rx_length changes based on the number of words specified */
            txn_config.rx_length += args->block_range * 16u;

            txn_config.response_type = Immediate;
        }
        else if (args->read_lock == Permalock)
        {
            txn_config.response_type = Delayed;
        }
    }
    else if (cmd_spec->command == Gen2Authenticate)
    {
        struct AuthenticateCommandArgs const* args =
            (struct AuthenticateCommandArgs const*)(cmd_spec->args);

        if (args->send_rep)
        {
            txn_config.rx_length += args->rep_len_bits;

            if (args->inc_rep_len)
            {
                txn_config.rx_length += 16;
            }
        }
    }

    // Copy the data to the user struct
    *txn_control = txn_config;

    return true;
}

static void general_reply_decode(uint16_t          num_bits,
                                 const uint8_t*    data,
                                 struct Gen2Reply* reply)
{
    assert(data);
    assert(reply);

    size_t word_count = (num_bits - (num_bits % 16)) / 16;
    word_count += (num_bits % 16) ? 1 : 0;
    uint16_t* decoded_word = reply->data;

    for (size_t iter = 0u; iter < word_count; iter++)
    {
        *decoded_word = *(data++) << 8u;
        *decoded_word++ |= *(data++);
    }
}

static bool decode_reply(enum Gen2Command              command,
                         const struct EventFifoPacket* gen2_pkt,
                         struct Gen2Reply*             decoded_reply)
{
    assert(gen2_pkt);
    assert(decoded_reply);
    assert(decoded_reply->data);

    uint16_t num_bits = gen2_pkt->static_data->gen2_transaction.num_bits;

    decoded_reply->reply      = command;
    decoded_reply->error_code = NoError;
    decoded_reply->transaction_status =
        gen2_pkt->static_data->gen2_transaction.status;

    if (decoded_reply->transaction_status != Gen2TransactionStatusOk)
    {
        return false;
    }

    // Ensure this is a supported command and check if it contains an error
    // header
    bool reply_has_error_header = false;
    switch (command)
    {
        case Gen2Read:
        case Gen2BlockPermalock:
        case Gen2Write:
        case Gen2Kill_2:
        case Gen2Lock:
        case Gen2BlockWrite:
            reply_has_error_header = true;
            break;
        case Gen2Authenticate:
        case Gen2Kill_1:
        case Gen2Access:
            reply_has_error_header = false;
            break;
        case Gen2Select:    // not supported for decode
        case _COMMAND_MAX:  // not supported for decode
        default:
            fprintf(stderr, "No known decoder for command %u\n", command);
            return false;
    }

    // Check header bit for errors
    if (reply_has_error_header)
    {
        // There will always be 1 bit for the error header and at
        // least one more byte. If there is an error shown in the
        // first bit, then the next byte is the error that occurred.
        // If there is not, there will always be at least one more
        // byte of data.
        if (num_bits < 9)
        {
            return false;
        }
        bool header_error = gen2_pkt->dynamic_data[0] & 0x01;


        if (header_error)
        {
            decoded_reply->error_code = gen2_pkt->dynamic_data[1];
            if (decoded_reply->error_code != NoError)
            {
                return false;
            }
        }

        // subtract header bit
        // Note: does not remove 9 bits in the case of and error
        // since we just return.
        num_bits -= 1;
    }

    // No error and a proper command. Now parse the data
    const uint8_t* data_after_header = (reply_has_error_header)
                                           ? &gen2_pkt->dynamic_data[1]
                                           : &gen2_pkt->dynamic_data[0];

    // Special case for  in process replies
    bool is_in_process_reply = (command == Gen2Authenticate) ? true : false;
    if (is_in_process_reply)
    {
        /*
         * The reply is an In-process Tag reply
         * Add 7 bits for the barker code
         */
        num_bits += 7;
    }

    general_reply_decode(num_bits, data_after_header, decoded_reply);
    return true;
}

struct Ex10Gen2Commands const* get_ex10_gen2_commands(void)
{
    static struct Ex10Gen2Commands gen2_commands_instance = {
        .encode_gen2_command        = encode_gen2_command,
        .decode_gen2_command        = decode_gen2_command,
        .decode_reply               = decode_reply,
        .get_gen2_tx_control_config = get_gen2_tx_control_config,
    };

    return &gen2_commands_instance;
}
