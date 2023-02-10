/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2021 Impinj, Inc. All rights reserved.                      *
 *                                                                           *
 *****************************************************************************/

#pragma once

#include <stddef.h>

#include "ex10_api/byte_span.h"
#include "ex10_api/fifo_buffer_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_HEADER_SPACE 4u

struct FifoBufferPool
{
    struct FifoBufferNode* fifo_buffer_nodes;
    struct ByteSpan const* fifo_buffers;
    size_t const           buffer_count;
};

struct FifoBufferPool const* get_ex10_event_fifo_buffer_pool(void);

#ifdef __cplusplus
}
#endif
