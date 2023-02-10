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

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "board/gpio_driver.h"
#include "board/region.h"
#include "board/time_helpers.h"
#include "ex10_api/application_registers.h"
#include "ex10_api/board_init.h"
#include "ex10_api/ex10_helpers.h"
#include "ex10_api/ex10_power_modes.h"
#include "ex10_api/ex10_reader.h"
#include "ex10_api/rf_mode_definitions.h"
#include "ex10_api/trace.h"

// static bool const verbose = true;
static uint32_t us_per_s = 1000u * 1000u;
static uint32_t ms_per_s = 1000u;

static uint8_t const select_all = 0u;
static uint8_t const target_A   = 0u;


/// Prints microsecond tick counts as seconds.milliseconds
static void print_microseconds(FILE* fp, uint32_t time_us)
{
    fprintf(
        fp, "%6u.%03u", time_us / us_per_s, (time_us % us_per_s) / ms_per_s);
}

/**
 * A continuous inventory helper function to call the
 * Ex10Helpers.continuous_inventory() function which in turn calls
 * Ex10Reader.continuous_inventory().
 *
 * @return int32_t An indication of success.
 * @retval >= 0    The number of tags found in the inventory.
 * @retval  < 0    An the negated InventoryHelperReturn value.
 */
static int32_t continuous_inventory(
    struct Ex10Interfaces const*  ex10,
    struct InventoryHelperParams* ihp,
    struct StopConditions const*  stop_conditions)
{
    printf("continuous inventory, duration: ");
    print_microseconds(stdout, stop_conditions->max_duration_us);
    printf("\n");
    struct InfoFromPackets packet_info = {0u, 0u, 0u, 0u, {0u}};
    ihp->packet_info                   = &packet_info;

    struct ContinuousInventorySummary continuous_inventory_summary = {0};

    struct ContInventoryHelperParams cihp = {
        .inventory_params = ihp,
        .stop_conditions  = stop_conditions,
        .summary_packet   = &continuous_inventory_summary,
    };

    enum InventoryHelperReturns const ret_val =
        ex10->helpers->continuous_inventory(&cihp);

    if (ret_val != InvHelperSuccess)
    {
        fprintf(stderr,
                "error: %s: Ex10Helpers.continuous_inventory() failed: %d\n",
                __func__,
                (int)ret_val);
        return -(int32_t)ret_val;
    }

    uint32_t const read_rate =
        continuous_inventory_summary.number_of_tags /
        (continuous_inventory_summary.duration_us / us_per_s);

    printf("Tag Read rate:       %6u\n", read_rate);
    printf("Number of tags read: %6u\n",
           continuous_inventory_summary.number_of_tags);
    printf("Numbers of seconds:  ");
    print_microseconds(stdout, continuous_inventory_summary.duration_us);
    printf("\n");
    printf("RF Mode:             %6u\n", ihp->rf_mode);

    if (continuous_inventory_summary.number_of_tags == 0)
    {
        fprintf(stderr, "warning: no tags found in inventory\n");
    }

    return continuous_inventory_summary.number_of_tags;
}

static char const* power_mode_string(enum PowerMode power_mode)
{
    switch (power_mode)
    {
        case PowerModeOff:
            return "PowerModeOff";
        case PowerModeStandby:
            return "PowerModeStandby";
        case PowerModeReadyCold:
            return "PowerModeReadyCold";
        case PowerModeReady:
            return "PowerModeReady";
        case PowerModeInvalid:
            return "PowerModeInvalid";
        default:
            return "PowerMode --unknown--";
    }
}

static void print_usage(FILE*          fp,
                        bool           as_default,
                        float          time_s_inventory,
                        float          time_s_low_power,
                        size_t         cycles,
                        enum PowerMode low_power_mode)
{
    char const* default_or_using = as_default ? " default" : "using";
    fprintf(fp,
            "-T time, in seconds, to run inventory,                "
            "%s: %6.1f seconds\n",
            default_or_using,
            time_s_inventory);

    fprintf(fp,
            "-t time, in seconds, to paused in low power mode,     "
            "%s: %6.1f seconds\n",
            default_or_using,
            time_s_low_power);

    fprintf(fp,
            "-n the number of inventory -> lower power iterations, "
            "%s: %4zu   cycles\n",
            default_or_using,
            cycles);

    fprintf(fp,
            "-p mode, the low power mode to use,                   "
            "%s: %4u   %s",
            default_or_using,
            low_power_mode,
            power_mode_string(low_power_mode));
    fprintf(fp, "\n");

    for (enum PowerMode power_mode = PowerModeOff;
         power_mode < PowerModeInvalid;
         ++power_mode)
    {
        fprintf(
            fp, "         %u: %s\n", power_mode, power_mode_string(power_mode));
    }
}

// In unparseable, returns error_value.
static long int parse_int(char const* str, int error_value)
{
    if (str == NULL)
    {
        fprintf(stderr, "error: missing arguement\n");
        return error_value;
    }

    char*          endp  = NULL;
    long int const value = strtol(str, &endp, 0);
    if (*endp != 0)
    {
        fprintf(stderr,
                "error: parsing %s as integer failed, pos: %c\n",
                str,
                *endp);
        return error_value;
    }

    return value;
}

static float parse_float(char const* str, float error_value)
{
    if (str == NULL)
    {
        fprintf(stderr, "error: missing arguement\n");
        return error_value;
    }

    char*       endp  = NULL;
    float const value = strtof(str, &endp);
    if (*endp != 0)
    {
        fprintf(
            stderr, "error: parsing %s as float failed, pos: %c\n", str, *endp);
        return error_value;
    }
    return value;
}

static int cycle_through_inventory_and_power_modes(
    struct Ex10Interfaces ex10,
    enum PowerMode        low_power_mode,
    uint32_t              time_us_inventory,
    uint32_t              time_ms_low_power,
    size_t                cycles)
{
    struct Ex10TimeHelpers const* time_helpers = get_ex10_time_helpers();
    struct Ex10GpioDriver const*  gpio_driver  = get_ex10_gpio_driver();

    // Host GPIO pins can be used to trigger instrumentation on specific
    // events:
    // - RPi GPIO pin 2, debug_pin(0):
    //   Falling edge indicates PowerModeOn operation running inventory.
    //   Rising  edge indicates the end of inventory.
    // - RPi GPIO pin 3, debug_pin(1):
    //   Falling edge indicates low power mode operation as specified
    //   on the command line.
    //   Rising edge indicates the end of low power mode.
    gpio_driver->debug_pin_set(0u, false);
    gpio_driver->debug_pin_set(1u, true);

    enum PowerMode power_mode = PowerModeInvalid;
    for (unsigned int iter = 0u; iter < cycles; ++iter)
    {
        struct InventoryRoundControlFields inventory_config = {
            .initial_q            = 8u,
            .max_q                = 15u,
            .min_q                = 0u,
            .num_min_q_cycles     = 1u,
            .fixed_q_mode         = false,
            .q_increase_use_query = false,
            .q_decrease_use_query = false,
            .session              = Session1,
            .select               = select_all,
            .target               = target_A,
            .halt_on_all_tags     = false,
            .tag_focus_enable     = false,
            .fast_id_enable       = false,
        };

        struct InventoryRoundControl_2Fields const inventory_config_2 = {
            .max_queries_since_valid_epc = 16u,
        };

        struct InventoryHelperParams inventory_params = {
            .antenna               = 1u,
            .rf_mode               = mode_11,
            .tx_power_cdbm         = 3000u,
            .inventory_config      = &inventory_config,
            .inventory_config_2    = &inventory_config_2,
            .send_selects          = false,
            .remain_on             = false,
            .dual_target           = true,
            .inventory_duration_ms = 0,
            .packet_info           = NULL,
            .verbose               = false,
        };

        // The goal is to inventory all tags within the field of view,
        // limiting the inventory rounds to 10 seconds if there are many
        // tags.
        struct StopConditions const stop_conditions = {
            .max_duration_us      = time_us_inventory,
            .max_number_of_rounds = 0u,
            .max_number_of_tags   = 0u,
        };

        printf("---------- iteration: %2u / %2zu:\n", iter + 1u, cycles);
        power_mode = ex10.power_modes->get_power_mode();
        printf("inventory power mode: %u, %s\n",
               power_mode,
               power_mode_string(power_mode));

        int const result =
            continuous_inventory(&ex10, &inventory_params, &stop_conditions);

        // At least one tag must be inventoried and no errors encountered.
        if (result <= 0)
        {
            return -1;
        }

        gpio_driver->debug_pin_toggle(0u);
        ex10.power_modes->set_power_mode(low_power_mode);
        gpio_driver->debug_pin_toggle(1u);
        power_mode = ex10.power_modes->get_power_mode();
        printf("low power mode: %u, %s\n",
               power_mode,
               power_mode_string(power_mode));
        time_helpers->busy_wait_ms(time_ms_low_power);

        gpio_driver->debug_pin_toggle(1u);
        ex10.power_modes->set_power_mode(PowerModeReady);
        gpio_driver->debug_pin_toggle(0u);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    float          time_s_inventory = 2.0;
    float          time_s_low_power = 2.0;
    enum PowerMode low_power_mode   = PowerModeOff;
    size_t         cycles           = 2u;

    char const* opt_spec = "T:t:p:n:h?";
    for (int opt_char = getopt(argc, argv, opt_spec); opt_char != -1;
         opt_char     = getopt(argc, argv, opt_spec))
    {
        switch (opt_char)
        {
            case 'T':
                time_s_inventory = parse_float(optarg, time_s_inventory);
                break;
            case 't':
                time_s_low_power = parse_float(optarg, time_s_low_power);
                break;
            case 'p':
                low_power_mode = parse_int(optarg, low_power_mode);
                break;
            case 'n':
                cycles = parse_int(optarg, cycles);
                break;
            case 'h':
            case '?':
                print_usage(stdout,
                            true,
                            time_s_inventory,
                            time_s_low_power,
                            cycles,
                            low_power_mode);
                return 0;
            default:
                fprintf(stderr,
                        "error: uknown argument specified: %c\n",
                        (char)opt_char);
                return -EINVAL;
                break;
        }
    }

    print_usage(stdout,
                false,
                time_s_inventory,
                time_s_low_power,
                cycles,
                low_power_mode);

    uint32_t const time_us_inventory = roundl(time_s_inventory * us_per_s);
    uint32_t const time_ms_low_power = roundl(time_s_low_power * ms_per_s);

    // Note: PowerModeReady can be used as a "low power mode". In this case
    // inventory will not be run, but the mode will be "Ready".
    bool const power_mode_ok =
        (low_power_mode >= PowerModeOff) && (low_power_mode <= PowerModeReady);
    if (power_mode_ok == false)
    {
        fprintf(stderr, "error: invalid PowerMode: %d\n", low_power_mode);
        return -EINVAL;
    }

    if (time_us_inventory == 0)
    {
        fprintf(stderr, "error: invalid time_us_inventory\n");
        return -EINVAL;
    }

    if (time_ms_low_power == 0)
    {
        fprintf(stderr, "error: invalid time_ms_low_power\n");
        return -EINVAL;
    }

    if (cycles == 0)
    {
        fprintf(stderr, "error: invalid cycles\n");
        return -EINVAL;
    }

    struct Ex10Interfaces const ex10 =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = ex10.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        result = cycle_through_inventory_and_power_modes(
            ex10, low_power_mode, time_us_inventory, time_ms_low_power, cycles);
    }

    ex10_typical_board_teardown();
    return result;
}
