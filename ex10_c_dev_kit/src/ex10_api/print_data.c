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

#include <ctype.h>
#include <stdint.h>

#include "ex10_api/print_data.h"

size_t ex10_print_data_line(FILE* fp, void const* data_vptr, size_t length)
{
    uint8_t const* data      = (uint8_t const*)data_vptr;
    size_t         n_written = 0u;
    for (size_t iter = 0u; iter < length; ++iter, ++data)
    {
        if ((iter % 4u == 0u) && (iter > 0u))
        {
            n_written += fprintf(fp, " ");
        }
        n_written += fprintf(fp, "%02X", *data);
    }

    return n_written;
}

size_t ex10_print_int32_line(FILE*       fp,
                             void const* data_vptr,
                             size_t      length,
                             uint8_t     radix,
                             bool        is_signed)
{
    uint8_t const* iter      = (uint8_t const*)data_vptr;
    size_t         n_written = 0u;
    for (size_t index = 0u; index < length; ++index, iter += sizeof(uint32_t))
    {
        uint32_t value = 0;
        value |= iter[3];
        value <<= 8u;
        value |= iter[2];
        value <<= 8u;
        value |= iter[1];
        value <<= 8u;
        value |= iter[0];

        if (radix == 10)
        {
            if (is_signed)
            {
                fprintf(fp, "%8d ", (int32_t)value);
            }
            else
            {
                fprintf(fp, "%8u ", value);
            }
        }
        else if (radix == 16)
        {
            n_written += fprintf(fp, "%08X ", value);
        }
    }

    return n_written;
}

size_t ex10_print_data(FILE*           fp,
                       void const*     data,
                       size_t          length,
                       enum DataPrefix prefix)
{
    size_t const         bytes_per_line = 16u;
    size_t               n_write        = 0u;
    uint8_t const*       data_ptr       = (uint8_t const*)data;
    uint8_t const* const end_ptr        = data_ptr + length;

    for (size_t iter = 0u; data_ptr < end_ptr;
         iter += bytes_per_line, data_ptr += bytes_per_line)
    {
        size_t const bytes_remaining = length - iter;
        size_t const bytes_to_write  = (bytes_remaining < bytes_per_line)
                                          ? bytes_remaining
                                          : bytes_per_line;
        switch (prefix)
        {
            case DataPrefixNone:
                break;
            case DataPrefixIndex:
                n_write += fprintf(fp, "%04zx: ", iter);
                break;
            case DataPrefixAddress:
                n_write += fprintf(fp, "%p: ", data_ptr);
                break;
            default:
                break;
        }

        n_write += ex10_print_data_line(fp, data_ptr, bytes_to_write);
        n_write += fprintf(fp, "\n");
    }
    return n_write;
}

size_t ex10_print_int32(FILE*           fp,
                        void const*     data,
                        size_t          length,
                        uint8_t         radix,
                        bool            is_signed,
                        enum DataPrefix prefix)
{
    size_t const words_per_line = 4u;
    size_t       n_write        = 0u;

    for (size_t index = 0u; index < length; index += words_per_line)
    {
        size_t const words_remaining = length - index;
        size_t const words_to_write  = (words_remaining < words_per_line)
                                          ? words_remaining
                                          : words_per_line;

        uint8_t const* data_ptr =
            (uint8_t const*)data + index * sizeof(uint32_t);

        switch (prefix)
        {
            case DataPrefixNone:
                break;
            case DataPrefixIndex:
                n_write += fprintf(fp, "%04zx: ", index);
                break;
            case DataPrefixAddress:
                n_write += fprintf(fp, "%p: ", data_ptr);
                break;
            default:
                break;
        }

        n_write += ex10_print_int32_line(
            fp, data_ptr, words_to_write, radix, is_signed);
        n_write += fprintf(fp, "\n");
    }
    return n_write;
}
