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

#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/bootloader_registers.h"
#include "ex10_api/trace.h"
#include "ex10_api/version_info.h"


uint8_t image_array[MAX_IMAGE_BYTES];


static struct ConstByteSpan read_in_image(char* image_file)
{
    // Attempt to open the file
    FILE* file = fopen(image_file, "r");
    assert(file != 0 && "Issue opening file");

    fseek(file, 0, SEEK_END);
    size_t file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    int result = fread(image_array, file_len, 1, file);
    assert(result != 0 && "Issue reading file");

    struct ConstByteSpan image = {
        .data   = image_array,
        .length = file_len,
    };

    fclose(file);
    return image;
}

static int app_upload_example(struct Ex10Protocol const* protocol,
                              char*                      file_with_image)
{
    // Locally read in entire image from file
    struct ConstByteSpan image_info = read_in_image(file_with_image);
    assert(image_info.length != 0 && "Image file was empty");

    // Reset into the bootloader and hold it there using the READY_N line.
    // In the case of a non-responsive application this hard reset and entry
    // to the bootloader is needed.
    protocol->reset(Bootloader);
    assert(protocol->get_running_location() == Bootloader);

    printf("Uploading image...\n");
    protocol->upload_image(UploadFlash, image_info);
    printf("Done\n");

    protocol->reset(Application);

    char                       ver_info[VERSION_STRING_SIZE];
    struct ImageValidityFields image_validity;
    get_ex10_version()->get_application_info(
        ver_info, sizeof(ver_info), &image_validity, NULL);

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

    return (protocol->get_running_location() != Application);
}


int main(int argc, char* argv[])
{
    tracepoint(pi_ex10sdk, EXEC_start, __FILE__);
    int result = 0;

    // Ensure argc is 2 meaning the image file was specified
    // EX: sudo app_upload_example bin_file
    if (argc != 2)
    {
        printf("No file passed in to upload.");
        return result;
    }
    // The argument input is the image file
    char* file_name = argv[1];

    struct Ex10Protocol const* ex10_protocol =
        ex10_bootloader_board_setup(BOOTLOADER_SPI_CLOCK_HZ);
    result = app_upload_example(ex10_protocol, file_name);

    ex10_bootloader_board_teardown();
    tracepoint(pi_ex10sdk, EXEC_end, __FILE__);

    return result;
}
