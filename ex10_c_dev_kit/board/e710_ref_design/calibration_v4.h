/*****************************************************************************
 *                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      *
 *                                                                           *
 * This source code is the property of Impinj, Inc. Your use of this source  *
 * code in whole or in part is subject to your applicable license terms      *
 * from Impinj.                                                              *
 * Contact support@impinj.com for a copy of the applicable Impinj license    *
 * terms.                                                                    *
 *                                                                           *
 * (c) Copyright 2021 Impinj, Inc. All rights reserved.                      *
 *                                                                           *
 *****************************************************************************/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ex10_api/ex10_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
// IPJ_autogen | generate_calibration_v4_h {
#pragma pack(push)
struct CalibrationVersionV4 {
    uint8_t cal_file_version;
};
static_assert(sizeof(struct CalibrationVersionV4) == 1,
              "Size of packet header not packed properly");
struct VersionStringsV4 {
    uint8_t power_detect_cal_type;
    uint8_t forward_power_cal_type;
    uint8_t power_detector_temp_comp_type;
    uint8_t forward_power_temp_comp_type;
    uint8_t power_detector_freq_comp_type;
    uint8_t forward_power_freq_comp_type;
};
static_assert(sizeof(struct VersionStringsV4) == 6,
              "Size of packet header not packed properly");
struct UserBoardIdV4 {
    uint16_t user_board_id;
};
static_assert(sizeof(struct UserBoardIdV4) == 2,
              "Size of packet header not packed properly");
struct TxScalarCalV4 {
    int16_t tx_scalar_cal;
};
static_assert(sizeof(struct TxScalarCalV4) == 2,
              "Size of packet header not packed properly");
struct PerBandRfFilterV4 {
    float low_freq_limit;
    float high_freq_limit;
};
static_assert(sizeof(struct PerBandRfFilterV4) == 8,
              "Size of packet header not packed properly");
struct ValidPdetAdcsV4 {
    uint16_t valid_min_adc;
    uint16_t valid_max_adc;
};
static_assert(sizeof(struct ValidPdetAdcsV4) == 4,
              "Size of packet header not packed properly");
struct ControlLoopParamsV4 {
    uint16_t loop_gain_divisor;
    uint8_t error_threshold;
    uint8_t max_iterations;
};
static_assert(sizeof(struct ControlLoopParamsV4) == 4,
              "Size of packet header not packed properly");
struct PerBandPdetAdcLutV4 {
    uint16_t pdet0_adc_lut[31u];
    uint16_t pdet1_adc_lut[31u];
    uint16_t pdet2_adc_lut[31u];
};
static_assert(sizeof(struct PerBandPdetAdcLutV4) == 186,
              "Size of packet header not packed properly");
struct PerBandFwdPowerCoarsePwrCalV4 {
    float coarse_attn_cal[31u];
};
static_assert(sizeof(struct PerBandFwdPowerCoarsePwrCalV4) == 124,
              "Size of packet header not packed properly");
struct PerBandFwdPowerTempSlopeV4 {
    float fwd_power_temp_slope;
};
static_assert(sizeof(struct PerBandFwdPowerTempSlopeV4) == 4,
              "Size of packet header not packed properly");
struct PerBandCalTempV4 {
    uint16_t cal_temp_a_d_c;
};
static_assert(sizeof(struct PerBandCalTempV4) == 2,
              "Size of packet header not packed properly");
struct PerBandLoPdetTempSlopeV4 {
    float lo_pdet_temp_slope[3u];
};
static_assert(sizeof(struct PerBandLoPdetTempSlopeV4) == 12,
              "Size of packet header not packed properly");
struct PerBandLoPdetFreqLutV4 {
    int16_t lo_pdet_freq_adc_shifts0[4u];
    int16_t lo_pdet_freq_adc_shifts1[4u];
    int16_t lo_pdet_freq_adc_shifts2[4u];
};
static_assert(sizeof(struct PerBandLoPdetFreqLutV4) == 24,
              "Size of packet header not packed properly");
struct PerBandLoPdetFreqsV4 {
    float lo_pdet_freqs[4u];
};
static_assert(sizeof(struct PerBandLoPdetFreqsV4) == 16,
              "Size of packet header not packed properly");
struct PerBandFwdPwrFreqLutV4 {
    float fwd_pwr_shifts[4u];
};
static_assert(sizeof(struct PerBandFwdPwrFreqLutV4) == 16,
              "Size of packet header not packed properly");
struct DcOffsetCalV4 {
    int32_t dc_offset[31u];
};
static_assert(sizeof(struct DcOffsetCalV4) == 124,
              "Size of packet header not packed properly");
#pragma pack(pop)

struct Ex10CalibrationParamsV4 {
    struct CalibrationVersionV4             calibration_version;
    struct VersionStringsV4                 version_strings;
    struct UserBoardIdV4                    user_board_id;
    struct TxScalarCalV4                    tx_scalar_cal;
    struct PerBandRfFilterV4                rf_filter_upper_band;
    struct PerBandRfFilterV4                rf_filter_lower_band;
    struct ValidPdetAdcsV4                  valid_pdet_adcs;
    struct ControlLoopParamsV4              control_loop_params;
    struct PerBandPdetAdcLutV4              upper_band_pdet_adc_lut;
    struct PerBandFwdPowerCoarsePwrCalV4    upper_band_fwd_power_coarse_pwr_cal;
    struct PerBandFwdPowerTempSlopeV4       upper_band_fwd_power_temp_slope;
    struct PerBandCalTempV4                 upper_band_cal_temp;
    struct PerBandLoPdetTempSlopeV4         upper_band_lo_pdet_temp_slope;
    struct PerBandLoPdetFreqLutV4           upper_band_lo_pdet_freq_lut;
    struct PerBandLoPdetFreqsV4             upper_band_lo_pdet_freqs;
    struct PerBandFwdPwrFreqLutV4           upper_band_fwd_pwr_freq_lut;
    struct PerBandPdetAdcLutV4              lower_band_pdet_adc_lut;
    struct PerBandFwdPowerCoarsePwrCalV4    lower_band_fwd_power_coarse_pwr_cal;
    struct PerBandFwdPowerTempSlopeV4       lower_band_fwd_power_temp_slope;
    struct PerBandCalTempV4                 lower_band_cal_temp;
    struct PerBandLoPdetTempSlopeV4         lower_band_lo_pdet_temp_slope;
    struct PerBandLoPdetFreqLutV4           lower_band_lo_pdet_freq_lut;
    struct PerBandLoPdetFreqsV4             lower_band_lo_pdet_freqs;
    struct PerBandFwdPwrFreqLutV4           lower_band_fwd_pwr_freq_lut;
    struct DcOffsetCalV4                    dc_offset_cal;
};
// IPJ_autogen }
// clang-format on

struct Ex10CalibrationV4
{
    /**
     * Read the calibration parameters from the Ex10 into the C library
     * struct CalibrationParameters store.
     *
     * @param ex10_protocol The protocol object used to communicate with the
     * Ex10.
     */
    void (*init)(struct Ex10Protocol const* ex10_protocol);

    /**
     * @return struct CalibrationParameters const* The pointer to the C library
     * struct CalibrationParameters store.
     */
    struct Ex10CalibrationParamsV4 const* (*get_params)(void);
};

struct Ex10CalibrationV4 const* get_ex10_cal_v4(void);

#ifdef __cplusplus
}
#endif
