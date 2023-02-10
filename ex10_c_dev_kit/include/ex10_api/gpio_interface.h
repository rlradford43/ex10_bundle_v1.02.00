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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct Ex10GpioInterface
 * This interface is a pass-through to the Ex10GpioDriver interface.
 * @see  struct Ex10GpioDriver for a description of these functions.
 * @note Not all of the Ex10GpioDriver functions are exposed by the
 *       Ex10GpioInterface interface.
 */
struct Ex10GpioInterface
{
    void (*initialize)(bool board_power_on, bool ex10_enable, bool reset);
    void (*cleanup)(void);

    void (*set_board_power)(bool power_on);
    bool (*get_board_power)(void);

    void (*set_ex10_enable)(bool enable);
    bool (*get_ex10_enable)(void);

    int (*register_irq_callback)(void (*cb_func)(void));
    int (*deregister_irq_callback)(void);
    void (*irq_enable)(bool);
    bool (*thread_is_irq_monitor)(void);

    void (*assert_reset_n)(void);
    void (*deassert_reset_n)(void);
    void (*reset_device)(void);

    void (*assert_ready_n)(void);
    void (*release_ready_n)(void);

    int (*busy_wait_ready_n)(uint32_t ready_n_timeout_ms);
    int (*ready_n_pin_get)(void);
};

#ifdef __cplusplus
}
#endif
