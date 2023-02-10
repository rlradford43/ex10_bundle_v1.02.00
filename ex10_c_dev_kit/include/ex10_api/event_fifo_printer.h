/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2020 - 2022 Impinj, Inc. All rights reserved.               *
 *                                                                           *
 *****************************************************************************/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ex10_api/application_register_definitions.h"
#include "ex10_api/bootloader_registers.h"
#include "ex10_api/event_fifo_packet_types.h"
#include "ex10_api/event_packet_parser.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_protocol.h"
#include "ex10_api/gen2_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Ex10EventFifoPrinter
{
    /**
     * Print EventFifo packets based on their type to stdout.
     */
    size_t (*print_packets)(struct EventFifoPacket const* packet);

    /**
     * Print the TagReadData struct contents to stdout.
     */
    size_t (*print_tag_read_data)(struct TagReadData const* tag_read_data);
};

const struct Ex10EventFifoPrinter* get_ex10_event_fifo_printer(void);

#ifdef __cplusplus
}
#endif
