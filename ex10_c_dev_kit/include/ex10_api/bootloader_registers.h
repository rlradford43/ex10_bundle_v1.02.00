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
// Data structure definition and initialization
// for all bootloader registers. This contains the basic
// register info and a pointer to the register data.

#pragma once

#include <stddef.h>

#include "bootloader_register_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
// IPJ_autogen | generate_bootloader_ex10_api_c_reg_instances {

static struct RegisterInfo const ram_image_return_value_reg = {
    .name = "RamImageReturnValue",
    .address = 0x0030,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const fref_freq_bootloader_reg = {
    .name = "FrefFreq",
    .address = 0x0034,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadWrite,
};
static struct RegisterInfo const remain_reason_reg = {
    .name = "RemainReason",
    .address = 0x0038,
    .length = 0x0001,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const image_validity_reg = {
    .name = "ImageValidity",
    .address = 0x0039,
    .length = 0x0001,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const bootloader_version_string_reg = {
    .name = "BootloaderVersionString",
    .address = 0x003A,
    .length = 0x0020,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const bootloader_build_number_reg = {
    .name = "BootloaderBuildNumber",
    .address = 0x005A,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const bootloader_git_hash_reg = {
    .name = "BootloaderGitHash",
    .address = 0x005E,
    .length = 0x0004,
    .num_entries = 1,
    .access = ReadOnly,
};
static struct RegisterInfo const crash_info_reg = {
    .name = "CrashInfo",
    .address = 0x0100,
    .length = 0x0100,
    .num_entries = 1,
    .access = ReadOnly,
};
// IPJ_autogen }
// clang-format on

#ifdef __cplusplus
}
#endif
