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

// Data structure definition and initialization
// for private application registers. This contains the basic
// register info and a pointer to the register data.

#pragma once

#include <stddef.h>

#include "application_register_definitions_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
// IPJ_autogen | generate_application_ex10_api_c_reg_instances_private {

static struct RegisterInfo const rx_external_lo_reg = {
    .name = "RxExternalLo",
    .address = 0x00D8,
    .length = 0x0001,
    .num_entries = 1,
    .access = ReadWrite,
};
static struct RegisterInfo const modem_data_control_reg = {
    .name = "ModemDataControl",
    .address = 0x0340,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadWrite,
};
static struct RegisterInfo const multi_tone_control_reg = {
    .name = "MultiToneControl",
    .address = 0x0530,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadWrite,
};
static struct RegisterInfo const command_validator_override_reg = {
    .name = "CommandValidatorOverride",
    .address = 0x0534,
    .length = 0x0001,
    .num_entries = 1,
    .access = ReadWrite,
};
// IPJ_autogen }
// clang-format on

#ifdef __cplusplus
}
#endif
