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
import argparse
import os
import sys

# Append this script's parent dir to path so that modules in ex10_api can be
# imported with "ex10_api.module".
parent_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.append(parent_dir)

# Equipment addresses can be GPIB or USB.  For example
# addr="GPIB0::19::INSTR", etc.
power_meter_addr = "USB0::0x2A8D::0x0701::MY59260011::0::INSTR"

from cal.power_meter import PowerMeter
from cal.uart_helper import UartBaud, UartHelper
from cal.uart_reader import UartReader
import cal.calib_info as calib_info


class Ex10CalibrationTest:
    """
    Calibration test class for Ex10 reader.
    """
    # The configuration of the cal test is set using the definitions below
    SLEEP_TIME = 0.2  # Time to wait for power meter read
    RF_MODE = 5  # RF mode
    R807_ANTENNA_PORT = 1  # Antenna select
    UPPER_REGION_EXAMPLE = 'FCC'  # Example of upper band region
    LOWER_REGION_EXAMPLE = 'ETSI LOWER'  # Example of lower band region
    FREQUENCIES = [865.7, 867.5, 903.25, 915.75, 927.25]  # Test frequencies
    CW_TOL_HIGH_PWR = 0.5  # CW accuracy tolerance >20 dBm
    CW_TOL_LOW_PWR = 1.0  # CW accuracy tolerance <20 dBm
    DC_OFFSET_TOL = 0.2  # DC offset accuracy tolerance
    CW_RANGE_HIGH = [20, 25, 30, 31]  # Tx output >20 dBm
    CW_RANGE_LOW = [5, 10, 15]  # Tx output <20 dBm
    DC_OFFSET_TEST_FREQ = 903.25  # Frequency in MHz to test DC offset
    DC_COARSE_ATTS = [7, 10, 15, 20, 25, 30]  # Coarse gains for DC offset test

    def __init__(self, power_meter, ex10_reader, verbose=True):
        self.power_meter = power_meter
        self.ex10_reader = ex10_reader
        self.cal_info = calib_info.CalibrationInfoPageAccessor()
        cal_data = self.ex10_reader.read_cal_info_page()
        self.cal_info.from_info_page_string(cal_data)
        self.verbose = verbose

    def _print(self, *args):
        if self.verbose:
            print(*args)

    def test_cw_on(self,
                   freq_mhz,
                   power_level):
        """
        Tests power ramp accuracy
        :param freq_mhz: Tx LO frequency
        :param power_level: 'high' or 'low' input power
        :return: A list of cw power-specific tuples.
        """
        if power_level == 'high':
            power_targets = self.CW_RANGE_HIGH
        else:
            power_targets = self.CW_RANGE_LOW
        results = [(0,)] * len(power_targets)
        print('Testing power ramp at {} MHz at {} power'.format(freq_mhz,
                                                                power_level))
        # Loop through power values
        for power_ind, power_target in enumerate(power_targets):
            self.ex10_reader.cw_test(antenna=self.R807_ANTENNA_PORT,
                                     rf_mode=self.RF_MODE,
                                     tx_power_dbm=power_target,
                                     freq_mhz=freq_mhz,
                                     remain_on=True)
            time.sleep(self.SLEEP_TIME)  # Delay to allow power meter to settle
            power_pm = self.power_meter.read_power()

            results[power_ind] = (power_target,
                                  round(power_pm, 1),
                                  round(power_pm - power_target, 2))
            print(results)
        return results

    def test_dc_offset(self):
        """"
        Tests DC offset compenation accuracy
        :return: A list of DC offset specific tuples.
        """
        results = [(0,)] * len(self.DC_COARSE_ATTS)
        # Set up hardware
        print('Testing DC offsets')
        # Loop through coarse gain values
        for c_ind, c_val in enumerate(self.DC_COARSE_ATTS):
            dc_offset = self.cal_info.get_parameter('DcOffsetCal')[c_val]
            self.ex10_reader.set_coarse_gain(c_val)
            self.ex10_reader.set_tx_fine_gain(
                self.cal_info.get_parameter('TxScalarCal')[0])
            self.ex10_reader.tx_ramp_up(dc_offset=dc_offset)

            pwr_diff, pwr_p, pwr_n = self.get_fwd_pwr_diff(
                (c_val + 3) * 50)  # Set Tx_scalar roughly

            results[c_ind] = (c_val,
                              dc_offset,
                              round(pwr_p, 1),
                              round(pwr_n, 1),
                              round(pwr_diff, 2))
        return results

    def get_fwd_pwr_diff(self, tx_scalar):
        """
        Gets the power difference between positive and negative Tx-ramps
        :param tx_scalar: Fine gain power scalar
        :return: dB power difference, dBm pos power, dBm neg power
        """
        self.ex10_reader.set_tx_fine_gain(tx_scalar)
        time.sleep(self.SLEEP_TIME)
        pwr_p = self.power_meter.read_power()
        self.ex10_reader.set_tx_fine_gain(-1 * tx_scalar)
        time.sleep(self.SLEEP_TIME)
        pwr_n = self.power_meter.read_power()
        pwr_diff = pwr_p - pwr_n
        return pwr_diff, pwr_p, pwr_n

    def set_frequency(self, freq_mhz):
        """
        Sets up RF filter based on the Tx frequency
        :param freq_mhz: Target frequency in MHz
        """
        lower_rf_filter_bands = self.cal_info.get_parameter(
            'RfFilterLowerBand')
        upper_rf_filter_bands = self.cal_info.get_parameter(
            'RfFilterUpperBand')

        if round(lower_rf_filter_bands[0], 2) <= freq_mhz <= round(
                lower_rf_filter_bands[1], 2):
            region = self.LOWER_REGION_EXAMPLE
            self.ex10_reader.set_region(region)
            rf_cal_prefix = 'LowerBand'
        elif round(upper_rf_filter_bands[0], 2) <= freq_mhz <= round(
                upper_rf_filter_bands[1], 2):
            region = self.UPPER_REGION_EXAMPLE
            self.ex10_reader.set_region(region)
            rf_cal_prefix = 'UpperBand'
        else:
            raise ValueError('Frequency {} MHz not in calibration range'.
                             format(freq_mhz))

        self.ex10_reader.enable_radio(antenna=self.R807_ANTENNA_PORT,
                                      rf_mode=self.RF_MODE)
        self.ex10_reader.radio_power_control(True)
        self.ex10_reader.lock_synthesizer(freq_mhz)
        self.power_meter.set_frequency(freq_mhz)
        return region, rf_cal_prefix

    def power_off(self):
        """
        Shuts down CW and radio power, and closes SPI connection.
        """
        # Disable transmitter
        self.ex10_reader.stop_transmitting()

    def run_power_control_tests(self):
        start_time = time.time()

        # warm board
        region, _ = self.set_frequency(915)
        self.ex10_reader.cw_test(self.R807_ANTENNA_PORT,
                                 self.RF_MODE,
                                 30,
                                 915,
                                 True)
        time.sleep(self.SLEEP_TIME)

        # Test CW_on accuracy
        n_cw_high_errs = 0
        n_cw_low_errs = 0
        for freq_mhz in self.FREQUENCIES:
            self.set_frequency(freq_mhz)
            cw_pwr_returns_high = self.test_cw_on(freq_mhz, 'high')
            cw_pwr_returns_low = self.test_cw_on(freq_mhz, 'low')

            # Compare power desired to power measured
            for cw_pwr_return_high in cw_pwr_returns_high:
                self._print(cw_pwr_return_high)
            for cw_pwr_return_low in cw_pwr_returns_low:
                self._print(cw_pwr_return_low)

            n_cw_high_array = [abs(r[2]) > self.CW_TOL_HIGH_PWR for r in
                               cw_pwr_returns_high]
            n_cw_low_array = [abs(r[2]) > self.CW_TOL_LOW_PWR for r in
                              cw_pwr_returns_low]
            n_cw_high_errs += sum(n_cw_high_array)
            n_cw_low_errs += sum(n_cw_low_array)

        # Test DC offset estimation accuracy
        self.set_frequency(self.DC_OFFSET_TEST_FREQ)
        dc_offset_returns = self.test_dc_offset()

        for dc_offset_return in dc_offset_returns:
            self._print(dc_offset_return)
        n_dc_array = [abs(r[4]) > self.DC_OFFSET_TOL for r in dc_offset_returns]
        n_dc_errs = sum(n_dc_array)
        self.power_off()

        test_time = time.time()

        print('Test time: {}'.format(round(test_time - start_time)))

        self._print('{} high-pwr CW errors'.format(
            n_cw_high_errs)) if n_cw_high_errs else None
        self._print('{} low-pwr CW errors'.format(
            n_cw_low_errs)) if n_cw_low_errs else None
        self._print('{} DC-offset errors'.format(
            n_dc_errs)) if n_dc_errs else None

        return sum([n_cw_high_errs,
                    n_cw_low_errs,
                    n_dc_errs])


def run_pc_cal_test(uart_helper):
    parser = argparse.ArgumentParser(
        description='Tests Ex10 R807 development board calibration from PC')
    parser.add_argument('-d', '--debug_serial', default=False, action='store_true',
                        help='Flag to print out all RX serial data')
    parser.add_argument('-v', '--verbose', default=False, action='store_true',
                        help='Flag to print out cal and test data')
    parser.add_argument('-o', '--power_offset', type=float,
                        help='Power offset setting for power meter')
    args = parser.parse_args()

    uart_helper.open_port(UartBaud.RATE115200)

    power_meter = PowerMeter(addr=power_meter_addr, ofs=args.power_offset)
    ex10_reader = UartReader(uart_helper)
    ex10_reader.dump_serial(args.debug_serial)
    cal_test = Ex10CalibrationTest(power_meter=power_meter,
                                   ex10_reader=ex10_reader,
                                   verbose=args.verbose)

    num_power_accuracy_errors = cal_test.run_power_control_tests()
    print('Total number of power accuracy errors: {}'.format(
        num_power_accuracy_errors))

    print('Calibration test complete.')


if __name__ == "__main__":
    print('Ex10 Development Kit PC Calibration Test')

    # Use selected port with speed 115200, 8n1
    uart_helper = UartHelper()
    uart_helper.choose_serial_port()

    try:
        run_pc_cal_test(uart_helper=uart_helper)
    finally:
        uart_helper.shutdown()
