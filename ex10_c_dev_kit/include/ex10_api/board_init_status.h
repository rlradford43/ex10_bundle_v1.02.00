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

#include "ex10_api/ex10_ops.h"

/**
 * @struct Ex10BoardInitStatus
 * An aggregate of board initialization results which are encountered during
 * the power up process.
 */
struct Ex10BoardInitStatus
{
    /**
     * This field contains the result from the call to
     * Ex10Protocol.power_up_to_application(). Positive values indicate that
     * the device powered up into ether the bootloader or the application.
     * Negative values indicate an error.
     * @see enum Status and Ex10Protocol.power_up_to_application().
     */
    int power_up_status;

    /**
     * This field contains the error result code returned when
     * Ex10Protocol.init_ex10() was called. If non-zero then this value
     * represents a POSIX-like error that occurred. If zero, then the call
     * to Ex10Protocol.init_ex10() was successful.
     */
    int protocol_error;

    /**
     * When the successive calls to the init_ex10() function within the
     * Ex10Protocol, Ex10Ops, Ex10Reader objects is made, this field contains
     * the result of an error encountered. If no errors were encountered then
     * this member's .error_occurred field will be set to false.
     */
    struct OpCompletionStatus op_status;
};
