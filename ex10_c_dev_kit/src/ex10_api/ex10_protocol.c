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
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "board/board_spec.h"
#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/bootloader_registers.h"
#include "ex10_api/command_transactor.h"
#include "ex10_api/commands.h"
#include "ex10_api/crc16.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/fifo_buffer_list.h"
#include "ex10_api/gpio_interface.h"
#include "ex10_api/trace.h"


// Note that the value used for the fifo threshold will dictate how often the
// sdk reads from the device. This, in accordance with number of tags being
// read, dictates how many SDK event FIFO buffers are being used at a given
// time. A smaller value will use more buffers, but the amount in that buffer
// will be smaller and therefore faster to process. There is a tradeoff here on
// memory available, traffic to the device, and tags being read.
static uint16_t const DEFAULT_EVENT_FIFO_THRESHOLD = EX10_EVENT_FIFO_SIZE / 2;
static size_t const   INFO_PAGE_SIZE               = 2048u;

struct InterruptMaskFields const irq_mask_clear = {
    .op_done                 = false,
    .halted                  = false,
    .event_fifo_above_thresh = false,
    .event_fifo_full         = false,
    .inventory_round_done    = false,
    .halted_sequence_done    = false,
    .command_error           = false,
    .aggregate_op_done       = false,
};

static void (*fifo_data_callback)(struct FifoBufferNode*)       = NULL;
static bool (*interrupt_callback)(struct InterruptStatusFields) = NULL;

static struct Ex10GpioInterface const*     _gpio_if                 = NULL;
static struct HostInterface const*         _host_if                 = NULL;
static struct Ex10Commands const*          _ex10_commands           = NULL;
static struct Ex10CommandTransactor const* _ex10_command_transactor = NULL;
static struct FifoBufferList const*        _fifo_buffer_list        = NULL;

/* Forward declarations as needed */
static void proto_write(struct RegisterInfo const* const reg_info,
                        void const*                      buffer);

static void proto_read(struct RegisterInfo const* const reg_info, void* buffer);

static struct Ex10CommandsHostErrors read_multiple(
    struct RegisterInfo const* const regs[],
    void*                            buffers[],
    size_t                           num_regs);

static enum Status               get_running_location(void);
static struct OpCompletionStatus wait_op_completion(void);
static struct OpCompletionStatus wait_op_completion_with_timeout(uint32_t);

static void unregister_fifo_data_callback(void)
{
    fifo_data_callback = NULL;
}

static void unregister_interrupt_callback(void)
{
    _gpio_if->irq_enable(false);
    bool const ex10_is_powered = _gpio_if->get_board_power();
    _gpio_if->irq_enable(true);

    if (ex10_is_powered)
    {
        // Disable all interrupts
        proto_write(&interrupt_mask_reg, &irq_mask_clear);
    }

    // Clear callback
    interrupt_callback = NULL;
}

static void register_fifo_data_callback(void (*fifo_cb)(struct FifoBufferNode*))
{
    assert(fifo_data_callback == NULL &&
           "Fifo Data callback already registered");
    fifo_data_callback = fifo_cb;
}

static void register_interrupt_callback(
    struct InterruptMaskFields enable_mask,
    bool (*interrupt_cb)(struct InterruptStatusFields))
{
    assert(interrupt_callback == NULL &&
           "Interrupt callback already registered");

    interrupt_callback = interrupt_cb;

    // Overwrite the interrupt mask register
    proto_write(&interrupt_mask_reg, &enable_mask);
}

/**
 * Read the EventFifo multiple times filling an EventFifoBuffer node.
 *
 * @param fifo_num_bytes The number of Event Fifo bytes to be read.
 * This value should have been read from the EventFifoNumBytes register.
 *
 * @return struct FifoBufferNode* A node pointing to captured EventFifo data
 * read from the Ex10 using the ReadFifo command.
 * @retval NULL If no EventFifo data was available then
 */
static struct FifoBufferNode* read_event_fifo(size_t fifo_num_bytes)
{
    struct FifoBufferNode* fifo_buffer = _fifo_buffer_list->free_list_get();

    if (!fifo_buffer)
    {
        fprintf(stderr, "Note: %s: no free event fifo buffers\n", __func__);

        // There are no EventFifo buffers available for use.
        return NULL;
    }

    // For the event fifo parsing to work we are relying on all fifo
    // packets to be read from the Ex10 into a contiguous buffer of data.
    // Otherwise packets will straddle buffers,
    // making packet parsing much more complicated.
    assert(fifo_num_bytes <= fifo_buffer->raw_buffer.length);

    // The response code will be placed in the byte prior to the
    // first packet which is 32-bit aligned.
    struct ByteSpan bytes = {
        .data   = fifo_buffer->raw_buffer.data,
        .length = fifo_num_bytes,
    };

    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response =
        _ex10_commands->read_fifo(EventFifo, &bytes);
    _gpio_if->irq_enable(true);

    if (response.error_occurred == false)
    {
        fifo_buffer->fifo_data.length = bytes.length;
    }
    else
    {
        // If the ReadFifo command failed then the contents of the buffer
        // cannot be parsed. Release the fifo buffer to the free list.
        _fifo_buffer_list->free_list_put(fifo_buffer);
        fifo_buffer = NULL;
    }

    return fifo_buffer;
}

static void interrupt_handler(void)
{
    // Do not attempt to process interrupts when the Impinj Reader Chip is
    // powered off. When the Ex10 is transitioned to PowerModeOff, the IRQ_N
    // line will fall triggering an interrupt. Do not process this interrupt.
    _gpio_if->irq_enable(false);
    bool const ex10_is_powered = _gpio_if->get_board_power();
    _gpio_if->irq_enable(true);

    if (ex10_is_powered == false)
    {
        return;
    }

    struct RegisterInfo const* const regs[] = {
        &status_reg, &interrupt_status_reg, &event_fifo_num_bytes_reg};

    struct StatusFields            status;
    struct InterruptStatusFields   irq_status;
    struct EventFifoNumBytesFields fifo_num_bytes;

    void* buffers[] = {&status, &irq_status, &fifo_num_bytes};
    struct Ex10CommandsHostErrors const response =
        read_multiple(regs, buffers, sizeof(regs) / sizeof(void*));
    if (response.error_occurred)
    {
        fprintf(stderr, "error: %s: read_multiple() failed:\n", __func__);
        get_ex10_helpers()->print_commands_host_errors(&response);
        assert(0);
        return;
    }

    tracepoint(pi_ex10sdk, PROTOCOL_interrupt, irq_status);

    if (status.status != Application)
    {
        // Don't perform interrupt actions if we are not in the application.
        return;
    }

    // Determine if we want to read the fifo based on the interrupt_callback
    bool trigger_fifo_read = false;
    // If the interrupt fires, the interrupt registration set it, so we perform
    // the interrupt callback for any interrupt.
    if (interrupt_callback != NULL)
    {
        // The non fifo interrupt can trigger a fifo callback
        trigger_fifo_read = interrupt_callback(irq_status);
    }

    if (trigger_fifo_read)
    {
        // If the ReadFifo command fails the data should be discarded and not
        // reported up the stack. It cannot be parsed.
        if (fifo_num_bytes.num_bytes > 0)
        {
            struct FifoBufferNode* fifo_buffer =
                read_event_fifo(fifo_num_bytes.num_bytes);
            if (fifo_buffer != NULL)
            {
                if (fifo_data_callback != NULL)
                {
                    fifo_data_callback(fifo_buffer);
                }
                else
                {
                    // There are no consumers of the data; free the buffer.
                    _fifo_buffer_list->free_list_put(fifo_buffer);
                }
            }
        }
    }
}

static void init(struct Ex10DriverList const* driver_list)
{
    interrupt_callback = NULL;
    fifo_data_callback = NULL;

    _gpio_if       = &driver_list->gpio_if;
    _host_if       = &driver_list->host_if;
    _ex10_commands = get_ex10_commands();

    _ex10_command_transactor = get_ex10_command_transactor();
    _ex10_command_transactor->init(&driver_list->gpio_if,
                                   &driver_list->host_if);

    _fifo_buffer_list = get_ex10_fifo_buffer_list();
}

static int init_ex10(void)
{
    // Disable all interrupts
    proto_write(&interrupt_mask_reg, &irq_mask_clear);

    // Configure EventFifo interrupt threshold
    struct EventFifoIntLevelFields const level_data = {
        .threshold = DEFAULT_EVENT_FIFO_THRESHOLD};
    proto_write(&event_fifo_int_level_reg, &level_data);

    // Clear pending interrupts
    struct InterruptStatusFields irq_status;
    proto_read(&interrupt_status_reg, &irq_status);

    // Note: Ex10Protocol interrupt processing should not be enabled until
    // Ex10 is powered into the application.
    return _gpio_if->register_irq_callback(interrupt_handler);
}

static void deinit(void)
{
    unregister_fifo_data_callback();
    unregister_interrupt_callback();

    _gpio_if->deregister_irq_callback();
    _ex10_command_transactor->deinit();
}

static void read_partial(uint16_t address, uint16_t length, void* buffer)
{
    struct RegisterInfo const reg_info = {
        .address     = address,
        .length      = length,
        .num_entries = 1,
        .access      = ReadWrite,
    };
    struct RegisterInfo const* regs[] = {
        &reg_info,
    };

    void* buffers[] = {
        buffer,
    };

    struct Ex10CommandsHostErrors response = read_multiple(regs, buffers, 1);
    assert(response.error_occurred == false);

    tracepoint(pi_ex10sdk,
               PROTOCOL_read,
               ex10_get_thread_id(),
               address,
               length,
               buffer);
}

static void read_index(struct RegisterInfo const* const reg_info,
                       void*                            buffer,
                       uint8_t                          index)
{
    assert(reg_info);
    assert(buffer);
    assert(reg_info->access != WriteOnly || reg_info->access != Restricted);
    assert(index < reg_info->num_entries);
    read_partial(reg_info->address + (reg_info->length * index),
                 reg_info->length,
                 buffer);
}

static void proto_read(struct RegisterInfo const* const reg_info, void* buffer)
{
    struct RegisterInfo const* const regs[] = {
        reg_info,
    };
    void* buffers[] = {
        buffer,
    };

    struct Ex10CommandsHostErrors response = read_multiple(regs, buffers, 1);
    assert(response.error_occurred == false);
}

static struct Ex10CommandsHostErrors read_multiple(
    struct RegisterInfo const* const regs[],
    void*                            buffers[],
    size_t                           num_regs)
{
    struct ByteSpan  byte_spans[num_regs];
    struct ByteSpan* byte_span_ptrs[num_regs];

    for (size_t iter = 0; iter < num_regs; iter++)
    {
        // Ensure each reg is readable
        assert(regs[iter]->access != WriteOnly ||
               regs[iter]->access != Restricted);

        byte_span_ptrs[iter]  = &byte_spans[iter];
        byte_spans[iter].data = buffers[iter];
    }

    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response = _ex10_commands->read(
        regs, byte_span_ptrs, num_regs, NOMINAL_READY_N_TIMEOUT_MS);
    _gpio_if->irq_enable(true);

    return response;
}

static void proto_test_read(uint32_t address, uint16_t length, void* buffer)
{
    // Address reads needs to fall on 4 byte boundary
    assert(address % 4 == 0);
    assert(length % 4 == 0);

    uint16_t offset     = 0;
    uint8_t* buffer_ptr = (uint8_t*)buffer;

    // Determine max size while maintaining 4 byte increments
    uint16_t max_u32_reads = 128 - 1;
    max_u32_reads -= max_u32_reads % 4;
    uint16_t max_bytes_read = max_u32_reads * 4;

    // Loops through multiple reads for long spans of memory
    while (length > 0)
    {
        // decide how much to read per transaction
        uint16_t read_len_bytes =
            (length > max_bytes_read) ? max_bytes_read : length;

        // perform test read
        _gpio_if->irq_enable(false);
        const struct Ex10CommandsHostErrors response =
            _ex10_commands->test_read(
                address + offset, read_len_bytes, &buffer_ptr[offset]);
        _gpio_if->irq_enable(true);
        assert(response.error_occurred == false);

        offset += read_len_bytes;
        length -= read_len_bytes;
    }
}

static void read_info_page_buffer(uint32_t address, uint8_t* read_buffer)
{
    // Info page is 2048 bytes long, divided into chunks to fit in buffers.
    // The transfer size must be a multiple of 4, so mask out the lower 2 bits.
    uint32_t       bytes_left = INFO_PAGE_SIZE;
    uint32_t const chunk_mask = ~0x03;
    uint32_t       chunk_size = EX10_SPI_BURST_SIZE & chunk_mask;
    uint32_t       chunks     = INFO_PAGE_SIZE / EX10_SPI_BURST_SIZE;
    uint32_t       offset     = 0;

    for (uint32_t chunk = 0; chunk < chunks; chunk++)
    {
        get_ex10_protocol()->test_read(
            address + offset, chunk_size, &read_buffer[offset]);

        offset += chunk_size;
        bytes_left -= chunk_size;
    }
    if (bytes_left)
    {
        get_ex10_protocol()->test_read(
            address + offset, bytes_left, &read_buffer[offset]);
    }
}

static void setup_write_spans(struct RegisterInfo const* const regs[],
                              void const*                      buffers[],
                              size_t                           num_regs,
                              struct ConstByteSpan             byte_spans[],
                              const struct ConstByteSpan*      byte_span_ptrs[])
{
    for (size_t iter = 0; iter < num_regs; iter++)
    {
        byte_spans[iter].data   = buffers[iter];
        byte_spans[iter].length = regs[iter]->length;
        byte_span_ptrs[iter]    = &byte_spans[iter];

        // Ensure each reg is writeable
        assert(regs[iter]->access != ReadOnly ||
               regs[iter]->access != Restricted);
    }
}

static void get_write_multiple_stored_settings(
    struct RegisterInfo const* const regs[],
    void const*                      buffers[],
    size_t                           num_regs,
    struct ByteSpan*                 span)
{
    assert(regs);
    assert(buffers);
    assert(span);

    // Header contains: remain_in_bl_after_crash + writes_format byte.
    static const size_t stored_settings_header_size =
        sizeof(uint64_t) + sizeof(uint8_t);

    struct ConstByteSpan        byte_spans[num_regs];
    const struct ConstByteSpan* byte_span_ptrs[num_regs];

    setup_write_spans(regs, buffers, num_regs, byte_spans, byte_span_ptrs);

    // Now begin setting the span for use with stored settings
    // Prepend 64 bits for remain_in_bl_after_crash field.
    for (uint8_t i = 0; i < sizeof(uint64_t); i++)
    {
        span->data[i] = 0xff;
    }

    // Need to set the write command as the first byte so stored settings knows
    // what to do.
    span->data[sizeof(uint64_t)] = CommandWrite;

    // Now we copy in the data to write for each register
    span->length        = stored_settings_header_size;
    uint8_t header_size = sizeof(regs[0]->address) + sizeof(regs[0]->length);
    for (size_t iter = 0; iter < num_regs; iter++)
    {
        _ex10_commands->extend_write_buffer(regs[iter]->address,
                                            regs[iter]->length,
                                            &span->data[span->length],
                                            buffers[iter]);
        span->length += regs[iter]->length + header_size;
    }
}

static void write_multiple(struct RegisterInfo const* const regs[],
                           void const*                      buffers[],
                           size_t                           num_regs)
{
    assert(regs);
    assert(buffers);

    struct ConstByteSpan        byte_spans[num_regs];
    const struct ConstByteSpan* byte_span_ptrs[num_regs];

    setup_write_spans(regs, buffers, num_regs, byte_spans, byte_span_ptrs);

    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response = _ex10_commands->write(
        regs, byte_span_ptrs, num_regs, NOMINAL_READY_N_TIMEOUT_MS);
    _gpio_if->irq_enable(true);

    assert(response.error_occurred == false);
}

static void proto_write(struct RegisterInfo const* const reg_info,
                        void const*                      buffer)
{
    struct RegisterInfo const* regs[] = {
        reg_info,
    };
    void const* buffers[] = {
        buffer,
    };
    write_multiple(regs, buffers, 1);
}

static void write_partial(uint16_t address, uint16_t length, void const* buffer)
{
    struct RegisterInfo const reg_info = {
        .address     = address,
        .length      = length,
        .num_entries = 1,
        .access      = ReadWrite,
    };
    struct RegisterInfo const* regs[] = {
        &reg_info,
    };
    void const* buffers[] = {
        buffer,
    };

    write_multiple(regs, buffers, 1);
}

static void write_index(struct RegisterInfo const* const reg_info,
                        void const*                      buffer,
                        uint8_t                          index)
{
    assert(reg_info);
    assert(buffer);
    assert(reg_info->access != ReadOnly || reg_info->access != Restricted);
    assert(index < reg_info->num_entries);
    write_partial(reg_info->address + (reg_info->length * index),
                  reg_info->length,
                  buffer);
}

/**
 * @details
 * In both Application mode and Bootloader mode, the host will be able to
 * communicate to the device using the BOOTLOADER_SPI_CLOCK_HZ (1 MHz) speed.
 *
 * In Bootloader mode, the device will not respond to SPI
 * transactions clocked faster than 1 MHz. Therefore, after a reset the
 * SPI clock speed must be set to the slower BOOTLOADER_SPI_CLOCK_HZ.
 * Once it is established that the Ex10 is running in Application mode,
 * then the clock speed can be increased to 4 MHz (DEFAULT_SPI_CLOCK_HZ).
 */
static int reset(enum Status destination)
{
    _gpio_if->irq_enable(false);
    _host_if->close();
    int error = _host_if->open(BOOTLOADER_SPI_CLOCK_HZ);
    _ex10_commands->reset(destination);
    _gpio_if->irq_enable(true);

    enum Status const current_running_location = get_running_location();

    if ((error = 0) && (current_running_location == Application) &&
        (destination == Application))
    {
        // Application execution confirmed. Set the SPI clock rate to 4 MHz.
        _gpio_if->irq_enable(false);
        _host_if->close();
        error = _host_if->open(DEFAULT_SPI_CLOCK_HZ);
        _gpio_if->irq_enable(true);
    }

    return error;
}

static void set_event_fifo_threshold(size_t threshold)
{
    assert(threshold <= EX10_EVENT_FIFO_SIZE);

    struct EventFifoIntLevelFields const event_fifo_thresh = {
        .threshold = (uint16_t)threshold, .rfu = 0u};
    proto_write(&event_fifo_int_level_reg, &event_fifo_thresh);
}

static void insert_fifo_event(const bool                    trigger_irq,
                              struct EventFifoPacket const* event_packet)
{
    _gpio_if->irq_enable(false);
    _ex10_commands->insert_fifo_event(trigger_irq, event_packet);
    _gpio_if->irq_enable(true);
}

static struct OpCompletionStatus wait_op_completion(void)
{
    const uint32_t default_timeout_ms = 10000u;
    return wait_op_completion_with_timeout(default_timeout_ms);
}

static struct OpCompletionStatus wait_op_completion_with_timeout(
    uint32_t timeout_ms)
{
    uint32_t const start_time = get_ex10_time_helpers()->time_now();
    // Used to track all return errors
    struct OpsStatusFields ops_status;
    memset(&ops_status, 0u, sizeof(ops_status));
    struct OpCompletionStatus op_error = {.error_occurred = false,
                                          .ops_status     = ops_status,
                                          .command_error  = Success,
                                          .timeout_error  = NoTimeout,
                                          .aggregate_buffer_overflow = false};

    struct RegisterInfo const* regs[]    = {&ops_status_reg};
    void*                      buffers[] = {&ops_status};
    do
    {
        struct Ex10CommandsHostErrors response =
            read_multiple(regs, buffers, 1u);
        // Store results
        op_error.command_error = response.device_response;
        op_error.ops_status    = ops_status;
        // Check for errors
        if (response.error_occurred)
        {
            op_error.error_occurred = true;
        }
        if (ops_status.error != ErrorNone)
        {
            op_error.error_occurred = true;
        }
        if (get_ex10_time_helpers()->time_elapsed(start_time) >= timeout_ms)
        {
            op_error.error_occurred = true;
            op_error.timeout_error  = true;
        }
    } while (ops_status.busy && op_error.error_occurred == false);
    tracepoint(
        pi_ex10sdk, PROTOCOL_op_done, ops_status.op_id, ops_status.error);

    if (op_error.error_occurred)
    {
        fprintf(stderr,
                "Op timeout: %d\n Command error: %d\n Op ERROR: Op 0x%02X, "
                "code %d, busy: %d\n",
                op_error.timeout_error,
                op_error.command_error,
                op_error.ops_status.op_id,
                op_error.ops_status.error,
                op_error.ops_status.busy);
    }
    return op_error;
}

static void start_op(enum OpId op_id)
{
    struct OpsControlFields const ops_control_data = {.op_id = (uint8_t)op_id};
    proto_write(&ops_control_reg, &ops_control_data);
    tracepoint(pi_ex10sdk, PROTOCOL_start_op, op_id);
}

static void stop_op(void)
{
    start_op(Idle);
}

static bool is_op_currently_running(void)
{
    struct OpsControlFields ops_control;
    proto_read(&ops_control_reg, &ops_control);
    return ops_control.op_id != Idle;
}

static enum Status get_running_location(void)
{
    struct StatusFields status;
    proto_read(&status_reg, &status);
    return status.status;
}

static void write_info_page(enum PageIds page_id,
                            void const*  data_ptr,
                            size_t       write_length,
                            uint32_t     fref_khz)
{
    assert(data_ptr);
    assert(get_running_location() == Bootloader);

    // A length of 0 will erase the calibration page
    // Set flash frequency to allow info page update
    struct FrefFreqBootloaderFields const fref_freq = {.fref_freq_khz =
                                                           fref_khz};
    proto_write(&fref_freq_reg, &fref_freq);

    uint16_t crc16 = 0;
    if (write_length)
    {
        crc16 = ex10_compute_crc16(data_ptr, write_length);
    }

    struct ConstByteSpan page_data = {
        .data   = data_ptr,
        .length = write_length,
    };

    // Send the data
    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response =
        _ex10_commands->write_info_page(page_id, &page_data, crc16);
    _gpio_if->irq_enable(true);
    assert(response.error_occurred == false);
}

static void write_calibration_page(uint8_t const* data_ptr, size_t write_length)
{
    write_info_page(CalPageId, data_ptr, write_length, TCXO_FREQ_KHZ);
}

static void write_stored_settings_page(uint8_t const* data_ptr,
                                       size_t         write_length)
{
    write_info_page(StoredSettingsId, data_ptr, write_length, TCXO_FREQ_KHZ);
}

static void erase_info_page(enum PageIds page_id, uint32_t fref_khz)
{
    uint8_t fake_data[1] = {0u};

    write_info_page(page_id, fake_data, 0u, fref_khz);
}

static void erase_calibration_page(void)
{
    erase_info_page(CalPageId, TCXO_FREQ_KHZ);
}

static void upload_image(uint8_t code, const struct ConstByteSpan upload_image)
{
    assert(get_running_location() == Bootloader);

    // Set flash frequency to allow flash programming
    struct FrefFreqBootloaderFields const fref_freq = {.fref_freq_khz =
                                                           TCXO_FREQ_KHZ};
    proto_write(&fref_freq_reg, &fref_freq);

    struct CommandResultFields result;
    size_t                     remaining_length = upload_image.length;

    // Use the maximum SPI burst size less 2 bytes (one byte for command code
    // and one byte for the destination).
    const size_t upload_chunk_size = EX10_SPI_BURST_SIZE - 2;

    // used to split image into uploadable chunks
    struct ConstByteSpan chunk = {
        .data   = upload_image.data,
        .length = upload_chunk_size,
    };

    // Upload the image
    while (remaining_length)
    {
        chunk.length = (remaining_length < upload_chunk_size)
                           ? remaining_length
                           : upload_chunk_size;
        if (remaining_length == upload_image.length)
        {
            _gpio_if->irq_enable(false);
            const struct Ex10CommandsHostErrors response =
                _ex10_commands->start_upload(code, &chunk);
            _gpio_if->irq_enable(true);
            assert(response.error_occurred == false);
        }
        else
        {
            _gpio_if->irq_enable(false);
            const struct Ex10CommandsHostErrors response =
                _ex10_commands->continue_upload(&chunk);
            _gpio_if->irq_enable(true);
            assert(response.error_occurred == false);
        }
        remaining_length -= chunk.length;
        chunk.data += chunk.length;

        // Check upload status
        proto_read(&command_result_reg, &result);
        assert(result.failed_result_code == Success);
    }

    // Signify end of upload and check status
    _gpio_if->irq_enable(false);
    _ex10_commands->complete_upload();
    _gpio_if->irq_enable(true);
    proto_read(&command_result_reg, &result);
    assert(result.failed_result_code == Success);
}

static size_t upload_remaining_length = 0;
static size_t upload_image_length     = 0;

static void upload_start(uint8_t                    destination,
                         size_t                     image_length,
                         const struct ConstByteSpan image_chunk)
{
    assert(get_running_location() == Bootloader);
    assert(image_length <= MAX_IMAGE_BYTES);

    // Set flash frequency to allow flash programming
    struct FrefFreqBootloaderFields const fref_freq = {.fref_freq_khz =
                                                           TCXO_FREQ_KHZ};
    proto_write(&fref_freq_reg, &fref_freq);

    upload_remaining_length = image_length;
    upload_image_length     = image_length;

    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response =
        _ex10_commands->start_upload(destination, &image_chunk);
    _gpio_if->irq_enable(true);
    assert(response.error_occurred == false);
}

static void upload_continue(const struct ConstByteSpan image_chunk)
{
    assert(get_running_location() == Bootloader);

    // Use the maximum SPI burst size less 2 bytes (one byte for command code
    // and one byte for the destination).
    const size_t upload_chunk_size = EX10_MAX_IMAGE_CHUNK_SIZE - 2;

    // The chunk must be within the max chunk and the remaining size.
    assert(image_chunk.length <= upload_chunk_size);
    assert(image_chunk.length <= upload_remaining_length);

    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response =
        _ex10_commands->continue_upload(&image_chunk);
    _gpio_if->irq_enable(true);
    assert(response.error_occurred == false);

    upload_remaining_length -= image_chunk.length;

    // Check upload status
    struct CommandResultFields result;
    proto_read(&command_result_reg, &result);
    assert(result.failed_result_code == Success);
}

static void upload_complete(void)
{
    assert(get_running_location() == Bootloader);

    struct CommandResultFields result;

    // Signify end of upload and check status
    _gpio_if->irq_enable(false);
    _ex10_commands->complete_upload();
    _gpio_if->irq_enable(true);
    proto_read(&command_result_reg, &result);
}

static struct ImageValidityFields revalidate_image(void)
{
    // Set flash frequency to allow flash programming
    struct FrefFreqBootloaderFields const fref_freq = {.fref_freq_khz =
                                                           TCXO_FREQ_KHZ};
    proto_write(&fref_freq_reg, &fref_freq);

    _gpio_if->irq_enable(false);
    _ex10_commands->revalidate_main_image();
    _gpio_if->irq_enable(true);

    struct CommandResultFields result;
    proto_read(&command_result_reg, &result);
    assert(result.failed_result_code == Success);

    struct ImageValidityFields image_validity;
    proto_read(&image_validity_reg, &image_validity);
    return image_validity;
}

static struct Ex10CommandsHostErrors proto_test_transfer(
    struct ConstByteSpan const* send,
    struct ByteSpan*            recv,
    bool                        verify)
{
    _gpio_if->irq_enable(false);
    const struct Ex10CommandsHostErrors response =
        _ex10_commands->test_transfer(send, recv, verify);
    _gpio_if->irq_enable(true);

    return response;
}

static const struct Ex10Protocol ex10_protocol = {
    .init                               = init,
    .init_ex10                          = init_ex10,
    .deinit                             = deinit,
    .register_fifo_data_callback        = register_fifo_data_callback,
    .register_interrupt_callback        = register_interrupt_callback,
    .unregister_fifo_data_callback      = unregister_fifo_data_callback,
    .unregister_interrupt_callback      = unregister_interrupt_callback,
    .read                               = proto_read,
    .test_read                          = proto_test_read,
    .read_index                         = read_index,
    .write                              = proto_write,
    .write_index                        = write_index,
    .read_partial                       = read_partial,
    .write_partial                      = write_partial,
    .write_multiple                     = write_multiple,
    .get_write_multiple_stored_settings = get_write_multiple_stored_settings,
    .read_info_page_buffer              = read_info_page_buffer,
    .read_multiple                      = read_multiple,
    .stop_op                            = stop_op,
    .start_op                           = start_op,
    .is_op_currently_running            = is_op_currently_running,
    .wait_op_completion                 = wait_op_completion,
    .wait_op_completion_with_timeout    = wait_op_completion_with_timeout,
    .reset                              = reset,
    .set_event_fifo_threshold           = set_event_fifo_threshold,
    .insert_fifo_event                  = insert_fifo_event,
    .get_running_location               = get_running_location,
    .write_info_page                    = write_info_page,
    .erase_info_page                    = erase_info_page,
    .write_calibration_page             = write_calibration_page,
    .erase_calibration_page             = erase_calibration_page,
    .write_stored_settings_page         = write_stored_settings_page,
    .upload_image                       = upload_image,
    .upload_start                       = upload_start,
    .upload_continue                    = upload_continue,
    .upload_complete                    = upload_complete,
    .revalidate_image                   = revalidate_image,
    .test_transfer                      = proto_test_transfer,
};

struct Ex10Protocol const* get_ex10_protocol(void)
{
    return &ex10_protocol;
}
