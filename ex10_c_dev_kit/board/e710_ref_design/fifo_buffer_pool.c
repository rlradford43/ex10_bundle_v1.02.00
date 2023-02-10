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

#include "board/fifo_buffer_pool.h"
#include "ex10_api/event_fifo_packet_types.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/**
 * Each buffer needs to be large enough to contain the full contents of
 * the ReadFifo command (4096 bytes), plus the 1-byte result code,
 * and maintain Fifo packet 4-byte alignment.
 */
#define EVENT_FIFO_BUFFER_SIZE (EX10_EVENT_FIFO_SIZE + FIFO_HEADER_SPACE)

/**
 * Note that the number of buffers should be changed based on the expected event
 * FIFO traffic and available memory on your host controller. For example, if
 * you have too few buffers and a large number of tags in a short window of
 * time, you may not have enough space to pull them from the device, and thus
 * the device-side event FIFO buffer could overfill.
 */
static uint8_t buffer_0[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_1[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_2[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_3[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_4[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_5[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_6[EVENT_FIFO_BUFFER_SIZE];
static uint8_t buffer_7[EVENT_FIFO_BUFFER_SIZE];

static struct ByteSpan const event_fifo_buffers[] = {
    {.data = buffer_0, .length = sizeof(buffer_0)},
    {.data = buffer_1, .length = sizeof(buffer_1)},
    {.data = buffer_2, .length = sizeof(buffer_2)},
    {.data = buffer_3, .length = sizeof(buffer_3)},
    {.data = buffer_4, .length = sizeof(buffer_4)},
    {.data = buffer_5, .length = sizeof(buffer_5)},
    {.data = buffer_6, .length = sizeof(buffer_6)},
    {.data = buffer_7, .length = sizeof(buffer_7)},
};

static struct FifoBufferNode
    event_fifo_buffer_nodes[ARRAY_SIZE(event_fifo_buffers)];

static struct FifoBufferPool const event_fifo_buffer_pool = {
    .fifo_buffer_nodes = event_fifo_buffer_nodes,
    .fifo_buffers      = event_fifo_buffers,
    .buffer_count      = ARRAY_SIZE(event_fifo_buffers)};

struct FifoBufferPool const* get_ex10_event_fifo_buffer_pool(void)
{
    return &event_fifo_buffer_pool;
}
