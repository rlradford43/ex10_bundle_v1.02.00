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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ex10_api/application_register_definitions.h"
#include "ex10_api/application_register_field_enums.h"
#include "ex10_api/byte_span.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/gcov_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/// The maximum length of a Ex10 bootloader command and response.
#define EX10_BOOTLOADER_MAX_COMMAND_SIZE ((size_t)(2048u + 4u))

/// Use the max bootloader cmd size less 2 bytes (one byte for command code
/// and one byte for the destination).
#define EX10_MAX_IMAGE_CHUNK_SIZE \
    ((size_t)(EX10_BOOTLOADER_MAX_COMMAND_SIZE - 2))

/// The nominal timeout to wait for the Ex10 chip to process a command
/// and assert READY_N low, indicating that it is ready for a new command or
/// that it is ready to send out a response.
#define NOMINAL_READY_N_TIMEOUT_MS ((uint32_t)2500u)

enum Ex10CommandsHostResult
{
    HostCommandsSuccess = 0,
    HostCommandsBadNumSpans,
    HostCommandsNullData,
    HostCommandsOverMaxDeviceAddress,
    HostCommandsRecievedLengthIncorrect,
    HostCommandsBadCommandedLength,
    HostCommandsTestTransferVerifyError,
    HostCommands32BitAlignment,
};

struct Ex10CommandsHostErrors
{
    bool                        error_occurred;
    enum Ex10CommandsHostResult host_result;
    enum ResponseCode           device_response;
};

// clang-format off
// IPJ_autogen | generate_application_sdk_ex10_api_formatting {
// The formatting for the given Ex10 API commands
#pragma pack(push, 1)

struct Ex10ReadFormat
{
    uint16_t address;
    uint16_t length;
};
struct Ex10WriteFormat
{
    uint16_t address;
    uint16_t length;
    uint8_t const* data;
};
struct Ex10ReadFifoFormat
{
    uint8_t fifo_select;
    uint16_t transfer_size;
};
struct Ex10StartUploadFormat
{
    uint8_t upload_code;
    uint8_t const* upload_data;
};
struct Ex10ContinueUploadFormat
{
    uint8_t const* upload_data;
};
struct Ex10ResetFormat
{
    uint8_t destination;
};
struct Ex10TestTransferFormat
{
    uint8_t const* data;
};
struct Ex10WriteInfoPageFormat
{
    uint8_t page_id;
    uint8_t const* data;
    uint16_t crc;
};
struct Ex10TestReadFormat
{
    uint32_t address;
    uint16_t length;
};
struct Ex10InsertFifoEventFormat
{
    uint8_t trigger_irq;
    uint8_t const* packet;
};

#pragma pack(pop)
// IPJ_autogen }
// clang-format on

enum FifoSelection
{
    EventFifo = 0,
};

struct Ex10Commands
{
    /**
     * Read one or more byte spans of memory on the device.
     * Given an addresses and lengths to read, access the device over SPI bus to
     * read the byte spans.
     *
     * @param reg_list           A pointer to a buffer of register_info structs,
     *                           from which the address and length are used.
     * @param byte_spans         A pointer to an array of buffer pointers to
     *                           where the read data it stored.
     * @param segment_count      The number of registers to read from, which
     * must match the number of nodes in both the reg_list[] and byte_spans[]
     * arrays.
     * @param ready_n_timeout_ms The number of milliseconds to wait for the
     *                           Ex10 READY_N line to assert low.
     */
    struct Ex10CommandsHostErrors (*read)(
        struct RegisterInfo const* const reg_list[],
        struct ByteSpan*                 byte_spans[],
        size_t                           segment_count,
        uint32_t                         ready_n_timeout_ms);

    /**
     * Read an Ex10 register blob.
     *
     * @param address          Address to begin reading from.
     * @note                   This must be 32-bit aligned.
     * @param length_in_bytes  Number of bytes to read from the register.
     * @note                   This must a mulitple of 4 bytes.
     * @param buffer           The buffer data from the register will be written
     * to.
     *
     * @note This function assumes that the buffer parameter is at least as
     *       large as the length parameter.
     */
    struct Ex10CommandsHostErrors (*test_read)(uint32_t address,
                                               uint16_t length_in_bytes,
                                               void*    read_buffer);

    /**
     * Extend the passed in 'cmd_buffer' with the next write comand. This takes
     * care of proper formatting by inserting the write command's address,
     * length, and data copied from the passed in 'buffer'.
     *
     * @param address     The address specifier for the write command.
     * @param length      The length specifier for the write command. Also the
     * length to be copied from 'buffer'.
     * @param cmd_buffer  The buffer into which the write command will be
     * copied.
     * @param buffer      The buffer which contains the data to write to the
     * register.
     */
    void (*extend_write_buffer)(uint16_t    address,
                                uint16_t    length,
                                uint8_t*    cmd_buffer,
                                void const* buffer);

    /**
     * Write one or more byte spans on the device.
     * Given an array of byte spans, access the device over SPI to write.
     *
     * @param reg_list           A pointer to a buffer of register_info structs,
     *                           from which the address and length are used.
     * @param byte_spans         An array of ConstByteSpan structs to write
     *                           to the Ex10 device.
     * @param segment_count      The number of registers to write to, which
     * must match the number of nodes in both the reg_list[] and byte_spans[]
     * arrays.
     * @param ready_n_timeout_ms The number of milliseconds to wait for the
     *                           Ex10 READY_N line to assert low.
     */
    struct Ex10CommandsHostErrors (*write)(
        const struct RegisterInfo* const reg_list[],
        struct ConstByteSpan const*      byte_spans[],
        size_t                           segment_count,
        uint32_t                         ready_n_timeout_ms);

    /**
     * Read the specified number of bytes from device FIFO stream.
     * Given a selected fifo, read bytes from the fifo into a ByteSpan.
     *
     * Multiple ReadFifo commands are made until the byte_span length
     * value is filled to capacity.
     *
     * @param selection Which FIFO to read from.
     *                  In the Ex10 only the EventFifo (0) is supported.
     * @param byte_span A struct with a pointer to a ByteSpan struct to hold
     *                  the requested bytes, along with a length.
     *                  The bytespan data buffer should be able to hold one
     *                  more byte than the number of bytes to read from the
     *                  fifo. This extra byte will hold the response code
     *                  from the device.
     * @return Returns the Ex10CommandsHostErrors struct to inform the caller
     *         what type of issues happened during the command.
     *         Can return a host_result error if the passed data address for
     *         reading the fifo is not 32 bit aligned.
     *         Can return a device_response error if the first byte read back
     *         from the device is not a Success code.
     *         Can return a host_result error if the length read back is not
     *         what was expected from the response.
     * @note If 32-bit alignment of the event fifo messages is required then
     *       byte_span->data[1] must be u32 aligned since data[0] will contain
     *       the response code.
     *
     * The byte_span->length should be the number of bytes obtained by reading
     * the EventFifoNumBytes register. It does not include the response code
     * byte.
     *
     * The byte_span->length value will be updated with the number of bytes read
     * using the ReadFifo command into the buffer.
     *
     * @warning If the byte_span value is greater than the number of bytes
     *          contained in the fifo then this function will continue to read
     *          from the fifo until the length value is satisfied. Therefore the
     *          byte_span length value should contain the value read from
     *          EventFifoNumBytes register.
     */
    struct Ex10CommandsHostErrors (*read_fifo)(enum FifoSelection selection,
                                               struct ByteSpan*   byte_span);

    /**
     * Erase and write to an info page.
     *
     * @param page_code  Tells the bootloader which info page to work with.
     * @param image_data A struct with a pointer to a struct ConstByteSpan which
     *                   contains the image_data to be uploaded and length.
     * @param crc16      crc16-ccitt calculated over image_data bytes
     */
    struct Ex10CommandsHostErrors (*write_info_page)(
        uint8_t                     page_code,
        const struct ConstByteSpan* image_data,
        uint16_t                    crc16);

    /**
     * Initiate an image upload while in the bootloader.
     *
     * @param code       Tells the bootloader where to upload the App image in
     *                    memory.
     * @param image_data A struct with a pointer to a struct ConstByteSpan which
     *                   contains the image_data to be uploaded and length.
     */
    struct Ex10CommandsHostErrors (
        *start_upload)(uint8_t code, const struct ConstByteSpan* image_data);

    /**
     * Continue the FW upload process. Should have been preceded by a start
     * upload or continue upload command.
     *
     * @param image_data A struct with a pointer to a struct ConstByteSpan which
     *                   contains the image_data to be uploaded and length.
     */
    struct Ex10CommandsHostErrors (*continue_upload)(
        const struct ConstByteSpan* image_data);

    /**
     * Complete an image upload after all image data has been sent. Should have
     * been preceded by a start upload or continue upload command.
     */
    void (*complete_upload)(void);

    /**
     * Forces re-validation of the app image from the bootloader.
     */
    void (*revalidate_main_image)(void);

    /**
     * Soft reset of the device.
     *
     * @param destination Where to go after the reset:
     *                    1 - Bootloader
     *                    2 - Application
     * @note This command has no response.
     */
    void (*reset)(enum Status destination);

    /**
     * Run the Transfer Test command and get the response.
     *
     * @param send A ConstByteSpan to send as the transfer test payload.
     * @param recv A ByteSpan of received bytes from the transfer test. ByteSpan
     *             length must be => length of the ByteSpan to send.
     * @param verify Check the test response and return error if incorrect.
     */
    struct Ex10CommandsHostErrors (*test_transfer)(
        struct ConstByteSpan const* send,
        struct ByteSpan*            recv,
        bool                        verify);

    void (*create_fifo_event)(struct EventFifoPacket const* event_packet,
                              uint8_t*                      command_buffer,
                              size_t const                  padding_bytes,
                              size_t const                  packet_bytes);

    /**
     * Insert an arbitray EventFifo packet into the Event Fifo stream.
     *
     * @param trigger_irq  A bool that indicates a fifo above threshold
     *                     interrupt will be generated.
     * @param event_packet The EventFifo packet to append to the end of the Ex10
     *                     message queue.
     *
     * @note This command has no response
     */
    void (*insert_fifo_event)(const bool                    trigger_irq,
                              struct EventFifoPacket const* event_packet);
};

struct Ex10Commands const* get_ex10_commands(void);

#ifdef __cplusplus
}
#endif
