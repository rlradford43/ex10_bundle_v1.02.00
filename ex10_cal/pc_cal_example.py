#############################################################################
#                  IMPINJ CONFIDENTIAL AND PROPRIETARY                      #
#                                                                           #
# This source code is the property of Impinj, Inc. Your use of this source  #
# code in whole or in part is subject to your applicable license terms      #
# from Impinj.                                                              #
# Contact support@impinj.com for a copy of the applicable Impinj license    #
# terms.                                                                    #
#                                                                           #
# (c) Copyright 2021 Impinj, Inc. All rights reserved.                      #
#                                                                           #
#############################################################################
"""
This example script runs the calibration procedure for the Ex10 dev kit from
a PC, communicating via a UART interface.
"""

from __future__ import (division, absolute_import, print_function,
                        unicode_literals)

import time
import os
import argparse
import cal.calib_info as calib_info
from cal.power_meter import PowerMeter
from cal.sig_gen import KeysightSigGen
from cal.uart_helper import UartBaud, UartHelper
from cal.uart_reader import UartReader
import ex10_api.application_address_range as aar
import ex10_api.mnemonics as mne


class Ex10CalibrationExample(object):
    """
    Calibration procedure example class for Ex10 reader.
    """

    # The calibration procedure configuration is set with the definitions below
    CAL_CFG = {'CAL_VERSION': 5,
               'PM_SLEEP_TIME': 0.15,
               'PD_SLEEP_TIME': 0.02,
               'ANTENNA': 1,
               'RF_MODE': 5,
               'TX_SCALAR': 1152,  # 5 dB backoff from FS
               'COARSE_ATTS': list(range(30, -1, -1)),
               'DFLT_COARSE_ATT': 8,
               'DFLT_UPPER_FREQ_MHZ': 920.25,
               'DFLT_UPPER_FREQ_REGION': 'FCC',
               'UPPER_FREQS_MHZ': [902.75, 910.25, 920.25, 927.25],
               'COARSE_ATTS_FREQ': [25, 17, 8],
               'DFLT_LOWER_FREQ_MHZ': 866.3,
               'DFLT_LOWER_FREQ_REGION': 'ETSI LOWER',
               'LOWER_FREQS_MHZ': [865.7, 866.3, 866.9, 867.5],
               'SOAK_TIME': 3,
               'SOAK_COARSE_ATT': 6,
               'PDET_PWR_PER_TEMP_ADC': [0.02, 0.013, 0.012],
               'DB_PER_ADC_FWD_PWR': -0.039,
               'FWD_PWR_LIMIT_DBM': 32,
               'NUM_PDET_BLOCKS': 3,
               'PDET_ADC_MIN_MAX': [150, 915],
               'ADC_ERROR_THRESHOLD': 10,
               'LOOP_GAIN_DIVISOR': 800,
               'MAX_ITERATIONS': 10,
               'NUM_PDETS': 4,
               }

    # The DC offset characterization algorithm uses the following definitions
    DC_CFG = {'PWR_TOL': 1,  # Power ripple tolerance in ADC codes
              'MAX_ITERS': 10,  # Max num DC offset algorithm loops
              'INIT_TX_SCALAR': 2047,  # Initial power scalar
              'MAX_FWD_PWR': 550,  # Max power in ADC codes to avoid compression
              'PWR_BACKOFF_SCALAR': 0.8,  # Fwd pwr backoff for Tx scalar
              }

    BLF_KHZ = {  # Used for RSSI calibration
        1: 640,
        3: 320,
        5: 320,
        7: 250,
        11: 640,
        12: 320,
        13: 160,
        15: 640
    }

    def __init__(self, power_meter, sig_gen, ex10_reader, verbose=True):
        self.power_meter = power_meter
        self.sig_gen = sig_gen
        self.ex10_reader = ex10_reader
        self.verbose = verbose
        self.info = self.initialize_cal_info()

    def _print(self, *args):
        if self.verbose:
            print(*args)

    @staticmethod
    def get_board_id(board_id=None, directory='c:/users/lab_user/ex10_board_cal'):
        # pylint: disable=locally-disabled, missing-docstring
        if board_id is None:
            max_id_len = 16
            board_id = int(input('Enter calibrated board ID number (max '
                                 + str(2 ** max_id_len - 1) + '): '))
            if board_id >= 2 ** max_id_len:
                raise ValueError('Board ID number larger than max')
        json_file_to_write = directory + os.sep + str(board_id) + '.json'
        return board_id, json_file_to_write

    def initialize_cal_info(self):
        """
        Instantiates an accessor object to read in and parse calibration
        configurations from YAML.
        :return: calibration accessor object
        """
        info = calib_info.CalibrationInfoPageAccessor()
        info.read_in_yaml(self.CAL_CFG['CAL_VERSION'])
        return info

    def init_pwr_cal_data(self):
        """
        Init power control data structure as nested dict
        """
        c_len = len(self.CAL_CFG['COARSE_ATTS'])
        freq_len = len(self.CAL_CFG['UPPER_FREQS_MHZ'])
        cal_data = {
            'COARSE': {
                'FWD_PWR': [0xFF] * c_len,
                'LO_PDET': [[0xFFF for j in range(self.CAL_CFG['NUM_PDETS'])]
                            for i in range(c_len)],
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

    def init_rssi_configs(self):
        """
        Chooses RSSI calibration settings based on product SKU
        """
        _, sku_str = self.ex10_reader.get_sku()
        sku = int(sku_str, 16)
        sku_rssi_cfg = {
            mne.ProductSku.E310.value: {
                'RF_MODES': [3, 5, 7, 12, 13],
            },
            mne.ProductSku.E510.value: {
                'RF_MODES': [1, 3, 5, 7, 12, 13, 15],
            },
            mne.ProductSku.E710.value: {
                'RF_MODES': [1, 3, 5, 7, 11, 12, 13, 15],
            }
        }
        self.RSSI_CFG = {
            'N_RF_MODES': 32,  # Number of max allowed RF modes in cal table
            'N_ANTENNA_PORTS': 8,  # Number of max allowed antennas in cal table
            'ANTENNA_PORTS': [2, 1],  # Antenna ports
            'RF_MODES': sku_rssi_cfg[sku]['RF_MODES'],
            'TEMP_COEFS': [0, 0, 0],  # Units: [RSSI_LOG_2/ADC, ADC]
            'PGA1_GAINS': ['Gain_n6_dB', 'Gain_0_dB', 'Gain_6_dB', 'Gain_12_dB'],
            'PGA2_GAINS': ['Gain_0_dB', 'Gain_6_dB', 'Gain_12_dB', 'Gain_18_dB'],
            'PGA3_GAINS': ['Gain_0_dB', 'Gain_6_dB', 'Gain_12_dB', 'Gain_18_dB'],
            'MIXER_GAINS': ['Gain_1p6_dB', 'Gain_11p2_dB', 'Gain_17p2_dB',
                            'Gain_20p7_dB'],
            'RX_ATTS': ['Atten_0_dB', 'Atten_3_dB', 'Atten_6_dB',
                        'Atten_12_dB', ],
        }
        self.RSSI_DFLT_CFG = {
            'RX_PWR': -55,  # dBm input power from sig_gen
            'ANTENNA': 1,
            'RF_MODE': 13,
            'FREQ_MHZ': 920.25,
            'UPPER_FREQ_REGION': 'FCC',
            'LOWER_FREQ_MHZ': 866.3,
            'LOWER_FREQ_REGION': 'ETSI LOWER',
            'PGA1_GAIN': 'Gain_6_dB',
            'PGA2_GAIN': 'Gain_6_dB',
            'PGA3_GAIN': 'Gain_18_dB',
            'MIXER_GAIN': 'Gain_11p2_dB',
            'RX_ATT': 'Atten_0_dB',
        }

    def init_rssi_data(self):
        rssi_data = {
            'DFLT': 0,
            'ANTENNA': [0] * len(self.RSSI_CFG['ANTENNA_PORTS']),
            'MODES': [0] * len(self.RSSI_CFG['RF_MODES']),
            'UPPER_FREQS_MHZ': [0],
            'LOWER_FREQS_MHZ': [0],
            'PGA1': [0] * len(self.RSSI_CFG['PGA1_GAINS']),
            'PGA2': [0] * len(self.RSSI_CFG['PGA2_GAINS']),
            'PGA3': [0] * len(self.RSSI_CFG['PGA3_GAINS']),
            'MIXER': [0] * len(self.RSSI_CFG['MIXER_GAINS']),
            'RX_ATT': [0] * len(self.RSSI_CFG['RX_ATTS']),
        }
        return rssi_data

    def measure_fwd_power(self):
        """
        Reads power from power meter and Ex10 lo power detector
        :return: power meter reading in dBm, lo power detector ADC code
        """
        time.sleep(self.CAL_CFG['PM_SLEEP_TIME'])
        fwd_pwr = self.power_meter.read_power()
        lo_pdet = self.ex10_reader.measure_adc('lo')
        return fwd_pwr, lo_pdet

    def estimate_dc_offset(self):
        """
        Estimates DC offset value by minimizing ripple between positive and
        negative TX ramps
        :return: Optimal DC offset value
        :rtype: int
        """
        # Set appropriate Tx scalar to avoid compression
        tx_scalar = self.DC_CFG['INIT_TX_SCALAR']
        self.ex10_reader.set_tx_fine_gain(tx_scalar)
        # Back off tx scalar until power is in linear range
        while (self.ex10_reader.measure_adc('lo_sum') >
               self.DC_CFG['MAX_FWD_PWR']):
            tx_scalar = int(tx_scalar * self.DC_CFG['PWR_BACKOFF_SCALAR'])
            self.ex10_reader.set_tx_fine_gain(tx_scalar)
            time.sleep(self.CAL_CFG['PD_SLEEP_TIME'])
        # Determine if DC offset search range should be negative
        self.ex10_reader.tx_ramp_up(dc_offset=0)
        pwr_diff, pwr_p, pwr_n = self.get_fwd_pwr_diff(tx_scalar)
        dc_sign = -1 if pwr_diff > 0 else 1
        # Initialize binary search loop, with search boundaries as exponents
        # to optimize speed
        n_iters = 0
        dc_exp = 0
        dc_exp_low = 4
        dc_exp_high = 18
        while dc_exp_low <= dc_exp_high and n_iters < self.DC_CFG['MAX_ITERS']:
            # Set exponential to be average of bounds
            dc_exp = (dc_exp_low + dc_exp_high) / 2
            self.ex10_reader.tx_ramp_up(dc_offset=dc_sign * int(2 ** dc_exp))
            # Measure power difference between pos and neg tx ramps
            pwr_diff, pwr_p, pwr_n = self.get_fwd_pwr_diff(tx_scalar)
            if abs(pwr_diff) <= self.DC_CFG['PWR_TOL']:
                break
            elif dc_sign * pwr_diff < -1 * self.DC_CFG['PWR_TOL']:
                #  Adjust exponential search bounds for next loop
                dc_exp_low = dc_exp
            else:
                dc_exp_high = dc_exp

            n_iters += 1

        dc_ofs = dc_sign * int(2 ** dc_exp)
        return dc_ofs

    def get_fwd_pwr_diff(self, tx_scalar):
        """
        Gets the power difference between positive and negative Tx-ramps
        :param tx_scalar: Fine gain power scalar
        :return: dB power difference, dBm pos power, dBm neg power
        """
        self.ex10_reader.set_tx_fine_gain(tx_scalar)
        time.sleep(self.CAL_CFG['PD_SLEEP_TIME'])
        pwr_p = self.ex10_reader.measure_adc('lo_sum')
        self.ex10_reader.set_tx_fine_gain(-1 * tx_scalar)
        time.sleep(self.CAL_CFG['PD_SLEEP_TIME'])
        pwr_n = self.ex10_reader.measure_adc('lo_sum')
        pwr_diff = pwr_p - pwr_n
        return pwr_diff, pwr_p, pwr_n

    def set_freq_mhz(self, freq_mhz):
        self.ex10_reader.lock_synthesizer(freq_mhz=freq_mhz)
        self.power_meter.set_frequency(freq_mhz=freq_mhz)

    def reset_dflt_pwr_cal_gains(self):
        self.ex10_reader.set_tx_fine_gain(tx_scalar=self.CAL_CFG['TX_SCALAR'])
        self.ex10_reader.set_coarse_gain(
            tx_atten=self.CAL_CFG['DFLT_COARSE_ATT'])

    def acquire_pwr_cal_data(self, rf_filter='UPPER_BAND', cal_dc_offset=False):
        """
        Acquires data used for calibration power control parameters.
        :param rf_filter: 'UPPER_BAND' or 'LOWER_BAND'
        :param cal_dc_offset: Flag to determine if DC offset will be calibrated
        :return: Calibration data structure
        """

        self._print('Acquiring power control calibration data: ' + rf_filter)

        # Initialize data structure
        data = self.init_pwr_cal_data()

        # Set rf filter
        if rf_filter == 'UPPER_BAND':
            self.ex10_reader.set_region(self.CAL_CFG['DFLT_UPPER_FREQ_REGION'])
            freq_mhz = self.CAL_CFG['DFLT_UPPER_FREQ_MHZ']
            freqs = self.CAL_CFG['UPPER_FREQS_MHZ']
        elif rf_filter == 'LOWER_BAND':
            self.ex10_reader.set_region(self.CAL_CFG['DFLT_LOWER_FREQ_REGION'])
            freq_mhz = self.CAL_CFG['DFLT_LOWER_FREQ_MHZ']
            freqs = self.CAL_CFG['LOWER_FREQS_MHZ']
        else:
            raise ValueError('rf_filter not recognized.')

        # Initialize radio settings
        self.ex10_reader.enable_radio(antenna=self.CAL_CFG['ANTENNA'],
                                      rf_mode=self.CAL_CFG['RF_MODE'])
        self.ex10_reader.radio_power_control(True)
        # Set Tx frequency
        self.set_freq_mhz(freq_mhz)
        # Set default gains
        self.reset_dflt_pwr_cal_gains()
        # Enable the transmitter
        self.ex10_reader.tx_ramp_up()

        # Heat the board up to operating temp
        self._print('Warming up board')
        self.ex10_reader.set_coarse_gain(self.CAL_CFG['SOAK_COARSE_ATT'])
        time.sleep(self.CAL_CFG['SOAK_TIME'])

        # Cal across coarse attenuations
        self._print('Measuring across coarse att.')

        for c_ind, c_val in enumerate(self.CAL_CFG['COARSE_ATTS']):
            self.ex10_reader.set_coarse_gain(c_val)
            (data['COARSE']['FWD_PWR'][c_val],
             data['COARSE']['LO_PDET'][c_val]) = self.measure_fwd_power()
            data['COARSE']['TEMP'][c_val] = self.ex10_reader.measure_adc('temp')
            if cal_dc_offset:
                data['DC_OFS'][c_val] = self.estimate_dc_offset()
                self.ex10_reader.set_tx_fine_gain(
                    tx_scalar=self.CAL_CFG['TX_SCALAR'])
            to_print = (round(data['COARSE']['FWD_PWR'][c_val], 1),
                        data['COARSE']['LO_PDET'][c_val],
                        data['COARSE']['TEMP'][c_val], data['DC_OFS'][c_val])
            self._print(to_print)
            # Stop transmitting above specified limit to protect board
            if (data['COARSE']['FWD_PWR'][c_val]
                    > self.CAL_CFG['FWD_PWR_LIMIT_DBM']):
                break

        self.reset_dflt_pwr_cal_gains()

        data['TEMP'] = self.ex10_reader.measure_adc('temp')

        # Cal across freq
        self._print('Measuring across frequency.')
        for f_ind, f_val in enumerate(freqs):
            self.set_freq_mhz(freq_mhz=f_val)
            for c_ind, c_val in enumerate(self.CAL_CFG['COARSE_ATTS_FREQ']):
                self.ex10_reader.set_coarse_gain(c_val)
                (pwr, pdet_adc) = self.measure_fwd_power()
                data['FREQ']['FWD_PWR'][f_ind].append(pwr)
                data['FREQ']['LO_PDET'][f_ind].append(pdet_adc)
                temp_adc = self.ex10_reader.measure_adc('temp')
                data['FREQ']['TEMP'][f_ind].append(temp_adc)
            to_print = (
                f_val,
                [round(x, 1) for x in data['FREQ']['FWD_PWR'][f_ind]],
                data['FREQ']['LO_PDET'][f_ind])
            self._print(to_print)
        self.set_freq_mhz(freq_mhz)

        self.power_off()
        self._print('Finished acquiring cal data ' + rf_filter)
        return data

    def acquire_rssi_cal_data(self, path_loss=0):
        """
        Acquires data used for receiver RSSI calibration/compensation
        :param path_loss: Signal generator to antenna port path loss (dB)
        :return: Calibration data structure
        """

        self._print('Acquiring RSSI calibration data.')

        # Init cal configs based on SKU
        self.init_rssi_configs()
        # Initialize data structure
        data = self.init_rssi_data()

        # Set up defaults
        self.ex10_reader.radio_power_control(True)
        self.set_mode_antenna()
        self.sig_gen.set_power_dbm(power=self.RSSI_DFLT_CFG['RX_PWR']+path_loss)
        self.sig_gen.on()
        # Set RX gains
        self.set_rssi_gains()
        # Set TX RF filter and frequency
        self.ex10_reader.set_region(self.RSSI_DFLT_CFG['UPPER_FREQ_REGION'])
        self.ex10_reader.lock_synthesizer(self.RSSI_DFLT_CFG['FREQ_MHZ'])

        self.RSSI_CFG['TEMP_COEFS'][1] = self.ex10_reader.measure_adc('temp')

        # Cal across RF Modes
        self._print('----Sweeping RF modes')
        for m, mode in enumerate(self.RSSI_CFG['RF_MODES']):
            # Set mode
            self.set_mode_antenna(rf_mode=mode,
                                  blf_khz=self.BLF_KHZ[mode])
            data['MODES'][m] = self.ex10_reader.read_rssi()
        self.set_mode_antenna()

        self._print('----Sweeping PGA1 gain')
        for g, pga1 in enumerate(self.RSSI_CFG['PGA1_GAINS']):
            # Set RX Gains
            self.set_rssi_gains(pga1_gain=pga1)
            data['PGA1'][g] = self.ex10_reader.read_rssi()
        self.set_rssi_gains()

        self._print('----Sweeping PGA2 gain')
        for g, pga2 in enumerate(self.RSSI_CFG['PGA2_GAINS']):
            # Set RX Gains
            self.set_rssi_gains(pga2_gain=pga2)
            data['PGA2'][g] = self.ex10_reader.read_rssi()
        self.set_rssi_gains()

        self._print('----Sweeping PGA3 gain')
        for g, pga3 in enumerate(self.RSSI_CFG['PGA3_GAINS']):
            # Set RX Gains
            self.set_rssi_gains(pga3_gain=pga3)
            data['PGA3'][g] = self.ex10_reader.read_rssi()
        self.set_rssi_gains()

        self._print('----Sweeping TIA gain')
        for g, tia in enumerate(self.RSSI_CFG['MIXER_GAINS']):
            # Set RX Gains
            self.set_rssi_gains(mixer_gain=tia)
            data['MIXER'][g] = self.ex10_reader.read_rssi()
        self.set_rssi_gains()

        self._print('----Sweeping RX ATT gain')
        for a, rx_atten in enumerate(self.RSSI_CFG['RX_ATTS']):
            # Set RX Gains
            self.set_rssi_gains(rx_att=rx_atten)
            data['RX_ATT'][a] = self.ex10_reader.read_rssi()
        self.set_rssi_gains()

        data['DFLT'] = self.ex10_reader.read_rssi()

        self._print('----Sweeping FREQS')
        self.ex10_reader.set_region(self.RSSI_DFLT_CFG['LOWER_FREQ_REGION'])
        self.ex10_reader.lock_synthesizer(self.RSSI_DFLT_CFG['LOWER_FREQ_MHZ'])
        self.set_mode_antenna(freq_mhz=self.RSSI_DFLT_CFG['LOWER_FREQ_MHZ'])
        data['LOWER_FREQS_MHZ'] = self.ex10_reader.read_rssi()
        self.ex10_reader.set_region(self.RSSI_DFLT_CFG['UPPER_FREQ_REGION'])
        self.ex10_reader.lock_synthesizer(self.RSSI_DFLT_CFG['FREQ_MHZ'])
        self.set_mode_antenna(freq_mhz=self.RSSI_DFLT_CFG['FREQ_MHZ'])
        data['UPPER_FREQS_MHZ'] = self.ex10_reader.read_rssi()

        # Check antenna path losses
        self._print('----Sweeping ANTENNA paths')
        for a, antenna in enumerate(self.RSSI_CFG['ANTENNA_PORTS']):
            input('Change to antenna port {} and press ENTER'.format(
                antenna))
            self.set_mode_antenna(antenna=antenna)
            data['ANTENNA'][a] = self.ex10_reader.read_rssi()

        return data

    def set_rssi_gains(self,
                       rx_att=None,
                       pga1_gain=None,
                       pga2_gain=None,
                       pga3_gain=None,
                       mixer_gain=None,
                       ):

        analog_rx = aar.APPLICATION_ADDRESS_RANGE['RxGainControl']['fields']
        rx_atten_enums = analog_rx['RxAtten']['enums']
        pga1_enums = analog_rx['Pga1Gain']['enums']
        pga2_enums = analog_rx['Pga2Gain']['enums']
        pga3_enums = analog_rx['Pga3Gain']['enums']
        mixer_enums = analog_rx['MixerGain']['enums']

        rx_att = rx_att if rx_att is not None else self.RSSI_DFLT_CFG['RX_ATT']
        pga1_gain = pga1_gain if pga1_gain is not None else self.RSSI_DFLT_CFG[
            'PGA1_GAIN']
        pga2_gain = pga2_gain if pga2_gain is not None else self.RSSI_DFLT_CFG[
            'PGA2_GAIN']
        pga3_gain = pga3_gain if pga3_gain is not None else self.RSSI_DFLT_CFG[
            'PGA3_GAIN']
        mixer_gain = (mixer_gain if mixer_gain is not None else
                      self.RSSI_DFLT_CFG['MIXER_GAIN'])

        rx_config = {
            'RxAtten': rx_atten_enums[rx_att],
            'Pga1Gain': pga1_enums[pga1_gain],
            'Pga2Gain': pga2_enums[pga2_gain],
            'Pga3Gain': pga3_enums[pga3_gain],
            'MixerGain': mixer_enums[mixer_gain],
            'MixerBandwidth': True,
            'Pga1RinSelect': False
        }
        self.ex10_reader.set_analog_rx_config(rx_config)

    def set_mode_antenna(self,
                         rf_mode=None,
                         blf_khz=None,
                         antenna=None,
                         freq_mhz=None):
        freq_mhz = freq_mhz if freq_mhz is not None else self.RSSI_DFLT_CFG[
            'FREQ_MHZ']
        rf_mode = rf_mode if rf_mode is not None else self.RSSI_DFLT_CFG[
            'RF_MODE']
        blf_khz = blf_khz if blf_khz is not None else self.BLF_KHZ[
            self.RSSI_DFLT_CFG['RF_MODE']]
        antenna = antenna if antenna is not None else self.RSSI_DFLT_CFG[
            'ANTENNA']

        self.sig_gen.set_freq_mhz(freq_mhz + blf_khz / 1e3)
        self.ex10_reader.enable_radio(antenna=antenna,
                                      rf_mode=rf_mode)
        time.sleep(0.05)

    @staticmethod
    def argmin_2(num_list):
        """
        Calculates indices of a number list with the two lowest values
        :param num_list: Python list or tuple of numbers
        """
        array = list(num_list)
        index1 = array.index(min(array))
        array.pop(index1)
        index2 = array.index(min(array))
        index2 = index2 if index2 < index1 else index2 + 1  # compensate for pop
        return index1, index2

    def _valid_pdet_adc(self, adc):
        valid_min_adc, valid_max_adc = self.CAL_CFG['PDET_ADC_MIN_MAX']
        return (adc >= valid_min_adc) and adc <= valid_max_adc

    def _adc_to_power(self, adc, powers, adcs):
        if not self._valid_pdet_adc(adc):
            return -0xFF, -0xFF
        i1, i2 = self.argmin_2([abs(i - adc) for i in adcs])
        power = powers[i1] + (adc - adcs[i1]) * (
                powers[i2] - powers[i1]) / (adcs[i2] - adcs[i1])
        return round(power, 2), i1

    def store_pwr_cal_params(self, data, rf_filter):
        """
        Calculates calibration parameters from acquired data.
        """
        self._print('Calculating cal params ' + rf_filter)

        if rf_filter == 'UPPER_BAND':
            rf_cal_prefix = 'UpperBand'
            freqs = self.CAL_CFG['UPPER_FREQS_MHZ']
        elif rf_filter == 'LOWER_BAND':
            rf_cal_prefix = 'LowerBand'
            freqs = self.CAL_CFG['LOWER_FREQS_MHZ']
        else:
            raise ValueError('rf_filter not recognized.')

        # Save cal bands for each rf_filter
        self.info.set_parameter(
            'RfFilter' + rf_cal_prefix,
            tuple([min(freqs), max(freqs)]))

        # Set temperature compensation params
        self.info.set_parameter(rf_cal_prefix + 'LoPdetTempSlope',
                                tuple(self.CAL_CFG['PDET_PWR_PER_TEMP_ADC']))

        self.info.set_parameter(rf_cal_prefix + 'FwdPowerTempSlope',
                                tuple([self.CAL_CFG['DB_PER_ADC_FWD_PWR']]))

        self.info.set_parameter(rf_cal_prefix + 'CalTemp',
                                tuple([data['TEMP']]))

        # Store PDET ADC values as LUT
        pdet_lut = tuple(
            zip(*data['COARSE']['LO_PDET']))[0:self.CAL_CFG['NUM_PDET_BLOCKS']]
        self.info.set_parameter(
            rf_cal_prefix + 'PdetAdcLut',
            sum(pdet_lut, ()))

        # Calc freq power offsets in PDETs
        r_coarse_atts = range(len(self.CAL_CFG['COARSE_ATTS_FREQ']))
        r_pdet_blocks = range(self.CAL_CFG['NUM_PDET_BLOCKS'])
        pwr_diff_over_freq = [[0 for i in freqs] for j in r_pdet_blocks]
        for p_ind in r_pdet_blocks:
            for c_ind in r_coarse_atts:
                for f_ind, freq in enumerate(freqs):
                    # First calculate what the pwr should be from initial cal
                    adc = data['FREQ']['LO_PDET'][f_ind][c_ind][p_ind]
                    pwr, i = self._adc_to_power(adc,
                                                data['COARSE']['FWD_PWR'],
                                                pdet_lut[p_ind])
                    # Next comp temperature
                    if pwr != -0xFF:  # not invalid power
                        pwr += self.CAL_CFG['PDET_PWR_PER_TEMP_ADC'][p_ind] * (
                                data['COARSE']['TEMP'][i] -
                                data['FREQ']['TEMP'][f_ind][c_ind])
                        pwr_diff_over_freq[p_ind][f_ind] = int((pwr - data[
                            'FREQ']['FWD_PWR'][f_ind][c_ind]) * 100)

        pwr_diff_over_freq = tuple([f for p in pwr_diff_over_freq for f in p])
        self.info.set_parameter(
            rf_cal_prefix + 'LoPdetFreqLut',
            pwr_diff_over_freq)

        self.info.set_parameter(
            rf_cal_prefix + 'LoPdetFreqs',
            tuple(freqs))

        # Store forward power data
        self.info.set_parameter(
            rf_cal_prefix + 'FwdPowerCoarsePwrCal',
            tuple(data['COARSE']['FWD_PWR']))

        # Store forward power freq shifts
        dflt_pwr = data['COARSE']['FWD_PWR'][
            self.CAL_CFG['COARSE_ATTS_FREQ'][0]]
        power_shifts_over_freq = [i[0] - dflt_pwr for i in data['FREQ'][
            'FWD_PWR']]
        self.info.set_parameter(rf_cal_prefix + 'FwdPwrFreqLut',
                                tuple(power_shifts_over_freq))

        self._print('Calibration parameters done ' + rf_filter)

    def store_dc_cal_params(self, data):
        # Store DC offset estimates
        self.info.set_parameter('DcOffsetCal', tuple(data['DC_OFS']))

    def store_rssi_cal_params(self, rssi_data):
        baseline_rssi = rssi_data['DFLT']  # RSSI value at default settings

        # Store baseline RSSI value and input power
        self.info.set_parameter('RssiRxDefaultPwr',
                                tuple([self.RSSI_DFLT_CFG['RX_PWR']]))
        self.info.set_parameter('RssiRxDefaultLog2', tuple([baseline_rssi]))

        # Store RF modes and RF mode RSSI offsets
        rf_mode_cal_pad = [0] * (self.RSSI_CFG['N_RF_MODES'] - len(
            self.RSSI_CFG['RF_MODES']))
        self.info.set_parameter('RssiRfModes',
                                tuple(self.RSSI_CFG['RF_MODES']
                                    + rf_mode_cal_pad))
        mode_offsets = [i - baseline_rssi for i in rssi_data['MODES']]
        self.info.set_parameter('RssiRfModeLut',
                                tuple(mode_offsets + rf_mode_cal_pad))

        # Store PGA1 offsets
        pga1_offsets = [i - baseline_rssi for i in rssi_data['PGA1']]
        self.info.set_parameter('RssiPga1Lut',
                                tuple(pga1_offsets))

        # Store PGA2 offsets
        pga2_offsets = [i - baseline_rssi for i in rssi_data['PGA2']]
        self.info.set_parameter('RssiPga2Lut',
                                tuple(pga2_offsets))

        # Store PGA3 offsets
        pga3_offsets = [i - baseline_rssi for i in rssi_data['PGA3']]
        self.info.set_parameter('RssiPga3Lut',
                                tuple(pga3_offsets))

        # Store Mixer Gain offsets
        mixer_gain_offsets = [i - baseline_rssi for i in rssi_data['MIXER']]
        self.info.set_parameter('RssiMixerGainLut',
                                tuple(mixer_gain_offsets))

        # Store RX Att offsets
        rx_att_offsets = [i - baseline_rssi for i in rssi_data['RX_ATT']]
        self.info.set_parameter('RssiRxAttLut',
                                tuple(rx_att_offsets))

        # Store Antenna offsets
        antenna_cal_pad = [0] * (self.RSSI_CFG['N_ANTENNA_PORTS']
                - len(self.RSSI_CFG['ANTENNA_PORTS']))
        self.info.set_parameter('RssiAntennas',
                                tuple(self.RSSI_CFG['ANTENNA_PORTS']
                                    + antenna_cal_pad))
        antenna_offsets = [i - baseline_rssi for i in rssi_data['ANTENNA']]
        self.info.set_parameter('RssiAntennaLut',
                                tuple(antenna_offsets + antenna_cal_pad))

        # Store Upper Band RSSI offsets across frequency
        self.info.set_parameter('UpperBandRssiFreqOffset',
                                tuple([rssi_data['UPPER_FREQS_MHZ'] -
                                      baseline_rssi]))

        # Store Lower Band RSSI offsets across frequency
        self.info.set_parameter('LowerBandRssiFreqOffset',
                                tuple([rssi_data['LOWER_FREQS_MHZ'] -
                                      baseline_rssi]))

        # Store RSSI Temp Coefficients
        self.info.set_parameter('RssiTempSlope',
                                tuple([self.RSSI_CFG['TEMP_COEFS'][0]]))
        self.info.set_parameter('RssiTempIntercept',
                                tuple([self.RSSI_CFG['TEMP_COEFS'][1]]))


    def store_cal_version_params(self, board_id):
        # Store cal version number
        self.info.set_parameter(
            'CalibrationVersion', tuple([self.CAL_CFG['CAL_VERSION']]))

        # User board ID
        self.info.set_parameter(
            'UserBoardId',
            tuple([board_id]))

        # Tx scalar
        self.info.set_parameter(
            'TxScalarCal',
            tuple([self.CAL_CFG['TX_SCALAR']]))

        # Valid PDET ADC range for each PDET block
        self.info.set_parameter(
            'ValidPdetAdcs',
            tuple(self.CAL_CFG['PDET_ADC_MIN_MAX']))

        # Control loop params
        self.info.set_parameter(
            'ControlLoopParams',
            tuple([self.CAL_CFG['LOOP_GAIN_DIVISOR'],
                   self.CAL_CFG['ADC_ERROR_THRESHOLD'],
                   self.CAL_CFG['MAX_ITERATIONS']]))

    def write_calibration_info_page(self):
        """
        Create bytestring of calibration data and write to device
        """
        bytestream = self.info.to_info_page_string()
        self.ex10_reader.write_cal_info_page(bytestream)

    def read_calibration_info_page(self):
        """
        This method reads the calibration data from the info page on
        Ex10 and puts it into a calib_info object and returns it
        """
        data = self.ex10_reader.read_cal_info_page()
        self.info.from_info_page_string(data)

    def power_off(self):
        """
        Stop and ramp down
        """
        self.ex10_reader.stop_transmitting()

def run_pc_cal(uart_helper, power_meter, sig_gen, path_loss, board_id, verbose, debug_serial):
    """
    Executes calibration procedure: acquires calibration data, calculates
    calibration params, and writes cal table into flash.
    :param uart_helper: helper object for serial interface
    :param power_meter: power meter VISA object
    :param sig_gen: signal generator VISA object
    :param path_loss: Signal generator to antenna port path loss (dB)
    :param board_id: R807 board ID number
    :param verbose: verbose flag for printing
    :param debug_serial: flag for dumping all serial data to/from device
    """
    uart_helper.open_port(UartBaud.RATE115200)
    ex10_reader = UartReader(uart_helper)
    ex10_reader.dump_serial(debug_serial)

    cal_example = Ex10CalibrationExample(power_meter=power_meter,
                                         sig_gen=sig_gen,
                                         ex10_reader=ex10_reader,
                                         verbose=verbose)

    board_id, json_file_to_write = cal_example.get_board_id(board_id)
    rssi_data = cal_example.acquire_rssi_cal_data(path_loss=path_loss)
    upper_data = cal_example.acquire_pwr_cal_data(rf_filter='UPPER_BAND',
                                                  cal_dc_offset=True)
    lower_data = cal_example.acquire_pwr_cal_data(rf_filter='LOWER_BAND')

    cal_example.store_cal_version_params(board_id=board_id)
    cal_example.store_pwr_cal_params(data=upper_data, rf_filter='UPPER_BAND')
    cal_example.store_pwr_cal_params(data=lower_data, rf_filter='LOWER_BAND')
    cal_example.store_dc_cal_params(data=upper_data)
    cal_example.store_rssi_cal_params(rssi_data)
    cal_example.info.dump_params()
    cal_example.info.to_json(json_file_to_write)
    cal_example.power_off()

    #  Writing cal info page
    cal_example.write_calibration_info_page()

    # Test cal info page written correctly
    cal_example.read_calibration_info_page()
    print(*cal_example.info.dump_params(), sep=os.linesep)

    print('Calibration complete.')


def run_pc_cal_example():
    print('Ex10 Development Kit Calibration example')

    try:
        # Set up
        parser = argparse.ArgumentParser(
           description='Calibrates Ex10 R807 development board from PC')
        parser.add_argument('-v', '--verbose',
                            default=False,
                            action='store_true',
                            help='Verbose flag')
        parser.add_argument('-b', '--board_id', type=int,
                            help='Serial number of R807 board, '
                                 'between 0 and 65535')
        parser.add_argument('-d', '--debug_serial', default=False, action='store_true',
                            help='Flag to print out all RX serial data')
        parser.add_argument('-o', '--power_offset', type=float,
                            help='Power offset setting for power meter')
        parser.add_argument('-l', '--path_loss', type=float,
                            help='Path loss in sig-gen to R807 for RSSI cal')
        args = parser.parse_args()

        # Use selected port with speed 115200, 8n1
        uart_helper = UartHelper()
        uart_helper.choose_serial_port()

        power_meter = PowerMeter(ofs=args.power_offset)
        sig_gen = KeysightSigGen()

        run_pc_cal(uart_helper, power_meter, sig_gen,
                args.path_loss, args.board_id, args.verbose, args.debug_serial)
    finally:
        uart_helper.shutdown()


if __name__ == "__main__":
    run_pc_cal_example()
