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

#include "ex10_api/commands.h"
#include "board/board_spec.h"
#include "ex10_api/byte_span.h"
#include "ex10_api/command_transactor.h"
#include "ex10_api/event_packet_parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


/**
 * The Ex10 API commands
 *
 * This file implements the commands found in the ex10_api documentation.
 *
 * It sits directly above the connection to the device, executes commands
 * from the client and returns data and responses from the device.
 *
 * All inputs and returns are bytes (or pointers to same) at this layer
 * of the design.
 */

/**
 * @struct RegOffset
 * Contains the register offset result when calling get_reg_offset().
 */
struct RegOffset
{
    size_t index;   ///< The register within the list being accessd.
    size_t offset;  ///< The byte offset within the register being accessed.
};

/**
 * Given a list of registers of various lengths, get the offset into the
 * register being accessed based on the offset parameter.
 *
 * @example A list of 3 registers
 * Register 1 -> length = 7 bytes
 * Register 2 -> length = 16 bytes
 * Register 3 -> length = 9 bytes
 *
 * For param offset =  9, return RegOffset{.index = 1, .offset =  2}
 * For param offset = 26, return RegOffset{.index = 2, .offset =  3}
 * For param offset = 50, return RegOffset{.index = 3, .offset = 18}
 * @note For the offset of 50, which is above the size of all the registers
 * combined, the returned RegOffset shows an index of one greater than your max
 * register index, and an offset of the remainder of the bytes.
 *
 * @param offset   The number of bytes into the list of registers
 * @param max_regs The maximum number of registers to iterate over
 * @param reg_list A list of registers being accessed.
 * @note  The registers are not necessarily located in a contiguous manner.
 *
 * @return struct RegOffset The register struct containing the register and
 *                          byte offset being accessed within reg_list[].
 */
struct RegOffset get_reg_offset(size_t                           offset,
                                size_t                           max_regs,
                                const struct RegisterInfo* const reg_list[])
{
    size_t curr_idx = 0;
    while (curr_idx < max_regs && (reg_list[curr_idx]->length *
                                   reg_list[curr_idx]->num_entries) <= offset)
    {
        offset -= reg_list[curr_idx]->length * reg_list[curr_idx]->num_entries;
        curr_idx++;
    }
    return (struct RegOffset){.index = curr_idx, .offset = offset};
}

static struct Ex10CommandsHostErrors get_no_command_errors(void)
{
    return (struct Ex10CommandsHostErrors){
        .error_occurred  = false,
        .host_result     = HostCommandsSuccess,
        .device_response = Success,
    };
}

/**
 * Perform parameter checking the struct RegisterInfo and data parameters
 * to be used for read/write operations.
 *
 * @param reg_list  A list of registers to be used in read/write commands.
 * @param byte_span A data pointer to be used in read/write commands.
 *
 * @return struct Ex10CommandsHostErrors The resulting command error struct.
 */
struct Ex10CommandsHostErrors get_span_valid(
    const struct RegisterInfo* const reg_list,
    const void*                      byte_span)
{
    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    if (NULL == byte_span || NULL == reg_list)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsNullData;
        return command_errors;
    }
    // Ensure none of the addresses overflow
    else if ((reg_list->address + (reg_list->length * reg_list->num_entries)) >
             UINT16_MAX)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsOverMaxDeviceAddress;
        return command_errors;
    }
    return command_errors;
}

static struct Ex10CommandsHostErrors command_read(
    struct RegisterInfo const* const reg_list[],
    struct ByteSpan*                 byte_spans[],
    size_t                           segment_count,
    uint32_t                         ready_n_timeout_ms)
{
    assert(reg_list);
    assert(byte_spans);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    if (segment_count <= 0)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadNumSpans;
        return command_errors;
    }

    uint8_t payload[EX10_SPI_BURST_SIZE];

    size_t payload_length     = 0;
    payload[payload_length++] = CommandRead;

    // NOTE: This starts at 1 for the command response byte.
    size_t  response_length  = 1;
    uint8_t read_header_size = sizeof(struct Ex10ReadFormat);

    // The number of bytes that have been read
    size_t curr_bytes_read = 0;

    size_t total_bytes_to_read = 0;
    for (size_t iter = 0; iter < segment_count; iter++)
    {
        command_errors = get_span_valid(reg_list[iter], byte_spans[iter]->data);
        if (command_errors.error_occurred)
        {
            return command_errors;
        }
        total_bytes_to_read +=
            reg_list[iter]->length * reg_list[iter]->num_entries;
    }

    while (curr_bytes_read < total_bytes_to_read)
    {
        // If we can only fit the next header, send and start a new read
        bool const all_regs_inserted =
            curr_bytes_read + response_length > total_bytes_to_read;
        bool const tx_buffer_full =
            (payload_length + read_header_size) >= EX10_SPI_BURST_SIZE;
        bool const rx_buffer_full = response_length >= EX10_SPI_BURST_SIZE;

        if (all_regs_inserted || rx_buffer_full || tx_buffer_full)
        {
            uint8_t      curr_data[response_length];
            size_t const received =
                get_ex10_command_transactor()->send_and_recv_bytes(
                    payload,
                    payload_length,
                    curr_data,
                    response_length,
                    ready_n_timeout_ms);
            command_errors.device_response = (enum ResponseCode)curr_data[0];

            if (curr_data[0] != Success)
            {
                command_errors.error_occurred = true;
                return command_errors;
            }
            else if (received != response_length)
            {
                command_errors.error_occurred = true;
                command_errors.host_result =
                    HostCommandsRecievedLengthIncorrect;
                return command_errors;
            }
            else
            {
                // NOTE: This starts at 1 for the command response byte.
                size_t src_offset = 1;
                while (src_offset < response_length)
                {
                    struct RegOffset const reg_offset = get_reg_offset(
                        curr_bytes_read, segment_count, reg_list);
                    // Copy over the data reg by reg. Note the copy can start
                    // mid register
                    size_t reg_bytes_left =
                        (reg_list[reg_offset.index]->length *
                         reg_list[reg_offset.index]->num_entries) -
                        reg_offset.offset;
                    size_t bytes_for_data = EX10_SPI_BURST_SIZE - 1;
                    size_t copy_bytes     = (reg_bytes_left > bytes_for_data)
                                            ? bytes_for_data
                                            : reg_bytes_left;

                    memcpy(&(byte_spans[reg_offset.index]
                                 ->data[reg_offset.offset]),
                           &curr_data[src_offset],
                           copy_bytes);
                    curr_bytes_read += copy_bytes;
                    src_offset += copy_bytes;
                }
            }
            payload_length  = 1;
            response_length = 1;
        }
        else
        {
            // there is room for at least another header and some bytes
            // determine the current register from the offset
            // NOTE: we subtract 1 from the response length for the command
            // response byte. We only care about the data bytes.
            size_t const curr_offset = curr_bytes_read + response_length - 1;
            struct RegOffset const reg_offset =
                get_reg_offset(curr_offset, segment_count, reg_list);

            struct RegisterInfo const* curr_reg = reg_list[reg_offset.index];

            // set the address in the payload
            uint16_t const curr_address = curr_reg->address + reg_offset.offset;
            memcpy(&payload[payload_length],
                   &curr_address,
                   sizeof(curr_reg->address));
            payload_length += sizeof(curr_reg->address);

            // if there is not enough room for the entire reg read, read part of
            // it if we previously read part of the reg, read the later part
            uint16_t const data_bytes_allowed =
                EX10_SPI_BURST_SIZE - response_length;

            uint16_t const reg_bytes =
                (curr_reg->num_entries * curr_reg->length) - reg_offset.offset;
            uint16_t const copy_bytes = (reg_bytes > data_bytes_allowed)
                                            ? data_bytes_allowed
                                            : reg_bytes;

            // set the length in the payload
            memcpy(&payload[payload_length],
                   &copy_bytes,
                   sizeof(curr_reg->length));
            payload_length += sizeof(curr_reg->length);

            // plan for the read data
            response_length += copy_bytes;
        }
    }
    // Set the length of the spans now that everything is read in
    for (size_t iter = 0; iter < segment_count; iter++)
    {
        byte_spans[iter]->length =
            reg_list[iter]->length * reg_list[iter]->num_entries;
    }
    return command_errors;
}

static struct Ex10CommandsHostErrors command_test_read(uint32_t address,
                                                       uint16_t length_in_bytes,
                                                       void*    read_buffer)
{
    assert(read_buffer);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    // Response starts as one command byte
    size_t response_length = 1;

    // ensure length_in_bytes fits
    // Ensure response to current segment will fit in response buffer
    if ((address + length_in_bytes) > UINT32_MAX ||
        (response_length + length_in_bytes) > EX10_SPI_BURST_SIZE)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadCommandedLength;
        return command_errors;
    }

    // Place the command in the output
    uint8_t payload[EX10_SPI_BURST_SIZE];
    payload[0]            = CommandTestRead;
    size_t payload_length = 1;

    // The TestRead command takes in chunks of 4 bytes.
    // If the address or length are misaligned then Ex10 will fail the command.
    if (address % 4u != 0u || length_in_bytes % 4u != 0u)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommands32BitAlignment;
        return command_errors;
    }

    uint16_t u32_words_read = length_in_bytes / 4;

    // put the u32 length and address into the output
    payload[5] = u32_words_read;
    payload[6] = u32_words_read >> 8;
    payload[1] = address;
    payload[2] = address >> 8;
    payload[3] = address >> 16;
    payload[4] = address >> 24;
    payload_length += sizeof(struct Ex10TestReadFormat);

    // send the command and read in the response
    response_length += length_in_bytes;
    uint8_t curr_data[response_length];
    size_t  received = get_ex10_command_transactor()->send_and_recv_bytes(
        payload,
        payload_length,
        curr_data,
        response_length,
        NOMINAL_READY_N_TIMEOUT_MS);
    assert(received == response_length);
    // Check the response code

    command_errors.device_response = (enum ResponseCode)curr_data[0];
    if (curr_data[0] != Success)
    {
        command_errors.error_occurred = true;
        return command_errors;
    }

    // Copy the return into the output buffer
    memcpy(read_buffer, &curr_data[1], length_in_bytes);

    return command_errors;
}

static void extend_write_buffer(uint16_t    address,
                                uint16_t    length,
                                uint8_t*    cmd_buffer,
                                void const* buffer)
{
    assert(cmd_buffer);
    assert(buffer);

    size_t address_plus_length_size = 4;
    cmd_buffer[1]                   = (uint8_t)(address >> 8);
    cmd_buffer[0]                   = (uint8_t)address;
    cmd_buffer[3]                   = (uint8_t)(length >> 8);
    cmd_buffer[2]                   = (uint8_t)length;
    memcpy(&cmd_buffer[address_plus_length_size], buffer, length);
}

static struct Ex10CommandsHostErrors command_write(
    const struct RegisterInfo* const reg_list[],
    struct ConstByteSpan const*      byte_spans[],
    size_t                           segment_count,
    uint32_t                         ready_n_timeout_ms)
{
    assert(reg_list);
    assert(byte_spans);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    if (segment_count == 0)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadNumSpans;
        return command_errors;
    }

    size_t total_bytes_to_write = 0;
    for (size_t iter = 0; iter < segment_count; iter++)
    {
        command_errors = get_span_valid(reg_list[iter], byte_spans[iter]->data);
        if (command_errors.error_occurred)
        {
            return command_errors;
        }
        total_bytes_to_write +=
            reg_list[iter]->length * reg_list[iter]->num_entries;
    }

    uint8_t payload[EX10_SPI_BURST_SIZE];
    payload[0]                    = CommandWrite;
    size_t  payload_length        = 1;
    size_t  curr_bytes_written    = 0;
    size_t  data_bytes_in_payload = 0;
    uint8_t write_header_size     = 4;

    while (curr_bytes_written < total_bytes_to_write)
    {
        bool const all_regs_inserted =
            curr_bytes_written + data_bytes_in_payload >= total_bytes_to_write;
        // If we can only fit the next header, send and start a new write
        bool const tx_buffer_full =
            (payload_length + write_header_size) >= EX10_SPI_BURST_SIZE;
        if (all_regs_inserted || tx_buffer_full)
        {
            get_ex10_command_transactor()->send_command(
                payload, payload_length, ready_n_timeout_ms);
            payload_length = 1;
            curr_bytes_written += data_bytes_in_payload;
            data_bytes_in_payload = 0;
        }
        else
        {
            size_t const curr_offset =
                curr_bytes_written + data_bytes_in_payload;
            struct RegOffset const reg_offset =
                get_reg_offset(curr_offset, segment_count, reg_list);

            struct RegisterInfo const* curr_reg = reg_list[reg_offset.index];

            // the address to write
            uint16_t write_address = curr_reg->address + reg_offset.offset;

            // the length to write
            // if there is not enough room to write the header and the entire
            // register, write part of it
            uint16_t bytes_for_data =
                EX10_SPI_BURST_SIZE - payload_length - write_header_size;
            uint16_t reg_bytes =
                (curr_reg->length * curr_reg->num_entries) - reg_offset.offset;
            uint16_t copy_bytes =
                (reg_bytes > bytes_for_data) ? bytes_for_data : reg_bytes;

            // set the header and data in the payload
            extend_write_buffer(
                write_address,
                copy_bytes,
                &payload[payload_length],
                &(byte_spans[reg_offset.index]->data[reg_offset.offset]));

            // increment past the header and the inserted data
            payload_length += sizeof(curr_reg->address);
            payload_length += sizeof(curr_reg->length);
            payload_length += copy_bytes;

            // keep track of the data bytes copied in without the extra
            // specifiers
            data_bytes_in_payload += copy_bytes;
        }
    }
    return command_errors;
}

static struct Ex10CommandsHostErrors command_read_fifo(
    enum FifoSelection fifo_select,
    struct ByteSpan*   bytes)
{
    assert(bytes);
    assert(bytes->data);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    // The byte that follows byte[0] must be 32-bit aligned.
    // byte[0] will contain the initial response code.
    // byte[1] will contain the first byte of event packet data.
    if ((uintptr_t)bytes->data % sizeof(uint32_t) != 0u)
    {
        fprintf(stderr,
                "error: %s: Passed address of %p was not 32 bit aligned\n",
                __func__,
                bytes->data);
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommands32BitAlignment;
        return command_errors;
    }

    size_t   fifo_bytes_remaining = bytes->length;
    uint8_t* data_ptr             = bytes->data;
    bytes->length                 = 0;
    while (fifo_bytes_remaining > 0u)
    {
        // The EX10_SPI_BURST_SIZE is also the maximum response length
        // with the response code byte. i.e. The total available bytes in
        // the response can be one byte more than the maximum fifo_len.
        uint16_t const fifo_len =
            (fifo_bytes_remaining > EX10_SPI_BURST_SIZE - 1)
                ? EX10_SPI_BURST_SIZE - 1
                : fifo_bytes_remaining;
        uint16_t const resp_len = fifo_len + 1u;

        uint8_t const command[1u + sizeof(struct Ex10ReadFifoFormat)] = {
            (uint8_t)CommandReadFifo,
            (uint8_t)fifo_select,
            (uint8_t)(fifo_len >> 0u),
            (uint8_t)(fifo_len >> 8u)};
        get_ex10_command_transactor()->send_command(
            command, sizeof(command), NOMINAL_READY_N_TIMEOUT_MS);

        // The result code from the response will overwrite the last byte of
        // the last fifo packet read into the buffer.
        // Record its value so that it can be restored.
        uint8_t const restore_byte = *(--data_ptr);

        size_t const response_length =
            get_ex10_command_transactor()->receive_response(
                data_ptr, resp_len, NOMINAL_READY_N_TIMEOUT_MS);

        command_errors.device_response = (enum ResponseCode)(*data_ptr);
        if (*data_ptr != Success)
        {
            command_errors.error_occurred = true;
            return command_errors;
        }

        if (response_length != resp_len)
        {
            fprintf(stderr,
                    "error: %s: Length read back was: %zd, length expected "
                    "was: %d\n",
                    __func__,
                    response_length,
                    resp_len);
            command_errors.error_occurred = true;
            command_errors.host_result    = HostCommandsRecievedLengthIncorrect;
            return command_errors;
        }

        *data_ptr++ = restore_byte;

        // The next data_ptr position will transfer the response code into
        // the last byte of the last response byte transferred.
        data_ptr += fifo_len;
        fifo_bytes_remaining -= fifo_len;
        bytes->length += fifo_len;
    }
    return command_errors;
}

/**
 * @details
 * The WriteInfoPage command format:
 *
 *   Offset |  Size | Description
 *   -------|-------|------------
 *   0      | 1     | Command Code
 *   1      | 1     | PageId
 *   2      | N     | Image Data
 *   N + 2  | 2     | CRC-16
 *
 * The total command length = image_data->length + 4
 */
static struct Ex10CommandsHostErrors command_write_info_page(
    uint8_t                     page_id,
    const struct ConstByteSpan* image_data,
    uint16_t                    crc16)
{
    assert(image_data);
    assert(image_data->data);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    // We subtract 4 bytes for the command code, page id, and the crc. The rest
    // is left for the image.
    if (image_data->length > (EX10_BOOTLOADER_MAX_COMMAND_SIZE - 4u))
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadCommandedLength;
        return command_errors;
    }

    uint8_t payload[image_data->length + 4u];
    payload[0] = CommandWriteInfoPage;
    payload[1] = page_id;
    memcpy(&payload[2], image_data->data, image_data->length);
    payload[2 + image_data->length]     = (uint8_t)crc16;
    payload[2 + image_data->length + 1] = (uint8_t)(crc16 >> 8u);

    size_t  resp_len = 1;
    uint8_t resp_data[resp_len];
    get_ex10_command_transactor()->send_and_recv_bytes(
        payload,
        sizeof(payload),
        resp_data,
        resp_len,
        NOMINAL_READY_N_TIMEOUT_MS);

    command_errors.device_response = (enum ResponseCode)resp_data[0];
    if (resp_data[0] != Success)
    {
        command_errors.error_occurred = true;
    }
    return command_errors;
}

static struct Ex10CommandsHostErrors command_start_upload(
    uint8_t                     code,
    const struct ConstByteSpan* image_data)
{
    assert(image_data);
    assert(image_data->data);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    // -1 is from the destination byte passed as part of the command
    if (image_data->length >= (EX10_MAX_IMAGE_CHUNK_SIZE - 1))
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadCommandedLength;
        return command_errors;
    }

    uint8_t payload[image_data->length + 2];
    payload[0] = CommandStartUpload;
    payload[1] = code;
    memcpy(&payload[2], image_data->data, image_data->length);
    get_ex10_command_transactor()->send_command(
        payload, sizeof(payload), NOMINAL_READY_N_TIMEOUT_MS);

    return command_errors;
}

static struct Ex10CommandsHostErrors command_continue_upload(
    const struct ConstByteSpan* image_data)
{
    assert(image_data);
    assert(image_data->data);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    if (image_data->length >= EX10_MAX_IMAGE_CHUNK_SIZE)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadCommandedLength;
        return command_errors;
    }

    uint8_t payload[image_data->length + 1];
    payload[0] = CommandContinueUpload;
    memcpy(&payload[1], image_data->data, image_data->length);
    get_ex10_command_transactor()->send_command(
        payload, sizeof(payload), NOMINAL_READY_N_TIMEOUT_MS);

    return command_errors;
}

static void command_complete_upload(void)
{
    uint8_t payload = CommandCompleteUpload;
    get_ex10_command_transactor()->send_command(
        &payload, sizeof(payload), NOMINAL_READY_N_TIMEOUT_MS);
}

static void command_revalidate_main_image(void)
{
    uint8_t payload = CommandReValidateMainImage;
    get_ex10_command_transactor()->send_command(
        &payload, sizeof(payload), NOMINAL_READY_N_TIMEOUT_MS);
}

static void command_reset(enum Status destination)
{
    uint8_t payload[2] = {CommandReset, destination};
    get_ex10_command_transactor()->send_command(
        payload, sizeof(payload), NOMINAL_READY_N_TIMEOUT_MS);
}

static struct Ex10CommandsHostErrors test_transfer(
    struct ConstByteSpan const* send,
    struct ByteSpan*            recv,
    bool                        verify)
{
    ASSERT(send);
    ASSERT(send->data);
    ASSERT(recv->data);

    struct Ex10CommandsHostErrors command_errors = get_no_command_errors();

    if (send->length >= EX10_SPI_BURST_SIZE)
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsBadCommandedLength;
        return command_errors;
    }

    size_t  transfer_length = send->length + 1;
    uint8_t payload[transfer_length];
    payload[0] = CommandTestTransfer;
    memcpy(&payload[1], send->data, send->length);
    get_ex10_command_transactor()->send_command(
        payload, transfer_length, NOMINAL_READY_N_TIMEOUT_MS);

    uint8_t response[transfer_length];
    size_t  response_length = get_ex10_command_transactor()->receive_response(
        response, transfer_length, NOMINAL_READY_N_TIMEOUT_MS);
    if (verify && (response_length != transfer_length))
    {
        command_errors.error_occurred = true;
        command_errors.host_result    = HostCommandsRecievedLengthIncorrect;
        return command_errors;
    }

    command_errors.device_response = (enum ResponseCode)response[0];
    if (response[0] == Success)
    {
        recv->length = response_length - 1;
        memcpy(recv->data, &response[1], recv->length);
    }
    else
    {
        command_errors.error_occurred = true;
        return command_errors;
    }

    if (verify)
    {
        for (size_t i = 0; i < send->length; i++)
        {
            uint8_t expected = (send->data[i] + i) & 0xFF;
            if (expected != recv->data[i])
            {
                ERRMSG_FAIL(
                    "Received '%0X' expected '%0X'\n", recv->data[i], expected);
                command_errors.host_result =
                    HostCommandsTestTransferVerifyError;
                return command_errors;
            }
        }
    }
    return command_errors;
}

static void create_fifo_event(struct EventFifoPacket const* event_packet,
                              uint8_t*                      command_buffer,
                              size_t const                  padding_bytes,
                              size_t const                  packet_bytes)
{
    assert(event_packet);

    uint8_t* command_iter = command_buffer;

    struct PacketHeader packet_header =
        get_ex10_event_parser()->make_packet_header(event_packet->packet_type);

    packet_header.packet_length = packet_bytes / sizeof(uint32_t);

    memcpy(command_iter, &packet_header, sizeof(packet_header));
    command_iter += sizeof(packet_header);

    memcpy(command_iter,
           event_packet->static_data,
           event_packet->static_data_length);
    command_iter += event_packet->static_data_length;

    memcpy(command_iter,
           event_packet->dynamic_data,
           event_packet->dynamic_data_length);
    command_iter += event_packet->dynamic_data_length;

    memset(command_iter, 0, padding_bytes);
    command_iter += padding_bytes;
}

static void command_insert_fifo_event(
    const bool                    trigger_irq,
    struct EventFifoPacket const* event_packet)
{
    if (event_packet == NULL)
    {
        uint8_t const command_buffer[] = {CommandInsertFifoEvent, trigger_irq};
        get_ex10_command_transactor()->send_command(
            command_buffer, sizeof(command_buffer), NOMINAL_READY_N_TIMEOUT_MS);
        return;
    }

    size_t const event_bytes = sizeof(struct PacketHeader) +
                               event_packet->static_data_length +
                               event_packet->dynamic_data_length;

    size_t const padding_bytes =
        (sizeof(uint32_t) - event_bytes % sizeof(uint32_t)) % sizeof(uint32_t);

    size_t const packet_bytes = event_bytes + padding_bytes;

    // The buffer size is the size of the packet to insert +1 for the trigger
    // byte +1 for the command code
    size_t const command_buffer_size = packet_bytes + 2u;

    uint8_t command_buffer[command_buffer_size];
    command_buffer[0] = CommandInsertFifoEvent;
    command_buffer[1] = trigger_irq;

    create_fifo_event(
        event_packet, &command_buffer[2], padding_bytes, packet_bytes);

    get_ex10_command_transactor()->send_command(
        command_buffer, sizeof(command_buffer), NOMINAL_READY_N_TIMEOUT_MS);
}

static const struct Ex10Commands ex10_commands = {
    .read                  = command_read,
    .test_read             = command_test_read,
    .extend_write_buffer   = extend_write_buffer,
    .write                 = command_write,
    .read_fifo             = command_read_fifo,
    .write_info_page       = command_write_info_page,
    .start_upload          = command_start_upload,
    .continue_upload       = command_continue_upload,
    .complete_upload       = command_complete_upload,
    .revalidate_main_image = command_revalidate_main_image,
    .reset                 = command_reset,
    .test_transfer         = test_transfer,
    .create_fifo_event     = create_fifo_event,
    .insert_fifo_event     = command_insert_fifo_event,
};

struct Ex10Commands const* get_ex10_commands(void)
{
    return &ex10_commands;
}
