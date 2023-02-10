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

#include <stddef.h>

#include "board/e710_ref_design/calibration_v5.h"
#include "calibration.h"
#include "ex10_api/board_init.h"


#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void print_calibration(struct Ex10CalibrationParamsV5 const* calibration)
{
    printf("Calibration:\n");
    printf(
        "    Version: file: %u, pwr_det: %u, fwd_pwr: %u, pwr_det_temp: %u\n"
        "             fwd_pwr_temp: %u, pwr_det_freq: %u, fwd_pwr_freq: %u\n",
        calibration->calibration_version.cal_file_version,
        calibration->version_strings.power_detect_cal_type,
        calibration->version_strings.forward_power_cal_type,
        calibration->version_strings.power_detector_temp_comp_type,
        calibration->version_strings.forward_power_temp_comp_type,
        calibration->version_strings.power_detector_freq_comp_type,
        calibration->version_strings.forward_power_freq_comp_type);

    printf("    user_board_id: %u\n", calibration->user_board_id.user_board_id);
    printf("    tx_scalar_cal: %hi\n",
           calibration->tx_scalar_cal.tx_scalar_cal);

    printf("    RFFilter:\n");
    printf("            Lower Band: [%7.3f, %7.3f]\n",
           calibration->rf_filter_lower_band.low_freq_limit,
           calibration->rf_filter_lower_band.high_freq_limit);
    printf("            Upper Band: [%7.3f, %7.3f]\n",
           calibration->rf_filter_upper_band.low_freq_limit,
           calibration->rf_filter_upper_band.high_freq_limit);

    printf("    ValidPdetAdcs:\n");
    printf("            Valid min ADC: %u\n",
           calibration->valid_pdet_adcs.valid_min_adc);
    printf("            Valid max ADC: %u\n",
           calibration->valid_pdet_adcs.valid_max_adc);

    printf("    ControlLoopParams:\n");
    printf("            Loop Gain Divisor: %u\n",
           calibration->control_loop_params.loop_gain_divisor);
    printf("            Error Threshold: %u\n",
           calibration->control_loop_params.error_threshold);
    printf("            Max Iterations: %u\n",
           calibration->control_loop_params.max_iterations);

    printf("    PdetAdcLut:\n");
    printf("           Lower Band                       Upper Band\n");
    printf(
        "          pdet0     pdet1     pdet2        pdet0     pdet1     "
        "pdet2\n");
    size_t const pdet_adc_lut_row_count =
        ARRAY_SIZE(calibration->lower_band_pdet_adc_lut.pdet0_adc_lut);
    for (size_t idx = 0u; idx < pdet_adc_lut_row_count; ++idx)
    {
        printf("    [%2zu]: %5u     %5u     %5u        %5u     %5u     %5hi\n",
               idx,
               calibration->lower_band_pdet_adc_lut.pdet0_adc_lut[idx],
               calibration->lower_band_pdet_adc_lut.pdet1_adc_lut[idx],
               calibration->lower_band_pdet_adc_lut.pdet2_adc_lut[idx],
               calibration->upper_band_pdet_adc_lut.pdet0_adc_lut[idx],
               calibration->upper_band_pdet_adc_lut.pdet1_adc_lut[idx],
               calibration->upper_band_pdet_adc_lut.pdet2_adc_lut[idx]);
    }

    printf("    FwdPowerCoarsePwrCal:\n");
    printf("            Lower Band    Upper Band\n");
    size_t const coarse_attn_cal_count = ARRAY_SIZE(
        calibration->lower_band_fwd_power_coarse_pwr_cal.coarse_attn_cal);
    for (size_t idx = 0u; idx < coarse_attn_cal_count; ++idx)
    {
        printf("    [%2zu]:   %8.3f      %8.3f\n",
               idx,
               calibration->lower_band_fwd_power_coarse_pwr_cal
                   .coarse_attn_cal[idx],
               calibration->upper_band_fwd_power_coarse_pwr_cal
                   .coarse_attn_cal[idx]);
    }

    printf("    FwdPowerTempSlope:\n");
    printf("            Lower Band    Upper Band\n");
    printf("     slope: %8.3f      %8.3f\n",
           calibration->lower_band_fwd_power_temp_slope.fwd_power_temp_slope,
           calibration->upper_band_fwd_power_temp_slope.fwd_power_temp_slope);

    printf("    CalTemp ADC:\n");
    printf("            Lower Band    Upper Band\n");
    printf("              %5u          %5u\n",
           calibration->lower_band_cal_temp.cal_temp_a_d_c,
           calibration->upper_band_cal_temp.cal_temp_a_d_c);

    printf("    LoPdetTempSlope:\n");
    printf("            Lower Band\n");
    printf("     slope: %8.3f, %8.3f, %8.3f\n",
           calibration->lower_band_lo_pdet_temp_slope.lo_pdet_temp_slope[0],
           calibration->lower_band_lo_pdet_temp_slope.lo_pdet_temp_slope[1],
           calibration->lower_band_lo_pdet_temp_slope.lo_pdet_temp_slope[2]);

    printf("            Upper Band\n");
    printf("     slope: %8.3f, %8.3f, %8.3f\n",
           calibration->upper_band_lo_pdet_temp_slope.lo_pdet_temp_slope[0],
           calibration->upper_band_lo_pdet_temp_slope.lo_pdet_temp_slope[1],
           calibration->upper_band_lo_pdet_temp_slope.lo_pdet_temp_slope[2]);

    printf("    LoPdetFreqLut:\n");
    printf("        Lower Band                       Upper Band\n");
    printf(
        "         shifts0   shifts1   shifts2      shifts0   shifts1   "
        "shifts2\n");
    size_t const pdet_freq_lut_row_count = ARRAY_SIZE(
        calibration->upper_band_lo_pdet_freq_lut.lo_pdet_freq_adc_shifts0);
    for (size_t idx = 0u; idx < pdet_freq_lut_row_count; ++idx)
    {
        printf(
            "    [%2zu]: %5hi     %5hi     %5hi        %5hi     %5hi     "
            "%5hi\n",
            idx,
            calibration->lower_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts0[idx],
            calibration->lower_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts1[idx],
            calibration->lower_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts2[idx],
            calibration->upper_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts0[idx],
            calibration->upper_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts1[idx],
            calibration->upper_band_lo_pdet_freq_lut
                .lo_pdet_freq_adc_shifts2[idx]);
    }

    printf("    LoPdetFreqs:\n");
    printf("            Lower Band    Upper Band\n");
    size_t const pdet_freq_count =
        ARRAY_SIZE(calibration->lower_band_lo_pdet_freqs.lo_pdet_freqs);
    for (size_t idx = 0u; idx < pdet_freq_count; ++idx)
    {
        printf("            %8.3f      %8.3f\n",
               calibration->lower_band_lo_pdet_freqs.lo_pdet_freqs[idx],
               calibration->upper_band_lo_pdet_freqs.lo_pdet_freqs[idx]);
    }

    printf("    FwdPwrFreqLut:\n");
    printf("            Lower Band    Upper Band\n");
    size_t const fwd_pwr_freq_lut_count =
        ARRAY_SIZE(calibration->lower_band_fwd_pwr_freq_lut.fwd_pwr_shifts);
    for (size_t idx = 0u; idx < fwd_pwr_freq_lut_count; ++idx)
    {
        printf("            %8.3f      %8.3f\n",
               calibration->lower_band_fwd_pwr_freq_lut.fwd_pwr_shifts[idx],
               calibration->upper_band_fwd_pwr_freq_lut.fwd_pwr_shifts[idx]);
    }

    printf("    DcOffsetCal:\n");
    size_t const dc_offset_count =
        ARRAY_SIZE(calibration->dc_offset_cal.dc_offset);
    for (size_t idx = 0u; idx < dc_offset_count; ++idx)
    {
        printf("    [%2zu]: %7d\n",
               idx,
               calibration->dc_offset_cal.dc_offset[idx]);
    }

    printf("    RssiRfMode:          RssiRfModeLut:\n");
    size_t const rssi_rf_modes_count =
        ARRAY_SIZE(calibration->rssi_rf_modes.rf_modes);
    for (size_t idx = 0; idx < rssi_rf_modes_count; ++idx)
    {
        printf("    [%2zu]: %3u               %5hi\n",
               idx,
               calibration->rssi_rf_modes.rf_modes[idx],
               calibration->rssi_rf_mode_lut.rf_mode_lut[idx]);
    }

    printf("    RssiPgaLuts:\n");
    size_t const pga_lut_row_count =
        ARRAY_SIZE(calibration->rssi_pga1_lut.pga1_lut);
    for (size_t idx = 0u; idx < pga_lut_row_count; ++idx)
    {
        printf("    [%2zu]: %5hi    %5hi    %5hi\n",
               idx,
               calibration->rssi_pga1_lut.pga1_lut[idx],
               calibration->rssi_pga2_lut.pga2_lut[idx],
               calibration->rssi_pga3_lut.pga3_lut[idx]);
    }

    printf("    RssiMixerGainLut:\n");
    size_t const mixer_gain_lut_count =
        ARRAY_SIZE(calibration->rssi_mixer_gain_lut.mixer_gain_lut);
    for (size_t idx = 0; idx < mixer_gain_lut_count; ++idx)
    {
        printf("    [%2zu]: %5hi\n",
               idx,
               calibration->rssi_mixer_gain_lut.mixer_gain_lut[idx]);
    }

    printf("    RssiRxAttLut:\n");
    size_t const rx_atten_lut_count =
        ARRAY_SIZE(calibration->rssi_rx_att_lut.rx_att_gain_lut);
    for (size_t idx = 0; idx < rx_atten_lut_count; ++idx)
    {
        printf("    [%2zu]: %5hi\n",
               idx,
               calibration->rssi_rx_att_lut.rx_att_gain_lut[idx]);
    }

    printf("    RssiAntenna:       RssiAntennaLut:\n");
    size_t const antenna_count = ARRAY_SIZE(calibration->rssi_antennas.antenna);
    for (size_t idx = 0; idx < antenna_count; ++idx)
    {
        printf("    [%2zu]: %3u             %5hi\n",
               idx,
               calibration->rssi_antennas.antenna[idx],
               calibration->rssi_antenna_lut.antenna_lut[idx]);
    }

    printf("    RssiFreqOffset:\n");
    printf("            Lower Band    Upper Band\n");
    printf("               %5hi       %5hi\n",
           calibration->lower_band_rssi_freq_offset.freq_shift,
           calibration->upper_band_rssi_freq_offset.freq_shift);

    printf("    RssiRxDefaultPower:       RssiRxDefaultLog2:\n");
    printf("           %5hi                     %5hi\n",
           calibration->rssi_rx_default_pwr.input_powers,
           calibration->rssi_rx_default_log2.power_shifts);

    printf("    RssiTempSlope:       RssiTempIntercept:\n");
    printf("          %8.3f                  %5u\n",
           calibration->rssi_temp_slope.rssi_temp_slope,
           calibration->rssi_temp_intercept.rssi_temp_intercept);
}

int main(void)
{
    struct Ex10Interfaces ex10_iface =
        ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, "FCC");

    int result = ex10_iface.helpers->check_board_init_status(Application);
    if (result == 0)
    {
        uint8_t cal_version =
            get_ex10_calibration(ex10_iface.protocol)->get_cal_version();

        if (cal_version == 5)
        {
            get_ex10_cal_v5()->init(ex10_iface.protocol);

            struct Ex10CalibrationParamsV5 const* calibration =
                get_ex10_cal_v5()->get_params();
            print_calibration(calibration);
        }
        else
        {
            fprintf(stderr,
                    "The current calibration version of %d does not match the "
                    "example\n",
                    cal_version);
        }
    }

    ex10_typical_board_teardown();
    return result;
}
