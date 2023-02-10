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
#include <stdio.h>
#include <sys/types.h>

#include "board/uart_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Ex10UartDriver
{
    int32_t (*uart_open)(enum AllowedBpsRates bitrate);

    void (*uart_close)(void);

    /**
     * Used for any transaction requiring only a write with no
     * expectation of data coming back after the write.
     * @param tx_buff Buffer containing the data to be written
     * @param length  The number of bytes to write from tx_buff
     * @return Number of bytes written, -1 if not all bytes are written properly
     */
    int32_t (*uart_write)(const void* tx_buff, size_t length);

    /**
     * Used for any transaction requiring only a read from the
     * serial port.
     *
     * @param rx_buff Buffer in which to place incoming
     *                data after the write.
     * @param length  The number of bytes to read into rx_buff
     * @return Number of bytes read, -1 on read error
     */
    int32_t (*uart_read)(void* rx_buff, size_t length);
};

struct Ex10UartDriver const* get_ex10_uart_driver(void);

#ifdef __cplusplus
}
#endif
