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

#include "ex10_api/application_register_field_enums.h"
#include "ex10_api/gen2_commands.h"

enum IpjError
{
    IpjErrorNone = 0,

    /** Parameter out of range, null pointer, etc. */
    IpjErrorBadParam,

    /** Pointer parameter is null */
    IpjErrorBadParamNullPointer,

    /** Parameter value is invalid */
    IpjErrorBadParamRange,

    /** Invalid register address */
    IpjErrorBadParamAddressInvalid,

    /** Parameter must be 32-bit aligned */
    IpjErrorBadParamAlignment,

    /** Unexpected result in function internals */
    IpjErrorUnexpectedResult,

    /** The bootloader or application run location */
    IpjErrorRunLocation,

    /** Maximum expected time exceeded */
    IpjErrorTimeout,

    /** An existing callback must first be unregistered */
    IpjErrorCallbackAlreadyRegistered,

    /** Insufficent memory when setting up a gen2 transaction */
    IpjErrorGenBufferOverflow,

    /* Ensure the enum does not exceed 8 bits */
    IpjErrorMax = 0xff,
};
