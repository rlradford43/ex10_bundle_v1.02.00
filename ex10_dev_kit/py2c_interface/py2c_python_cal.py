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

import ctypes
from ctypes import *
from enum import IntEnum
import crcmod.predefined
import json
import os
import time
import sys
import struct
import numpy as np
import py2c_interface.py2c_python_auto_enums as ppae
import cal.calib_info as calib_info

# Description of the calibration data
# IPJ_autogen | generate_calibration_v4_shim {

class CalibrationVersionV4(Structure):
    _fields_ = [
        ('cal_file_version', c_uint8),
    ]
class VersionStringsV4(Structure):
    _fields_ = [
        ('power_detect_cal_type', c_uint8),
        ('forward_power_cal_type', c_uint8),
        ('power_detector_temp_comp_type', c_uint8),
        ('forward_power_temp_comp_type', c_uint8),
        ('power_detector_freq_comp_type', c_uint8),
        ('forward_power_freq_comp_type', c_uint8),
    ]
class UserBoardIdV4(Structure):
    _fields_ = [
        ('user_board_id', c_uint16),
    ]
class TxScalarCalV4(Structure):
    _fields_ = [
        ('tx_scalar_cal', c_int16),
    ]
class PerBandRfFilterV4(Structure):
    _fields_ = [
        ('low_freq_limit', c_float),
        ('high_freq_limit', c_float),
    ]
class ValidPdetAdcsV4(Structure):
    _fields_ = [
        ('valid_min_adc', c_uint16),
        ('valid_max_adc', c_uint16),
    ]
class ControlLoopParamsV4(Structure):
    _fields_ = [
        ('loop_gain_divisor', c_uint16),
        ('error_threshold', c_uint8),
        ('max_iterations', c_uint8),
    ]
class PerBandPdetAdcLutV4(Structure):
    _fields_ = [
        ('pdet0_adc_lut', c_uint16 * 31),
        ('pdet1_adc_lut', c_uint16 * 31),
        ('pdet2_adc_lut', c_uint16 * 31),
    ]
class PerBandFwdPowerCoarsePwrCalV4(Structure):
    _fields_ = [
        ('coarse_attn_cal', c_float * 31),
    ]
class PerBandFwdPowerTempSlopeV4(Structure):
    _fields_ = [
        ('fwd_power_temp_slope', c_float),
    ]
class PerBandCalTempV4(Structure):
    _fields_ = [
        ('cal_temp_a_d_c', c_uint16),
    ]
class PerBandLoPdetTempSlopeV4(Structure):
    _fields_ = [
        ('lo_pdet_temp_slope', c_float * 3),
    ]
class PerBandLoPdetFreqLutV4(Structure):
    _fields_ = [
        ('lo_pdet_freq_adc_shifts0', c_int16 * 4),
        ('lo_pdet_freq_adc_shifts1', c_int16 * 4),
        ('lo_pdet_freq_adc_shifts2', c_int16 * 4),
    ]
class PerBandLoPdetFreqsV4(Structure):
    _fields_ = [
        ('lo_pdet_freqs', c_float * 4),
    ]
class PerBandFwdPwrFreqLutV4(Structure):
    _fields_ = [
        ('fwd_pwr_shifts', c_float * 4),
    ]
class DcOffsetCalV4(Structure):
    _fields_ = [
        ('dc_offset', c_int32 * 31),
    ]

class Ex10CalibrationParamsV4(Structure):
    _fields_ = [
        ('calibration_version', CalibrationVersionV4),
        ('version_strings', VersionStringsV4),
        ('user_board_id', UserBoardIdV4),
        ('tx_scalar_cal', TxScalarCalV4),
        ('rf_filter_upper_band', PerBandRfFilterV4),
        ('rf_filter_lower_band', PerBandRfFilterV4),
        ('valid_pdet_adcs', ValidPdetAdcsV4),
        ('control_loop_params', ControlLoopParamsV4),
        ('upper_band_pdet_adc_lut', PerBandPdetAdcLutV4),
        ('upper_band_fwd_power_coarse_pwr_cal', PerBandFwdPowerCoarsePwrCalV4),
        ('upper_band_fwd_power_temp_slope', PerBandFwdPowerTempSlopeV4),
        ('upper_band_cal_temp', PerBandCalTempV4),
        ('upper_band_lo_pdet_temp_slope', PerBandLoPdetTempSlopeV4),
        ('upper_band_lo_pdet_freq_lut', PerBandLoPdetFreqLutV4),
        ('upper_band_lo_pdet_freqs', PerBandLoPdetFreqsV4),
        ('upper_band_fwd_pwr_freq_lut', PerBandFwdPwrFreqLutV4),
        ('lower_band_pdet_adc_lut', PerBandPdetAdcLutV4),
        ('lower_band_fwd_power_coarse_pwr_cal', PerBandFwdPowerCoarsePwrCalV4),
        ('lower_band_fwd_power_temp_slope', PerBandFwdPowerTempSlopeV4),
        ('lower_band_cal_temp', PerBandCalTempV4),
        ('lower_band_lo_pdet_temp_slope', PerBandLoPdetTempSlopeV4),
        ('lower_band_lo_pdet_freq_lut', PerBandLoPdetFreqLutV4),
        ('lower_band_lo_pdet_freqs', PerBandLoPdetFreqsV4),
        ('lower_band_fwd_pwr_freq_lut', PerBandFwdPwrFreqLutV4),
        ('dc_offset_cal', DcOffsetCalV4),
    ]
# IPJ_autogen }

# Description of the calibration data
# IPJ_autogen | generate_calibration_v5_shim {

class CalibrationVersionV5(Structure):
    _fields_ = [
        ('cal_file_version', c_uint8),
    ]
class VersionStringsV5(Structure):
    _fields_ = [
        ('power_detect_cal_type', c_uint8),
        ('forward_power_cal_type', c_uint8),
        ('power_detector_temp_comp_type', c_uint8),
        ('forward_power_temp_comp_type', c_uint8),
        ('power_detector_freq_comp_type', c_uint8),
        ('forward_power_freq_comp_type', c_uint8),
    ]
class UserBoardIdV5(Structure):
    _fields_ = [
        ('user_board_id', c_uint16),
    ]
class TxScalarCalV5(Structure):
    _fields_ = [
        ('tx_scalar_cal', c_int16),
    ]
class PerBandRfFilterV5(Structure):
    _fields_ = [
        ('low_freq_limit', c_float),
        ('high_freq_limit', c_float),
    ]
class ValidPdetAdcsV5(Structure):
    _fields_ = [
        ('valid_min_adc', c_uint16),
        ('valid_max_adc', c_uint16),
    ]
class ControlLoopParamsV5(Structure):
    _fields_ = [
        ('loop_gain_divisor', c_uint16),
        ('error_threshold', c_uint8),
        ('max_iterations', c_uint8),
    ]
class PerBandPdetAdcLutV5(Structure):
    _fields_ = [
        ('pdet0_adc_lut', c_uint16 * 31),
        ('pdet1_adc_lut', c_uint16 * 31),
        ('pdet2_adc_lut', c_uint16 * 31),
    ]
class PerBandFwdPowerCoarsePwrCalV5(Structure):
    _fields_ = [
        ('coarse_attn_cal', c_float * 31),
    ]
class PerBandFwdPowerTempSlopeV5(Structure):
    _fields_ = [
        ('fwd_power_temp_slope', c_float),
    ]
class PerBandCalTempV5(Structure):
    _fields_ = [
        ('cal_temp_a_d_c', c_uint16),
    ]
class PerBandLoPdetTempSlopeV5(Structure):
    _fields_ = [
        ('lo_pdet_temp_slope', c_float * 3),
    ]
class PerBandLoPdetFreqLutV5(Structure):
    _fields_ = [
        ('lo_pdet_freq_adc_shifts0', c_int16 * 4),
        ('lo_pdet_freq_adc_shifts1', c_int16 * 4),
        ('lo_pdet_freq_adc_shifts2', c_int16 * 4),
    ]
class PerBandLoPdetFreqsV5(Structure):
    _fields_ = [
        ('lo_pdet_freqs', c_float * 4),
    ]
class PerBandFwdPwrFreqLutV5(Structure):
    _fields_ = [
        ('fwd_pwr_shifts', c_float * 4),
    ]
class DcOffsetCalV5(Structure):
    _fields_ = [
        ('dc_offset', c_int32 * 31),
    ]
class RssiRfModesV5(Structure):
    _fields_ = [
        ('rf_modes', c_uint16 * 32),
    ]
class RssiRfModeLutV5(Structure):
    _fields_ = [
        ('rf_mode_lut', c_int16 * 32),
    ]
class RssiPga1LutV5(Structure):
    _fields_ = [
        ('pga1_lut', c_int16 * 4),
    ]
class RssiPga2LutV5(Structure):
    _fields_ = [
        ('pga2_lut', c_int16 * 4),
    ]
class RssiPga3LutV5(Structure):
    _fields_ = [
        ('pga3_lut', c_int16 * 4),
    ]
class RssiMixerGainLutV5(Structure):
    _fields_ = [
        ('mixer_gain_lut', c_int16 * 4),
    ]
class RssiRxAttLutV5(Structure):
    _fields_ = [
        ('rx_att_gain_lut', c_int16 * 4),
    ]
class RssiAntennasV5(Structure):
    _fields_ = [
        ('antenna', c_uint8 * 8),
    ]
class RssiAntennaLutV5(Structure):
    _fields_ = [
        ('antenna_lut', c_int16 * 8),
    ]
class PerBandRssiFreqOffsetV5(Structure):
    _fields_ = [
        ('freq_shift', c_int16),
    ]
class RssiRxDefaultPwrV5(Structure):
    _fields_ = [
        ('input_powers', c_int16),
    ]
class RssiRxDefaultLog2V5(Structure):
    _fields_ = [
        ('power_shifts', c_int16),
    ]
class RssiTempSlopeV5(Structure):
    _fields_ = [
        ('rssi_temp_slope', c_float),
    ]
class RssiTempInterceptV5(Structure):
    _fields_ = [
        ('rssi_temp_intercept', c_uint16),
    ]

class Ex10CalibrationParamsV5(Structure):
    _fields_ = [
        ('calibration_version', CalibrationVersionV5),
        ('version_strings', VersionStringsV5),
        ('user_board_id', UserBoardIdV5),
        ('tx_scalar_cal', TxScalarCalV5),
        ('rf_filter_upper_band', PerBandRfFilterV5),
        ('rf_filter_lower_band', PerBandRfFilterV5),
        ('valid_pdet_adcs', ValidPdetAdcsV5),
        ('control_loop_params', ControlLoopParamsV5),
        ('upper_band_pdet_adc_lut', PerBandPdetAdcLutV5),
        ('upper_band_fwd_power_coarse_pwr_cal', PerBandFwdPowerCoarsePwrCalV5),
        ('upper_band_fwd_power_temp_slope', PerBandFwdPowerTempSlopeV5),
        ('upper_band_cal_temp', PerBandCalTempV5),
        ('upper_band_lo_pdet_temp_slope', PerBandLoPdetTempSlopeV5),
        ('upper_band_lo_pdet_freq_lut', PerBandLoPdetFreqLutV5),
        ('upper_band_lo_pdet_freqs', PerBandLoPdetFreqsV5),
        ('upper_band_fwd_pwr_freq_lut', PerBandFwdPwrFreqLutV5),
        ('lower_band_pdet_adc_lut', PerBandPdetAdcLutV5),
        ('lower_band_fwd_power_coarse_pwr_cal', PerBandFwdPowerCoarsePwrCalV5),
        ('lower_band_fwd_power_temp_slope', PerBandFwdPowerTempSlopeV5),
        ('lower_band_cal_temp', PerBandCalTempV5),
        ('lower_band_lo_pdet_temp_slope', PerBandLoPdetTempSlopeV5),
        ('lower_band_lo_pdet_freq_lut', PerBandLoPdetFreqLutV5),
        ('lower_band_lo_pdet_freqs', PerBandLoPdetFreqsV5),
        ('lower_band_fwd_pwr_freq_lut', PerBandFwdPwrFreqLutV5),
        ('dc_offset_cal', DcOffsetCalV5),
        ('rssi_rf_modes', RssiRfModesV5),
        ('rssi_rf_mode_lut', RssiRfModeLutV5),
        ('rssi_pga1_lut', RssiPga1LutV5),
        ('rssi_pga2_lut', RssiPga2LutV5),
        ('rssi_pga3_lut', RssiPga3LutV5),
        ('rssi_mixer_gain_lut', RssiMixerGainLutV5),
        ('rssi_rx_att_lut', RssiRxAttLutV5),
        ('rssi_antennas', RssiAntennasV5),
        ('rssi_antenna_lut', RssiAntennaLutV5),
        ('upper_band_rssi_freq_offset', PerBandRssiFreqOffsetV5),
        ('lower_band_rssi_freq_offset', PerBandRssiFreqOffsetV5),
        ('rssi_rx_default_pwr', RssiRxDefaultPwrV5),
        ('rssi_rx_default_log2', RssiRxDefaultLog2V5),
        ('rssi_temp_slope', RssiTempSlopeV5),
        ('rssi_temp_intercept', RssiTempInterceptV5),
    ]
# IPJ_autogen }


class CalibrationInfoPageHelpers(object):
    """
    This is a class of helpers to work with the calibration info in an Ex10 device.
    This can be used to analyze and create calibration.
    """
    DB_PER_RSSI_LOG2 = 0.047

    # The calibration procedure configuration is set with the definitions below
    CAL_CFG = {'PM_SLEEP_TIME': 0.15,
               'PD_SLEEP_TIME': 0.02,
               'ANTENNA': 1,
               'RF_MODE': 5,
               'TX_SCALAR': 1152,  # 5 dB backoff from FS
               'COARSE_ATTS': list(range(30, -1, -1)),
               'DFLT_COARSE_ATT': 8,
               'DFLT_UPPER_FREQ_MHZ': 920.25,
               'DFLT_UPPER_FREQ_REGION': 'FCC',
               'UPPER_FREQS_MHZ': [902.75, 910, 920.25, 927.25],
               'COARSE_ATTS_FREQ': [25, 17, 8],
               'DFLT_LOWER_FREQ_MHZ': 866,
               'DFLT_LOWER_FREQ_REGION': 'ETSI_LOWER',
               'LOWER_FREQS_MHZ': [865, 866, 867, 867.9],
               'CAL_VERSION': 5,
               'SOAK_TIME': 3,
               'SOAK_COARSE_ATT': 6,
               'PDET_PWR_PER_TEMP_ADC': [0.02, 0.013, 0.012],
               'DB_PER_ADC_FWD_PWR': -0.039,
               'FWD_PWR_LIMIT_DBM': 32,
               'NUM_PDET_BLOCKS': 3,
               'PDET_ADC_MIN_MAX': [150, 950],
               'ADC_ERROR_THRESHOLD': 5,
               'LOOP_GAIN_DIVISOR': 500,
               'MAX_ITERATIONS': 10,
               }

    # The DC offset characterization algorithm uses the following definitions
    DC_CFG = {'PWR_TOL': 1,  # Power ripple tolerance in ADC codes
              'MAX_ITERS': 10,  # Max num DC offset algorithm loops
              'INIT_TX_SCALAR': 2047,  # Initial power scalar
              'MAX_FWD_PWR': 550,  # Max power in ADC codes to avoid compression
              'PWR_BACKOFF_SCALAR': 0.8,  # Fwd pwr backoff for Tx scalar
              }

    def __init__(self, py2c_wrapper, py2c_expert):
        """
        @param py2c_wrapper: The py2c wrapper object.
        @param py2c_expert: The expert class which prevides helpers
                            on top of the normal stack.
        """
        self._py2c = py2c_wrapper
        self._ex10_protocol = py2c_wrapper.get_ex10_protocol()
        self._py2c.get_ex10_cal_v5().init(self._ex10_protocol.ex10_protocol)
        self._cal_obj = self._py2c.get_ex10_calibration(self._ex10_protocol.ex10_protocol).contents
        self._py2c_expert = py2c_expert
        self.should_print = True
        # initialize with an empty set of params
        self.info = self.initialize_cal_info()

    def reset_layers(self):
        self._ex10_protocol = self._py2c.get_ex10_protocol()
        self._cal_obj = self._py2c.get_ex10_calibration(self._ex10_protocol.ex10_protocol).contents
 
    def initialize_cal_data(self):
        """
        Init data structure as (nested dict)
        """
        c_len = len(self.CAL_CFG['COARSE_ATTS'])
        freq_len = len(self.CAL_CFG['UPPER_FREQS_MHZ'])
        n_pdets = 4
        cal_data = {
            'COARSE': {
                'FWD_PWR': [0xFF] * c_len,
                'LO_PDET': [[0xFFF for j in range(n_pdets)] for i in range(c_len)],
                'TEMP': [0] * c_len,
            },
            'FREQ': {
                'FWD_PWR': [[] for j in range(freq_len)],
                'LO_PDET': [[] for j in range(freq_len)],
                'TEMP': [[] for j in range(freq_len)],
            },
            'DC_OFS': [0] * c_len,
            'TEMP': 0,
        }
        return cal_data

    def initialize_cal_info(self):
        """
        Instantiates an accessor object to read in and parse calibration
        configurations from YAML. This storage object lives in the info object and is
        used to write to and read from the device.
        :return: calibration accessor object
        """
        info = calib_info.CalibrationInfoPageAccessor()
        info.read_in_yaml(self.CAL_CFG['CAL_VERSION'])
        return info

    def to_json(self, filename):
        """ Save the cal params to a json file """
        return self.info.to_json(filename)

    def from_json(self, filename):
        """ Read in the cal params from a json file """
        return self.info.from_json(filename)

    def get_parameter(self, match_field):
        """ Get a value from the local cal params"""
        return self.info.get_parameter(match_field)

    def set_parameter(self, match_field, value):
        """ Set a value to the local cal params """
        return self.info.set_parameter(match_field, value)

    def dump_params(self):
        """
        This function iterates over all calibration params read from the device
        and prints them out.
        """
        return self.info.dump_params()

    def erase_calibration_info_page(self):
        """
        This writes empty data to the calibration info page, which erases it
        and leaves it empty
        """
        self.write_calibration_info_page(host_data=bytearray())


    def write_calibration_info_page(self, host_data=None):
        """ This writes to local cal params to the Ex10 info page """
        self.reset_layers()
        assert(self._ex10_protocol.get_running_location() == ppae.Status.Bootloader)
        # Create data for cal page
        if host_data:
            bytestream = host_data
        else:
            bytestream = self.info.to_info_page_string()
        # Write the cal page
        byte_array = ctypes.c_uint8 * len(bytestream)
        write_array = byte_array.from_buffer(bytearray(bytestream))
        self._ex10_protocol.write_calibration_info_page(write_array, len(bytestream))


    def read_calibration_info_page(self):
        """
        This method forces a re-read of the calibration data from
        the Ex10 info page into the local params.
        """
        self.reset_layers()
        # Reinitialize to re-read from the info page 
        cal_obj = self._py2c.get_ex10_cal_v5()
        cal_obj.init(self._ex10_protocol.ex10_protocol)
        # Get the params
        read_params = self._py2c.get_ex10_cal_v5().get_params()
        #self.local_cal_params = read_params.contents
        print(bytearray(read_params.contents))
        self.info.from_info_page_string(bytearray(read_params.contents))

    def get_parameter_from_c_struct(self, match_field):
        """
        Get a value from the device calibration params by
        analyzing the c structs.
        """
        cal_params = self._py2c.get_ex10_cal_v5().get_params().contents
        try:
            return getattr(cal_params, match_field)
        except:
            raise Exception("Passed parameter not found")

    def dump_params_from_c_struct(self):
        """
        This function iterates over all params in the cal structure
        and prints them out. This utilizes the direct read from the
        device instead of the json dump.
        """
        cal_params = self._py2c.get_ex10_cal_v5().get_params()
        # Attempt to access the params
        try:
            cal_params = cal_params.contents
        except:
            # We returned NULL from c due to a bad cal
            print("Cal on board is unsupported")
        # Parse and print the params
        for field_name, field_type in cal_params._fields_:
            sub_struct = getattr(cal_params, field_name)
            for sub_name, sub_type in sub_struct._fields_:
                is_array = isinstance(getattr(sub_struct, sub_name), ctypes.Array)
                if is_array:
                    print(sub_name, list(getattr(sub_struct, sub_name)))
                else:
                    print(sub_name, getattr(sub_struct, sub_name))
