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

#include "board/driver_list.h"
#include "ex10_api/application_register_definitions.h"
#include "ex10_api/application_register_field_enums.h"
#include "ex10_api/commands.h"
#include "ex10_api/crc16.h"
#include "ex10_api/fifo_buffer_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IMAGE_BYTES (254000)

enum
{
    UploadFlash = 0x01
};

enum PageIds
{
    MainBlockId       = 0u,
    FeatureControlsId = 1u,
    ManufacturingId   = 2u,
    CalPageId         = 3u,
    StoredSettingsId  = 4u,
};

enum TimeoutType
{
    NoTimeout = 0u,
    OpTimeout = 1u,
};

/**
 * @struct OpCompletionStatus
 * Used to report back errors encountered while performing ops.
 * This includes errors on the SDK side as well as coming from
 * the FW.
 */
struct OpCompletionStatus
{
    /// A simple denotation of whether an error was seen
    bool error_occurred;
    /// All info about the most recent op to finish aka the previous op run.
    /// This contains the op id and any errors the FW encountered while running
    /// it.
    struct OpsStatusFields ops_status;
    /// Whether there was an issue communicating with the Ex10 device.
    enum ResponseCode command_error;
    /// Whether there was a timeout while waiting on an op to complete.
    enum TimeoutType timeout_error;
    /// Whether an overflow occurred in adding an element to the aggregate op
    /// buffer.
    bool aggregate_buffer_overflow;
};

/**
 * @struct Ex10Protocol
 * Ex10 Protocol interface.
 */
struct Ex10Protocol
{
    /**
     * Initialize the Ex10Protocol object.
     * This sets up access to an Impinj Reader Chip.
     *
     * @note This function does not perform any interaction with the
     *       Impinj Reader Chip; it only sets up the object.
     *
     * @param driver_list The interface to use to communicate with the Ex10.
     */
    void (*init)(struct Ex10DriverList const* driver_list);

    /**
     * Initialize the Impinj Reader Chip at the Ex10Protocol level.
     * - Sets the default EventFifo threshold level to 2048 bytes.
     * - Clears the Ex10 interrupt mask register.
     * - Clears the interrupt status register by reading it.
     *
     * @note The Ex10 must be powered on prior to calling this function.
     *        The chain of Ex10 module processing is enabled in this call.
     *
     * @return int   A POSIX error code indicating whether board initialization
     *               passed or failed.
     * @retval 0     Indicates the initialization was successful.
     * @retval != 0  Indicates a pthread related initialization error.
     */
    int (*init_ex10)(void);

    /** Release any resources used by the Ex10Protocol object. */
    void (*deinit)(void);

    /**
     * Register an optional callback for EventFifo data events.
     * @param fifo_cb This callback is triggered when the interrupt_cb
     *                registered through register_interrupt_callback()
     *                returns true.
     * @see register_interrupt_callback
     */
    void (*register_fifo_data_callback)(
        void (*fifo_cb)(struct FifoBufferNode*));

    /**
     * Enable specified interrupts and register a callback
     * @param enable_mask The mask of interrupts to enable.
     * @param interrupt_cb A function called when an enabled interrupt fires.
     * The callback receives the interrupt status bits via argument and it
     * should return true or false. If the return value is true then the
     * fifo_data callback is triggered. @see register_fifo_data_callback.
     */
    void (*register_interrupt_callback)(
        struct InterruptMaskFields enable_mask,
        bool (*interrupt_cb)(struct InterruptStatusFields));

    /// Unregister the callback used to deal with fifo data
    void (*unregister_fifo_data_callback)(void);

    /**
     * Unregister the callback for interrupts and write to the Ex10 device to
     * disable the interrupts.
     */
    void (*unregister_interrupt_callback)(void);

    /**
     * Read an Ex10 Register.
     *
     * @param reg_info A description of the register to access.
     * @param buffer The buffer data from the register will be read into.
     *
     * @note This function assumes that the buffer parameter is the same length
     * as the register defined in reg_info.
     */
    void (*read)(struct RegisterInfo const* const reg_info, void* buffer);

    /**
     * Read raw data from the Impinj Reader Chip internal memory layout.
     *
     * @param address Address to begin reading from
     * @param length  Number of bytes to read from the register.
     * @param buffer  The buffer data from the register will be written to.
     *
     * @note This function assumes that the buffer parameter is at least as
     * large as the length parameter.
     */
    void (*test_read)(uint32_t address, uint16_t length, void* buffer);

    /**
     * Read an Ex10 Register.
     *
     * @param reg_info A description of the register to access.
     * @param buffer The buffer data from the register will be read into.
     * @param index    Provides an offset into registers which have multiple
     * entries. Every register has a num_entires fields, where some registers
     * are divided into pieces when used. This is documented on a reg by reg
     * basis. If an index exceeding that allowed by the register is passed in,
     * an error is thrown.
     *
     * @note This function assumes that the buffer parameter is the same length
     * as the register defined in reg_info.
     */
    void (*read_index)(struct RegisterInfo const* const reg_info,
                       void*                            buffer,
                       uint8_t                          index);

    /**
     * Write an Ex10 Register.
     *
     * @param reg_info A description of the register to access.
     * @param buffer   The data to be written to buffer.
     *
     * @note This function assumes that the buffer parameter is the same length
     * as the register defined in reg_info.
     */
    void (*write)(struct RegisterInfo const* const reg_info,
                  void const*                      buffer);

    /**
     * Write an Ex10 Register.
     *
     * @param reg_info A description of the register to access.
     * @param buffer   The data to be written to buffer.
     * @param index    Provides an offset into registers which have multiple
     * entries. Every register has a num_entires fields, where some registers
     * are divided into pieces when used. This is documented on a reg by reg
     * basis. If an index exceeding that allowed by the register is passed in,
     * an error is thrown.
     *
     * @note This function assumes that the buffer parameter is the same length
     * as the register defined in reg_info.
     */
    void (*write_index)(struct RegisterInfo const* const reg_info,
                        void const*                      buffer,
                        uint8_t                          index);

    /**
     * Read an Ex10 register blob.
     *
     * @param address Address to begin reading from
     * @param length  Number of bytes to read from the register.
     * @param buffer  The buffer data from the register will be written to.
     *
     * @note This function assumes that the buffer parameter is at least as
     * large as the length parameter.
     */
    void (*read_partial)(uint16_t address, uint16_t length, void* buffer);

    /**
     * Write an Ex10 register blob.
     *
     * @param address Address to begin writing to
     * @param length  Number of bytes to write to the register.
     * @param buffer  The data to be written to the register.
     *
     * @note This function assumes that the buffer parameter is at least as
     * large as the length parameter.
     */
    void (*write_partial)(uint16_t    address,
                          uint16_t    length,
                          void const* buffer);

    /**
     * Write multiple registers to the Ex10 device.
     *
     * @param regs     A list of registers to write to
     * @param buffers  A list of buffers with data to write to each register in
     * regs
     * @param num_regs The number of entries in regs and buffers.
     *
     * @note This function assumes that the buffer parameter is at least as
     * large as the length parameter.
     */
    void (*write_multiple)(struct RegisterInfo const* const regs[],
                           void const*                      buffers[],
                           size_t                           num_regs);

    /**
     * Get a span containing data written to multiple registers.
     *
     * The span will contain a write command, address, length, and data
     * suitable for stored settings.
     *
     * @param regs     A list of registers to write.
     * @param buffers  A list of buffers with data to write to each register in
     * regs.
     * @param num_regs The number of entries in regs and buffers.
     * @param span     Destination span that will contain all the writes.
     */
    void (*get_write_multiple_stored_settings)(
        struct RegisterInfo const* const regs[],
        void const*                      buffers[],
        size_t                           num_regs,
        struct ByteSpan*                 span);

    /**
     * Read an info page from flash.
     *
     * @param address     The base info page address.
     * @param read_buffer Destination buffer for the page data, 2048 bytes in
     * size.
     */
    void (*read_info_page_buffer)(uint32_t address, uint8_t* read_buffer);

    /**
     * Read multiple registers from the Ex10 device.
     *
     * @param regs     A list of registers to read from.
     * @param buffers  A list of destination buffers for register data.
     * @param num_regs The number of entries in regs and buffers.
     */
    struct Ex10CommandsHostErrors (*read_multiple)(
        struct RegisterInfo const* const regs[],
        void*                            buffers[],
        size_t                           num_regs);

    /**
     * Command the Ex10 device to start running the specified op.
     *
     * @param op_id The op to run.
     */
    void (*start_op)(enum OpId op_id);

    /** Command the Ex10 device to stop running an ongoing op. */
    void (*stop_op)(void);

    /**
     * Check to see if an op is currently active
     *
     * @return bool  true if an op is busy
     */
    bool (*is_op_currently_running)(void);

    /**
     * Block until ongoing op has completed.
     *
     * @return Info about the completed op.
     */
    struct OpCompletionStatus (*wait_op_completion)(void);

    /**
     * Block until ongoing op has completed.
     *
     * @param timeout_ms Function returns false if the op takes more
     *                   time than this number of milliseconds.
     * @return Info about the completed op.
     */
    struct OpCompletionStatus (*wait_op_completion_with_timeout)(
        uint32_t timeout_ms);

    /**
     * Issue a soft reset to the Ex10 chip.
     *
     * @param destination The requested execution context of the Ex10 device:
     *        Application or Bootlodaer.
     *
     * @note Requesting a reset into the Application does not guarantee that
     *       the device will actually enter Application mode.
     *
     * If there is no valid application image loaded into the Ex10, then the
     * request to reset into the Application will result in a reset into the
     * Bootloader.
     *
     * @return int  A integer error code indicating success or failure.
     * @retval = 0  The reset to the requested destination was successful.
     * @retval = 1  The Impinj Reader Chip reset to the Bootloader when the
     *              Application was requested.
     * @retval < 0  An operating system error code: The negative POSIX errno
     *              value.
     */
    int (*reset)(enum Status destination);

    /**
     * Set the EventFifo threshold in bytes. When the number of bytes
     * accumulated by the Ex10 is this threshold or higher then an interrupt
     * wil be generated.
     *
     * @param threshold The EventFifo threshold in bytes.
     */
    void (*set_event_fifo_threshold)(size_t threshold);

    /**
     * Insert a host defined event in the event fifo stream.
     *
     * @param event_packet The EventFifo packet append to the Ex10 EventFifo.
     */
    void (*insert_fifo_event)(const bool                    trigger_irq,
                              struct EventFifoPacket const* event_packet);

    /**
     * Get the running location of the FW (Application or Bootloader)
     *
     * @return The running location of the Ex10Device
     */
    enum Status (*get_running_location)(void);

    /**
     * Allows writing to device flash pages.
     *
     * @param page_id Determines which flash info page to erase
     * @param data_ptr A pointer to the calibration data
     * @param write_length The number of bytes to write
     * @param fref_khz FREF frequency (Khz), must be programmed to valid
     *                 value to enable programming or erasing flash.
     *                 Supported values listed in `FrefFreq`.
     *
     * A CRC-16-CCITT will be calculated across the data and appended to the
     * written data.
     */
    void (*write_info_page)(enum PageIds page_id,
                            void const*  data_ptr,
                            size_t       write_length,
                            uint32_t     fref_khz);

    /**
     * Allows writing the calibration data of the EX10 device.
     *
     * @param data_ptr A pointer to the calibration data
     * @param write_length The number of bytes to write
     *
     * A CRC-16-CCITT will be calculated across the data and appended to the
     * written data.
     */
    void (*write_calibration_page)(uint8_t const* data_ptr,
                                   size_t         write_length);

    /**
     * Erase an info flash page of the device
     *
     * @param page_id Determines which flash info page to erase
     * @param fref_khz FREF frequency (Khz), must be programmed to valid
     *                 value to enable programming or erasing flash.
     *                 Supported values listed in `FrefFreq`.
     *
     */
    void (*erase_info_page)(enum PageIds page_id, uint32_t fref_khz);

    /**
     * Erase the calibration page of the device.
     */
    void (*erase_calibration_page)(void);

    /**
     * Allows writing the stored settings of the EX10 device.
     *
     * @param data_ptr A pointer to the data to write to the settings
     * @param write_length The number of bytes to write
     *
     * A CRC-16-CCITT will be calculated across the data and appended to the
     * written data.
     */
    void (*write_stored_settings_page)(uint8_t const* data_ptr,
                                       size_t         write_length);

    /**
     * Upload an application image.
     *
     * @param destination  Where in memory to upload the image.
     * @param upload_image Info about the image to upload.
     */
    void (*upload_image)(uint8_t                    destination,
                         const struct ConstByteSpan upload_image);

    /**
     * Begin the image upload.
     *
     * @param destination  Where in memory to upload the image.
     * @param image_length The total length of the image, in bytes.
     */
    void (*upload_start)(uint8_t                    destination,
                         size_t                     image_length,
                         const struct ConstByteSpan image_chunk);

    /**
     * Upload part of an image.
     *
     * Call multiple times as need to upload a full image.
     * Each chunk size must not exceed 1021 bytes.
     *
     * @param image_chunk  Info about the image chunk to upload.
     */
    void (*upload_continue)(const struct ConstByteSpan image_chunk);

    /**
     * End the image upload and flash the image.
     */
    void (*upload_complete)(void);

    /**
     * Execute the revalidate command and ensure proper validation of
     * the application image in flash.
     */
    struct ImageValidityFields (*revalidate_image)(void);

    /**
     * Initiate a transfer test. This function executes the Ex10 TransferTest
     * command. This is useful for testing the host to Ex10 wireline
     * communication.
     *
     * @see test_transfer() in commands.h for parameter descriptions.
     */
    struct Ex10CommandsHostErrors (*test_transfer)(
        struct ConstByteSpan const* send,
        struct ByteSpan*            recv,
        bool                        verify);
};

struct Ex10Protocol const* get_ex10_protocol(void);

#ifdef __cplusplus
}
#endif
