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

#include "board/board_spec.h"
#include "ex10_api/command_transactor.h"

#include "ex10_api/byte_span.h"
#include "ex10_api/print_data.h"
#include "ex10_api/trace.h"

static struct ConstByteSpan last_command = {.data = NULL, .length = 0u};

struct CommandTransactor
{
    struct Ex10GpioInterface const* gpio_interface;
    struct HostInterface const*     host_interface;
};

static struct CommandTransactor command_transactor = {.gpio_interface = NULL,
                                                      .host_interface = NULL};

static void init(struct Ex10GpioInterface const* gpio_interface,
                 struct HostInterface const*     host_interface)
{
    command_transactor.gpio_interface = gpio_interface;
    command_transactor.host_interface = host_interface;
}

static void deinit(void)
{
    command_transactor.gpio_interface = NULL;
    command_transactor.host_interface = NULL;
}

static void send_command(const void* command_buffer,
                         size_t      command_length,
                         uint32_t    ready_n_timeout_ms)
{
    last_command.data   = (uint8_t const*)command_buffer;
    last_command.length = command_length;

    assert(command_transactor.gpio_interface != NULL);
    assert(command_transactor.host_interface != NULL);

    tracepoint(pi_ex10sdk, CMD_send, command_buffer, command_length);

    command_transactor.gpio_interface->busy_wait_ready_n(ready_n_timeout_ms);
    int32_t const bytes_sent = command_transactor.host_interface->write(
        command_buffer, command_length);
    assert(bytes_sent >= 0);
    assert((uint32_t)bytes_sent == command_length);
}

static size_t receive_response(void*    response_buffer,
                               size_t   response_buffer_length,
                               uint32_t ready_n_timeout_ms)
{
    assert(command_transactor.gpio_interface != NULL);
    assert(command_transactor.host_interface != NULL);

    bool warning = false;

    // Note: The Ex10 can put 1 more byte in the response buffer than it
    // can accept in the command buffer.
    if (response_buffer_length > EX10_SPI_BURST_SIZE + 1u)
    {
        fprintf(stderr,
                "error: receive_response: last command: %u: "
                "response_buffer_length: %zu > EX10_SPI_BURST_SIZE: %zu\n",
                *last_command.data,
                response_buffer_length,
                EX10_SPI_BURST_SIZE);

        response_buffer_length = EX10_SPI_BURST_SIZE + 1u;
        warning                = true;
    }

    command_transactor.gpio_interface->busy_wait_ready_n(ready_n_timeout_ms);
    int32_t const bytes_received = command_transactor.host_interface->read(
        response_buffer, response_buffer_length);

    if (bytes_received == 0u)
    {
        fprintf(stderr, "error: response length: %u \n", bytes_received);
        warning = true;
    }

    if (warning)
    {
        fprintf(stderr, "commmand: %u\n", *last_command.data);
        ex10_print_data(
            stderr, last_command.data, last_command.length, DataPrefixIndex);

        fprintf(stderr, "response:\n");
        ex10_print_data(
            stderr, response_buffer, bytes_received, DataPrefixIndex);
    }

    tracepoint(pi_ex10sdk, CMD_recv, response_buffer, response_buffer_length);

    return bytes_received;
}

static size_t send_and_recv_bytes(const void* command_buffer,
                                  size_t      command_length,
                                  void*       response_buffer,
                                  size_t      response_buffer_length,
                                  uint32_t    ready_n_timeout_ms)
{
    send_command(command_buffer, command_length, ready_n_timeout_ms);
    return receive_response(
        response_buffer, response_buffer_length, ready_n_timeout_ms);
}

static const struct Ex10CommandTransactor ex10_command_transactor = {
    .init                = init,
    .deinit              = deinit,
    .send_command        = send_command,
    .receive_response    = receive_response,
    .send_and_recv_bytes = send_and_recv_bytes,
};

struct Ex10CommandTransactor const* get_ex10_command_transactor(void)
{
    return &ex10_command_transactor;
}
