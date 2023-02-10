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

#include "calibration.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/bootloader_registers.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_ops.h"
#include "ex10_api/trace.h"
#include "ex10_api/version_info.h"


#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void calibration_version_info(struct Ex10Protocol const* ex10_protocol)
{
    // Attempt to reset into the Application
    ex10_protocol->reset(Application);

    printf("Calibration version: ");

    if (ex10_protocol->get_running_location() == Application)
    {
        struct Ex10Calibration const* calibration =
            get_ex10_calibration(ex10_protocol);

        printf("%u\n", calibration->get_cal_version());
    }
    else
    {
        // In the bootloader,
        // the calibration cannot be read via the Read command.
        printf("Unknown\n");
    }
}

static void sku_info(struct Ex10Protocol const* ex10_protocol)
{
    // Attempt to reset into the Application
    ex10_protocol->reset(Application);

    // Read the sku back from the device
    uint16_t sku_val = get_ex10_version()->get_sku();

    printf("SKU: ");
    switch (sku_val)
    {
        case SkuE310:
            printf("E310\n");
            break;
        case SkuE510:
            printf("E510\n");
            break;
        case SkuE710:
            printf("E710\n");
            break;
        case SkuE910:
            printf("E910\n");
            break;
        case SkuUnknown:
        default:
            printf("Unknown\n");
            break;
    }
}

static void device_info(void)
{
    const char* dev_info = get_ex10_version()->get_device_info();
    printf("%s\n", dev_info);
}

static void app_version_info(struct Ex10Protocol const* ex10_protocol)
{
    char                       ver_info[VERSION_STRING_SIZE];
    struct ImageValidityFields image_validity;
    struct RemainReasonFields  remain_reason;

    // Attempt to reset into the Application
    ex10_protocol->reset(Application);

    get_ex10_version()->get_application_info(
        ver_info, sizeof(ver_info), &image_validity, &remain_reason);
    printf("%s\n", ver_info);

    if ((image_validity.image_valid_marker) &&
        !(image_validity.image_non_valid_marker))
    {
        printf("Application image VALID\n");
    }
    else
    {
        printf("Application image INVALID\n");
    }

    printf("Remain in bootloader reason: %s\n",
           get_ex10_helpers()->get_remain_reason_string(
               remain_reason.remain_reason));
}

static void bootloader_version_info(void)
{
    char ver_info[VERSION_STRING_SIZE];

    get_ex10_version()->get_bootloader_info(ver_info, sizeof(ver_info));
    printf("%s\n", ver_info);
}

static void print_help(void)
{
    printf("No args given. Accepted arguments are...\n");
    printf("a: returns application info,\n");
    printf("b: returns bootloader info,\n");
    printf("c: returns calibration info\n");
    printf("d: returns device info,\n");
    printf("s: returns sku\n");
}

int main(int argc, char* argv[])
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    int result = 0;

    // Initialize version_list to print all.
    // The default set of versions to print when no arguments are specified.
    char         version_list[]    = {'a', 'b', 'c', 'd', 's'};
    size_t const version_list_size = ARRAY_SIZE(version_list);
    size_t       param_length      = version_list_size;

    if (argc > 1)
    {
        // Command arguments specified.
        // argc is all params and 1 count for the script itself
        param_length = argc - 1;
        if (param_length > version_list_size)
        {
            fprintf(stderr,
                    "warning: command line argument count %zu "
                    "exceeds allowed count %zu, extra arguments dropped\n",
                    param_length,
                    version_list_size);
            param_length = version_list_size;
        }

        for (size_t i = 0; i < param_length; i++)
        {
            // skip the script name in argv
            version_list[i] = *(argv[i + 1]);
        }
    }

    struct Ex10Protocol const* ex10_protocol =
        ex10_bootloader_board_setup(BOOTLOADER_SPI_CLOCK_HZ);

    for (size_t i = 0; i < param_length; i++)
    {
        // The argument input is the image file
        char version_arg = version_list[i];

        switch (version_arg)
        {
            case 'a':
                app_version_info(ex10_protocol);
                break;
            case 'b':
                bootloader_version_info();
                break;
            case 'c':
                calibration_version_info(ex10_protocol);
                break;
            case 'd':
                device_info();
                break;
            case 's':
                sku_info(ex10_protocol);
                break;
            default:
                print_help();
                return 1;
                break;
        }
    }

    ex10_typical_board_teardown();

    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);
    return result;
}
