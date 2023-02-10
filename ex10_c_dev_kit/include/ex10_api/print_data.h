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
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The type of prefix to write before each row of data written.
enum DataPrefix
{
    DataPrefixNone = 0,  ///< No prefix written.
    DataPrefixIndex,     ///< An index into the data, starting with zero.
    DataPrefixAddress    ///< The data address.
};

/**
 * Print a span of bytes to a file.
 *
 * @param fp        The destination file handle to print to.
 * @param data_vptr The bytewise data to print.
 * @param length    The length of the data to print.
 *
 * @return size_t   The number of bytes written to the file descriptor fp.
 */
size_t ex10_print_data_line(FILE* fp, void const* data_vptr, size_t length)
    __attribute__((visibility("hidden")));

/**
 * Print a span of 32-bit words to a file.
 *
 * @param fp        The destination file handle to print to.
 * @param data_vptr The bytewise data to print, which must be 32-bit aligned.
 * @param length    The number u32 words to print.
 *
 * @return size_t   The number of bytes written to the file descriptor fp.
 */
size_t ex10_print_int32_line(FILE*       fp,
                             void const* data_vptr,
                             size_t      length,
                             uint8_t     radix,
                             bool        is_signed)
    __attribute__((visibility("hidden")));

/**
 * Print data as bytes to a file stream.
 *
 * @param fp        The destination file stream.
 * @param data      A pointer to the data bytes to print.
 * @param length    The number of data bytes to print.
 * @param prefix    @see enum DataPrefix.
 *
 * @return size_t   The number of bytes written to the file descriptor fp.
 */
size_t ex10_print_data(FILE*           fp,
                       void const*     data,
                       size_t          length,
                       enum DataPrefix prefix)
    __attribute__((visibility("hidden")));

/**
 * Print data as 32-bit words to a file stream.
 * The data does not ned to be aligned.
 *
 * @param fp        The destination file stream.
 * @param data      A pointer to the 32-bit words to print.
 * @param length    The number of 32-bit words to print.
 * @param radix     10 or 16 to define the numberic base.
 * @param is_signed Defines the interpretation of each 32-bit
 *                  word when the radix is 10.
 * @param prefix    @see enum DataPrefix.
 *
 * @return size_t   The number of bytes written to the file descriptor fp.
 */
size_t ex10_print_int32(FILE*           fp,
                        void const*     data,
                        size_t          length,
                        uint8_t         radix,
                        bool            is_signed,
                        enum DataPrefix prefix)
    __attribute__((visibility("hidden")));

#ifdef __cplusplus
}
#endif
