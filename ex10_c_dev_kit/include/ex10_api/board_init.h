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

#include "board/uart_helpers.h"
#include "ex10_api/board_init_status.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_ops.h"
#include "ex10_api/ex10_power_modes.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/gen2_commands.h"
#include "ex10_api/version_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Typical usage is 4MHz when running the Application.
static uint32_t const DEFAULT_SPI_CLOCK_HZ = 4000000u;

/// A reduced clock speed is required when running the Bootloader.
static uint32_t const BOOTLOADER_SPI_CLOCK_HZ = 1000000u;

/**
 * @struct Ex10Interfaces
 * The Impinj Reader chip aggregate interface.
 */
struct Ex10Interfaces
{
    struct Ex10Protocol const*     protocol;
    struct Ex10Ops const*          ops;
    struct Ex10Reader const*       reader;
    struct Ex10PowerModes const*   power_modes;
    struct Ex10Helpers const*      helpers;
    struct Ex10Gen2Commands const* gen2_commands;
    struct Ex10EventParser const*  event_parser;
    struct Ex10Version const*      version;
};

/**
 * If an error was encountered during board initialization, it can be
 * returned via this function call. When this function is called, the
 * board_init.c internal struct BoardInitError is cleared.
 *
 * @return struct BoardInitError The error status structure that
 *         contains any error encountered during board initialization.
 */
struct Ex10BoardInitStatus ex10_get_board_init_status(void);

/**
 * Initialize the gpio_driver pins required for Impinj Reader Chip operation.
 * The following schematic pins are set as output direction, with the levels:
 *   PWR_EN     deassert, active high, pin level = 0
 *   ENABLE     deassert, active high, pin level = 0
 *   RESET_N    assert,   active low,  pin level = 0
 *
 * @param gpio_if The instance of the GpioInterface.
 */
void ex10_board_gpio_init(struct Ex10GpioInterface const* gpio_if);

/**
 * Initialize the Impinj Reader Chip and its associated Ex10 host interfaces
 * in a typical configuration for use in a reader application.
 *
 * @param spi_clock_hz The SPI interface clokc speed in Hz.
 * @param region_name  The region in which the reader will operate.
 *                     i.e. "FCC" or "ETSI"
 *
 * @return struct Ex10Interfaces The Ex10 interface struct. This interface
 *         contains pointers to each of the Ex10 interface objects.
 *         If the interface fails to initialize then each object pointer is
 *         is set to NULL.
 */
struct Ex10Interfaces ex10_typical_board_setup(uint32_t    spi_clock_hz,
                                               char const* region_name);

/**
 * Initialize the Impinj Reader Chip and its associated Ex10 host interfaces
 * in a minimal configuration for communication with the bootloader. This
 * initialization profile is primarily used for uploading new firmware images
 * into the Impinj Reader Chip.
 *
 * @param spi_clock_hz The SPI interface clokc speed in Hz.
 *                     This value must be 1 MHz or less.
 *
 * @return struct Ex10Protocol const* A pointer the Ex10 protocol object.
 */
struct Ex10Protocol const* ex10_bootloader_board_setup(uint32_t spi_clock_hz);

/**
 * On the Impinj Reader Chip development board this fucntion wraps the UART
 * device driver initialization. This function should only be called after
 * the successful call to ex10_typical_board_setup() when the host UART
 * is to be used to control the development board operation.
 *
 * @param bitrate One of the valid values in enum AllowedBpsRates.
 */
void ex10_typical_board_uart_setup(enum AllowedBpsRates bitrate);

/**
 * Deinitailize the Ex10Protocol object and power down the Impinj Reader Chip
 * when the Impinj Reader Chip was configured in bootloader execution status.
 */
void ex10_bootloader_board_teardown(void);

/**
 * Deinitailize the Ex10Interfaces groups of objects and power down the
 * Impinj Reader Chip. This is strictly a software and driver layer release
 * of host resources. No transactions are performed to the Impinj Reader
 * Chip over the host interface within this function call.
 */
void ex10_typical_board_teardown(void);

/**
 * Deinitailize the UART driver used to control the development board.
 */
void ex10_typical_board_uart_teardown(void);

#ifdef __cplusplus
}
#endif
