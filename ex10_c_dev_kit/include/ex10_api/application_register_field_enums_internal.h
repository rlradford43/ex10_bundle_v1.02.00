/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2022 Impinj, Inc. All rights reserved.                      *
 *                                                                           *
 *****************************************************************************/
// enums used by the register fields

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
// IPJ_autogen | generate_application_ex10_api_c_reg_field_enums_private {

enum ResponseCodeInternal {
    ImageExecFailure             = 0x0c,
};

enum CommandCodeInternal {
    CommandCallRamImage          = 0x09,
};

enum OpIdInternal {
    MultiToneTestOp              = 0xc3,
    ExternalLoEnableOp           = 0xfc,
    WriteProfileDataOp           = 0xfd,
    CrashTestOp                  = 0xfe,
};
// IPJ_autogen }

// clang-format on

#ifdef __cplusplus
}
#endif
