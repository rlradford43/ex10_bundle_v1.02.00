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

#include "board/time_helpers.h"
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>


// Returns 0 as a default, but this should be overridden in the sim
// to return the appropriate simulation time.
static uint32_t ex10_time_now(void)
{
    return 0;
}

// Returns 0 as a default, but this should be overridden in the sim
// to return the appropriate simulation time elapsed.
static uint32_t ex10_time_elapsed(uint32_t start_time)
{
    return (get_ex10_time_helpers()->time_now() - start_time);
}

// We ignore calls to wait in the sims to ensure we do not wait
// for long periods of time.
static void ex10_busy_wait_ms(uint32_t msec_to_wait)
{
    (void)msec_to_wait;
}

// We ignore calls to wait in the sims to ensure we do not wait
// for long periods of time.
static void ex10_wait_ms(uint32_t msec_to_wait)
{
    (void)msec_to_wait;
}

static struct Ex10TimeHelpers ex10_time_helpers = {
    .time_now     = ex10_time_now,
    .time_elapsed = ex10_time_elapsed,
    .busy_wait_ms = ex10_busy_wait_ms,
    .wait_ms      = ex10_wait_ms,
};

struct Ex10TimeHelpers* get_ex10_time_helpers(void)
{
    return &ex10_time_helpers;
}
