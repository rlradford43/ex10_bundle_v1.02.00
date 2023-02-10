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

#include <assert.h>
#include <pthread.h>

#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/fifo_buffer_list.h"
#include "ex10_api/linked_list.h"

// The list of FifoBufferNodes for use in reading the Ex10 Event Fifo
// using the ReadFifo command.
static struct Ex10LinkedList event_fifo_free_list;

static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool event_fifo_free_list_put(
    struct FifoBufferNode* event_fifo_buffer_node)
{
    pthread_mutex_lock(&list_mutex);
    bool const is_empty = list_is_empty(&event_fifo_free_list);

    // It is not necessary that fifo_data.length be set to zero,
    // but it provides a sanity check w.r.t the state of the buffer.
    event_fifo_buffer_node->fifo_data.length = 0u;
    list_push_back(&event_fifo_free_list, &event_fifo_buffer_node->list_node);

    pthread_mutex_unlock(&list_mutex);
    return is_empty;
}

static void fifo_buffer_list_init(struct FifoBufferNode* fifo_buffer_nodes,
                                  struct ByteSpan const* byte_spans,
                                  size_t                 buffer_count)
{
    list_init(&event_fifo_free_list);

    for (size_t index = 0u; index < buffer_count; ++index)
    {
        assert(byte_spans[index].data);

        // The location into the buffer to start reading data. This will keep
        // the fifo packets 32-bit aligned with respect to the start point and
        // allow space for the response code byte to be prepended to the stream
        // of fifo packets.
        uintptr_t const data_address = (uintptr_t)byte_spans[index].data;
        size_t const    data_length  = byte_spans[index].length;

        size_t const align  = sizeof(uint32_t);
        size_t const offset = align - data_address % align;
        size_t const length = ((data_length - offset) / align) * align;
        uint8_t*     data   = &byte_spans[index].data[offset];

        assert(length >= EX10_EVENT_FIFO_SIZE);

        fifo_buffer_nodes[index].raw_buffer.data   = data;
        fifo_buffer_nodes[index].raw_buffer.length = length;

        fifo_buffer_nodes[index].fifo_data.data   = data;
        fifo_buffer_nodes[index].fifo_data.length = 0u;

        get_ex10_list_node_helper()->init(&fifo_buffer_nodes[index].list_node);
        fifo_buffer_nodes[index].list_node.data = &fifo_buffer_nodes[index];
        event_fifo_free_list_put(&fifo_buffer_nodes[index]);
    }
}

static struct FifoBufferNode* event_fifo_free_list_get(void)
{
    pthread_mutex_lock(&list_mutex);

    struct FifoBufferNode* fifo_buffer = NULL;
    struct Ex10ListNode*   list_node   = list_front(&event_fifo_free_list);
    if (list_node->data)
    {
        list_pop_front(&event_fifo_free_list);
        fifo_buffer = (struct FifoBufferNode*)list_node->data;
    }

    pthread_mutex_unlock(&list_mutex);
    return fifo_buffer;
}

static size_t event_fifo_free_list_size(void)
{
    pthread_mutex_lock(&list_mutex);
    size_t const count = list_size(&event_fifo_free_list);
    pthread_mutex_unlock(&list_mutex);
    return count;
}

static struct FifoBufferList const ex10_fifo_buffer_list = {
    .init           = fifo_buffer_list_init,
    .free_list_put  = event_fifo_free_list_put,
    .free_list_get  = event_fifo_free_list_get,
    .free_list_size = event_fifo_free_list_size,
};

struct FifoBufferList const* get_ex10_fifo_buffer_list(void)
{
    return &ex10_fifo_buffer_list;
}