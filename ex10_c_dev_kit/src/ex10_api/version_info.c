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
#include <string.h>

#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/version_info.h"

static const char* get_device_info(void)
{
    struct Ex10Protocol const* ex10_protocol = get_ex10_protocol();
    struct DeviceInfoFields    dev_info;

    if (ex10_protocol->get_running_location() == Application)
    {
        ex10_protocol->read(&device_info_reg, &dev_info);
    }
    else
    {
        uint8_t git_hash_buffer[bootloader_git_hash_reg.length];
        ex10_protocol->read(&bootloader_git_hash_reg, git_hash_buffer);
        uint32_t bootloader_git_hash =
            (git_hash_buffer[0] << 24) | (git_hash_buffer[1] << 16) |
            (git_hash_buffer[2] << 8) | (git_hash_buffer[3]);

        switch (bootloader_git_hash)
        {
            case 0xb3a01818:
                dev_info.eco_revision       = 0;
                dev_info.device_revision_lo = 4;
                dev_info.device_revision_hi = 0;
                dev_info.device_identifier  = 1;
                break;

            case 0x804499bc:
                dev_info.eco_revision       = 3;
                dev_info.device_revision_lo = 3;
                dev_info.device_revision_hi = 0;
                dev_info.device_identifier  = 1;
                break;

            default:
                break;
        }
    }

    uint16_t device_revision = (uint16_t)(dev_info.device_revision_hi << 8 |
                                          dev_info.device_revision_lo);

    static char device_info_str[50];
    sprintf(device_info_str,
            "Device:\n  eco: %d\n  rev: %d\n  id:  %d",
            dev_info.eco_revision,
            device_revision,
            dev_info.device_identifier);
    return device_info_str;
}

static size_t fill_version_string(char*          buffer,
                                  size_t         buffer_length,
                                  char const*    app_or_bl,
                                  char const*    version_buffer,
                                  uint8_t const* git_hash_buffer,
                                  size_t         git_hash_length,
                                  uint32_t       build_no)
{
    size_t offset = 0u;
    if (buffer_length > offset + 1u)
    {
        offset += snprintf(&buffer[offset],
                           buffer_length - offset,
                           "%s:\n  version: %s\n",
                           app_or_bl,
                           version_buffer);
    }

    char const git_hash_tag[] = "  git hash: ";
    // The length of the git hash string includes the string null terminator +
    // 2 characters for each hash byte. The null gets replaced with newline.
    size_t const git_hash_str_len =
        sizeof(git_hash_tag) + (2u * git_hash_length);

    if (buffer_length > offset + git_hash_str_len)
    {
        memcpy(&buffer[offset], git_hash_tag, sizeof(git_hash_tag) - 1u);
        offset += sizeof(git_hash_tag) - 1u;
        for (size_t iter = 0u; iter < git_hash_length; ++iter)
        {
            offset += snprintf(&buffer[offset],
                               buffer_length - offset,
                               "%02x",
                               git_hash_buffer[iter]);
        }
        buffer[offset++] = '\n';
    }

    if (buffer_length > offset + 1u)
    {
        offset += snprintf(&buffer[offset],
                           buffer_length - offset,
                           "  build no: %d",
                           build_no);
    }

    return offset;
}

static size_t get_application_info(
    char*                       buffer,
    size_t                      buffer_length,
    struct ImageValidityFields* image_validity_buf,
    struct RemainReasonFields*  remain_reason_buf)
{
    struct Ex10Protocol const* ex10_protocol = get_ex10_protocol();
    char                       version_buffer[version_string_reg.length];
    uint8_t                    git_hash_buffer[git_hash_reg.length];
    uint32_t                   build_no = 0u;

    memset(version_buffer, 0x00, version_string_reg.length);
    memset(git_hash_buffer, 0x00, git_hash_reg.length);

    ex10_protocol->read(&version_string_reg, version_buffer);
    ex10_protocol->read(&git_hash_reg, git_hash_buffer);
    ex10_protocol->read(&build_number_reg, &build_no);

    if (ex10_protocol->get_running_location() == Application)
    {
        if (image_validity_buf)
        {
            image_validity_buf->image_valid_marker     = true;
            image_validity_buf->image_non_valid_marker = false;
        }

        if (remain_reason_buf)
        {
            remain_reason_buf->remain_reason = RemainReasonNoReason;
        }
    }
    else
    {
        if (image_validity_buf)
        {
            ex10_protocol->read(&image_validity_reg, image_validity_buf);
        }

        if (remain_reason_buf)
        {
            ex10_protocol->read(&remain_reason_reg, remain_reason_buf);
        }
    }

    return fill_version_string(buffer,
                               buffer_length,
                               "Application",
                               version_buffer,
                               git_hash_buffer,
                               git_hash_reg.length,
                               build_no);
}

static size_t get_bootloader_info(char* buffer, size_t buffer_length)
{
    struct Ex10Protocol const* ex10_protocol = get_ex10_protocol();

    ex10_protocol->reset(Bootloader);
    assert(ex10_protocol->get_running_location() == Bootloader);

    char version_buffer[bootloader_version_string_reg.length];
    ex10_protocol->read(&bootloader_version_string_reg, version_buffer);

    uint8_t git_hash_buffer[bootloader_git_hash_reg.length];
    ex10_protocol->read(&bootloader_git_hash_reg, git_hash_buffer);

    uint32_t build_no = 0u;
    ex10_protocol->read(&bootloader_build_number_reg, &build_no);

    return fill_version_string(buffer,
                               buffer_length,
                               "Bootloader",
                               version_buffer,
                               git_hash_buffer,
                               git_hash_reg.length,
                               build_no);
}

static enum ProductSku get_sku(void)
{
    struct Ex10Protocol const* ex10_protocol = get_ex10_protocol();

    if (ex10_protocol->get_running_location() != Application)
    {
        return SkuUnknown;
    }

    uint8_t buffer[product_sku_reg.length];
    ex10_protocol->read(&product_sku_reg, buffer);
    enum ProductSku sku_val = (buffer[1] << 8) | buffer[0];

    return sku_val;
}

static struct Ex10Version const ex10_version = {
    .get_bootloader_info  = get_bootloader_info,
    .get_application_info = get_application_info,
    .get_sku              = get_sku,
    .get_device_info      = get_device_info,
};

struct Ex10Version const* get_ex10_version(void)
{
    return &ex10_version;
}