#############################################################################
#                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      #
#                                                                           #
# This source code is the property of Impinj, Inc. Your use of this source  #
# code in whole or in part is subject to your applicable license terms      #
# from Impinj.                                                              #
# Contact support@impinj.com for a copy of the applicable Impinj license    #
# terms.                                                                    #
#                                                                           #
# (c) Copyright 2020 - 2021 Impinj, Inc. All rights reserved.               #
#                                                                           #
#############################################################################

from ctypes import *
from enum import IntEnum
import py2c_interface.py2c_python_auto_regs as ppar


# IPJ_autogen | generate_c2python_reg_instances {
class RegInstances:
    command_result_reg = ppar.RegisterInfo('CommandResult', 0x0000, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    reset_cause_reg = ppar.RegisterInfo('ResetCause', 0x0004, 0x0002, 1, ppar.RegisterAccessType.ReadOnly)
    status_reg = ppar.RegisterInfo('Status', 0x0006, 0x0002, 1, ppar.RegisterAccessType.ReadOnly)
    version_string_reg = ppar.RegisterInfo('VersionString', 0x0008, 0x0020, 1, ppar.RegisterAccessType.ReadOnly)
    build_number_reg = ppar.RegisterInfo('BuildNumber', 0x0028, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    git_hash_reg = ppar.RegisterInfo('GitHash', 0x002C, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    timestamp_reg = ppar.RegisterInfo('Timestamp', 0x0030, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    fref_freq_reg = ppar.RegisterInfo('FrefFreq', 0x0034, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    product_sku_reg = ppar.RegisterInfo('ProductSku', 0x0068, 0x0008, 1, ppar.RegisterAccessType.ReadOnly)
    serial_number_reg = ppar.RegisterInfo('SerialNumber', 0x0070, 0x0020, 1, ppar.RegisterAccessType.ReadOnly)
    device_info_reg = ppar.RegisterInfo('DeviceInfo', 0x0090, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    device_build_reg = ppar.RegisterInfo('DeviceBuild', 0x0094, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    rtl_revision_reg = ppar.RegisterInfo('RtlRevision', 0x0098, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    interrupt_mask_reg = ppar.RegisterInfo('InterruptMask', 0x00A0, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    interrupt_mask_set_reg = ppar.RegisterInfo('InterruptMaskSet', 0x00A4, 0x0004, 1, ppar.RegisterAccessType.WriteOnly)
    interrupt_mask_clear_reg = ppar.RegisterInfo('InterruptMaskClear', 0x00A8, 0x0004, 1, ppar.RegisterAccessType.WriteOnly)
    interrupt_status_reg = ppar.RegisterInfo('InterruptStatus', 0x00AC, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    event_fifo_num_bytes_reg = ppar.RegisterInfo('EventFifoNumBytes', 0x00B0, 0x0002, 1, ppar.RegisterAccessType.ReadOnly)
    event_fifo_int_level_reg = ppar.RegisterInfo('EventFifoIntLevel', 0x00B2, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    gpio_output_enable_reg = ppar.RegisterInfo('GpioOutputEnable', 0x00B4, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    gpio_output_level_reg = ppar.RegisterInfo('GpioOutputLevel', 0x00B8, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_aux_adc_control_reg = ppar.RegisterInfo('PowerControlLoopAuxAdcControl', 0x00BC, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_gain_divisor_reg = ppar.RegisterInfo('PowerControlLoopGainDivisor', 0x00C0, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_max_iterations_reg = ppar.RegisterInfo('PowerControlLoopMaxIterations', 0x00C4, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_initial_tx_scalar_reg = ppar.RegisterInfo('PowerControlLoopInitialTxScalar', 0x00C8, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_adc_target_reg = ppar.RegisterInfo('PowerControlLoopAdcTarget', 0x00CC, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    power_control_loop_adc_thresholds_reg = ppar.RegisterInfo('PowerControlLoopAdcThresholds', 0x00D0, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    delay_us_reg = ppar.RegisterInfo('DelayUs', 0x00D4, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    ops_control_reg = ppar.RegisterInfo('OpsControl', 0x0300, 0x0001, 1, ppar.RegisterAccessType.ReadWrite)
    ops_status_reg = ppar.RegisterInfo('OpsStatus', 0x0304, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    halted_control_reg = ppar.RegisterInfo('HaltedControl', 0x0308, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    halted_status_reg = ppar.RegisterInfo('HaltedStatus', 0x030C, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    log_test_period_reg = ppar.RegisterInfo('LogTestPeriod', 0x0320, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    log_test_word_repeat_reg = ppar.RegisterInfo('LogTestWordRepeat', 0x0324, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    event_fifo_test_period_reg = ppar.RegisterInfo('EventFifoTestPeriod', 0x0328, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    event_fifo_test_payload_num_words_reg = ppar.RegisterInfo('EventFifoTestPayloadNumWords', 0x032C, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    log_speed_reg = ppar.RegisterInfo('LogSpeed', 0x0330, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    log_enables_reg = ppar.RegisterInfo('LogEnables', 0x0334, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    ber_control_reg = ppar.RegisterInfo('BerControl', 0x0338, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    ber_mode_reg = ppar.RegisterInfo('BerMode', 0x033C, 0x0001, 1, ppar.RegisterAccessType.ReadWrite)
    modem_data_control_reg = ppar.RegisterInfo('ModemDataControl', 0x0340, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    aux_adc_control_reg = ppar.RegisterInfo('AuxAdcControl', 0x0400, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    aux_adc_results_reg = ppar.RegisterInfo('AuxAdcResults', 0x0404, 0x0002, 15, ppar.RegisterAccessType.ReadOnly)
    aux_dac_control_reg = ppar.RegisterInfo('AuxDacControl', 0x0430, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    aux_dac_settings_reg = ppar.RegisterInfo('AuxDacSettings', 0x0432, 0x0002, 2, ppar.RegisterAccessType.ReadWrite)
    a_test_mux_reg = ppar.RegisterInfo('ATestMux', 0x0440, 0x0004, 4, ppar.RegisterAccessType.ReadWrite)
    rf_synthesizer_control_reg = ppar.RegisterInfo('RfSynthesizerControl', 0x0500, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    tx_fine_gain_reg = ppar.RegisterInfo('TxFineGain', 0x0504, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    rx_gain_control_reg = ppar.RegisterInfo('RxGainControl', 0x0508, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    tx_coarse_gain_reg = ppar.RegisterInfo('TxCoarseGain', 0x050C, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    rf_mode_reg = ppar.RegisterInfo('RfMode', 0x0514, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    dc_offset_reg = ppar.RegisterInfo('DcOffset', 0x0518, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    etsi_burst_off_time_reg = ppar.RegisterInfo('EtsiBurstOffTime', 0x051C, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    cw_is_on_reg = ppar.RegisterInfo('CwIsOn', 0x0520, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    sjc_control_reg = ppar.RegisterInfo('SjcControl', 0x0600, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_gain_control_reg = ppar.RegisterInfo('SjcGainControl', 0x0604, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_initial_settling_time_reg = ppar.RegisterInfo('SjcInitialSettlingTime', 0x0608, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_residue_settling_time_reg = ppar.RegisterInfo('SjcResidueSettlingTime', 0x060C, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_cdac_i_reg = ppar.RegisterInfo('SjcCdacI', 0x0610, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_cdac_q_reg = ppar.RegisterInfo('SjcCdacQ', 0x0614, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    sjc_result_i_reg = ppar.RegisterInfo('SjcResultI', 0x0618, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    sjc_result_q_reg = ppar.RegisterInfo('SjcResultQ', 0x061C, 0x0004, 1, ppar.RegisterAccessType.ReadOnly)
    sjc_residue_threshold_reg = ppar.RegisterInfo('SjcResidueThreshold', 0x0620, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    analog_enable_reg = ppar.RegisterInfo('AnalogEnable', 0x0700, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    aggregate_op_buffer_reg = ppar.RegisterInfo('AggregateOpBuffer', 0x0704, 0x0100, 1, ppar.RegisterAccessType.ReadWrite)
    rssi_threshold_rn16_reg = ppar.RegisterInfo('RssiThresholdRn16', 0x0FFC, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    rssi_threshold_epc_reg = ppar.RegisterInfo('RssiThresholdEpc', 0x0FFE, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    inventory_round_control_reg = ppar.RegisterInfo('InventoryRoundControl', 0x1000, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    inventory_round_control_2_reg = ppar.RegisterInfo('InventoryRoundControl_2', 0x1004, 0x0004, 1, ppar.RegisterAccessType.ReadWrite)
    nominal_stop_time_reg = ppar.RegisterInfo('NominalStopTime', 0x1008, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    extended_stop_time_reg = ppar.RegisterInfo('ExtendedStopTime', 0x100C, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    regulatory_stop_time_reg = ppar.RegisterInfo('RegulatoryStopTime', 0x1010, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    gen2_select_enable_reg = ppar.RegisterInfo('Gen2SelectEnable', 0x1014, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    gen2_access_enable_reg = ppar.RegisterInfo('Gen2AccessEnable', 0x1018, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    gen2_auto_access_enable_reg = ppar.RegisterInfo('Gen2AutoAccessEnable', 0x101C, 0x0002, 1, ppar.RegisterAccessType.ReadWrite)
    gen2_offsets_reg = ppar.RegisterInfo('Gen2Offsets', 0x1020, 0x0001, 10, ppar.RegisterAccessType.ReadWrite)
    gen2_lengths_reg = ppar.RegisterInfo('Gen2Lengths', 0x1030, 0x0002, 10, ppar.RegisterAccessType.ReadWrite)
    gen2_transaction_ids_reg = ppar.RegisterInfo('Gen2TransactionIds', 0x1050, 0x0001, 10, ppar.RegisterAccessType.ReadWrite)
    gen2_txn_controls_reg = ppar.RegisterInfo('Gen2TxnControls', 0x1060, 0x0004, 10, ppar.RegisterAccessType.ReadWrite)
    gen2_tx_buffer_reg = ppar.RegisterInfo('Gen2TxBuffer', 0x1100, 0x0080, 1, ppar.RegisterAccessType.ReadWrite)
    calibration_info_reg = ppar.RegisterInfo('CalibrationInfo', 0xE800, 0x0800, 1, ppar.RegisterAccessType.ReadOnly)
# IPJ_autogen }
