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

//
// Field definitions for private registers in an EX10 device.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "application_register_field_enums_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off

// IPJ_autogen | generate_application_ex10_api_c_reg_internal {

// Structs which break down the fields and sizing within each register
#pragma pack(push, 1)

struct RxExternalLoFields {
    bool enable : 1;
    int8_t rfu : 7;
};

struct ModemDataControlFields {
    bool trl_data : 1;
    bool all_packets : 1;
    bool rn16_empty : 1;
    bool rn16_single : 1;
    bool rn16_collided_bd : 1;
    bool rn16_collided_trl : 1;
    bool rn16_single_below_rssi : 1;
    bool epc_pass_crc : 1;
    bool epc_fail_crc : 1;
    bool epc_single_below_rssi : 1;
    uint8_t Reserved0 : 6;
    uint16_t number_of_samples : 16;
};

struct MultiToneControlFields {
    uint32_t csel : 32;
};

struct CommandValidatorOverrideFields {
    bool force_command_invalid : 1;
    bool force_tid_invalid : 1;
    int8_t rfu : 6;
};


#pragma pack(pop)
// IPJ_autogen }
// clang-format on

#ifdef __cplusplus
}
#endif
