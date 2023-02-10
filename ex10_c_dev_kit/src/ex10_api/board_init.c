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

#include <stdlib.h>
#include <time.h>

#include "ex10_api/board_init.h"

#include "board/driver_list.h"
#include "board/fifo_buffer_pool.h"
#include "board/time_helpers.h"
#include "board/uart_helpers.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/power_transactor.h"

// Intentionally leaving board_init_status uninitialized. Being static, all
// fields will be set to zeroes, which is not even a valid state.
// When board intialization does happen, then real values will get filled in.
static struct Ex10BoardInitStatus board_init_status;

struct Ex10BoardInitStatus ex10_get_board_init_status(void)
{
    struct Ex10BoardInitStatus init_error = board_init_status;

    board_init_status.op_status       = *get_ex10_ops()->op_error_none;
    board_init_status.power_up_status = 0;  // Intentionally invalid.
    board_init_status.protocol_error  = 0;  // Set for no error.

    return init_error;
}

void ex10_board_gpio_init(struct Ex10GpioInterface const* gpio_if)
{
    bool const board_power_on = false;
    bool const ex10_enable    = false;
    bool const reset          = true;  // sets pin level zero.
    gpio_if->initialize(board_power_on, ex10_enable, reset);
}

static void reset_board_init_error(void)
{
    board_init_status.power_up_status = 0;
    board_init_status.protocol_error  = 0;
    board_init_status.op_status       = *get_ex10_ops()->op_error_none;
}

struct Ex10Interfaces ex10_typical_board_setup(uint32_t    spi_clock_hz,
                                               char const* region_name)
{
    // Seed the random number generator, in the board init layer, prior to
    // initializing the region table. The region table is initialized when
    // Ex10Reader.init() is called.
    srand(time(NULL));

    reset_board_init_error();

    struct FifoBufferPool const* event_fifo_buffer_pool =
        get_ex10_event_fifo_buffer_pool();

    struct FifoBufferList const* fifo_buffer_list = get_ex10_fifo_buffer_list();

    fifo_buffer_list->init(event_fifo_buffer_pool->fifo_buffer_nodes,
                           event_fifo_buffer_pool->fifo_buffers,
                           event_fifo_buffer_pool->buffer_count);

    struct Ex10DriverList const* driver_list = get_ex10_board_driver_list();

    struct Ex10Interfaces ex10 = {
        .protocol      = get_ex10_protocol(),
        .ops           = get_ex10_ops(),
        .reader        = get_ex10_reader(),
        .power_modes   = get_ex10_power_modes(),
        .helpers       = get_ex10_helpers(),
        .gen2_commands = get_ex10_gen2_commands(),
        .event_parser  = get_ex10_event_parser(),
        .version       = get_ex10_version(),
    };

    // Initialize the modules first:
    get_ex10_power_transactor()->init();
    ex10_board_gpio_init(&driver_list->gpio_if);
    driver_list->host_if.open(spi_clock_hz);

    ex10.protocol->init(driver_list);
    ex10.ops->init(ex10.protocol);
    ex10.reader->init(ex10.ops, region_name);
    ex10.power_modes->init();

    // Power up the Ex10 Reader Chip. This may return with Bootlaoder status.
    // If this happens then only proceed with Ex10Protocol init_ex10().
    board_init_status.power_up_status =
        get_ex10_power_transactor()->power_up_to_application();
    // If the power_up_status is < 0 then there was an error initializing the
    // host interface. Nothing further can be done.
    if (board_init_status.power_up_status < 0)
    {
        return ex10;
    }

    board_init_status.protocol_error = ex10.protocol->init_ex10();

    // If we reset into the bootloader then do not call
    // Ex10Ops.init_ex10(), Ex10Reader.ex10(), ...
    if ((board_init_status.power_up_status == Bootloader) ||
        (board_init_status.protocol_error != 0))
    {
        return ex10;
    }

    // Progress through the Ex10 modules' initialization of the
    // Impinj Reader Chip.
    board_init_status.op_status = ex10.ops->init_ex10();
    if (board_init_status.op_status.error_occurred == true)
    {
        return ex10;
    }

    board_init_status.op_status = ex10.reader->init_ex10();
    if (board_init_status.op_status.error_occurred == true)
    {
        return ex10;
    }

    ex10.reader->read_calibration();
    return ex10;
}

struct Ex10Protocol const* ex10_bootloader_board_setup(uint32_t spi_clock_hz)
{
    reset_board_init_error();

    struct FifoBufferList const* fifo_buffer_list = get_ex10_fifo_buffer_list();

    fifo_buffer_list->init(NULL, NULL, 0u);

    struct Ex10DriverList const* driver_list = get_ex10_board_driver_list();
    struct Ex10Protocol const*   protocol    = get_ex10_protocol();

    get_ex10_power_transactor()->init();
    ex10_board_gpio_init(&driver_list->gpio_if);
    driver_list->host_if.open(spi_clock_hz);

    protocol->init(driver_list);
    get_ex10_power_transactor()->power_up_to_bootloader();

    return protocol;
}

void ex10_typical_board_uart_setup(enum AllowedBpsRates bitrate)
{
    struct Ex10DriverList const* driver_list = get_ex10_board_driver_list();
    driver_list->uart_if.open(bitrate);
    get_ex10_uart_helper()->init(driver_list);
}

void ex10_bootloader_board_teardown(void)
{
    get_ex10_protocol()->deinit();
    get_ex10_power_transactor()->power_down();
    get_ex10_power_transactor()->deinit();

    struct Ex10DriverList const* driver_list = get_ex10_board_driver_list();
    driver_list->gpio_if.cleanup();
    driver_list->host_if.close();
}

void ex10_typical_board_teardown(void)
{
    get_ex10_reader()->deinit();
    get_ex10_ops()->release();
    get_ex10_protocol()->deinit();

    get_ex10_power_transactor()->power_down();
    get_ex10_power_transactor()->deinit();

    struct Ex10DriverList const* driver_list = get_ex10_board_driver_list();
    driver_list->gpio_if.cleanup();
    driver_list->host_if.close();
}

void ex10_typical_board_uart_teardown(void)
{
    get_ex10_uart_helper()->deinit();
    get_ex10_board_driver_list()->uart_if.close();
}
