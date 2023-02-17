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

import sys
import ctypes
from ctypes import *
import struct
import time
from datetime import datetime
from py2c_interface.py2c_python_wrapper import *

VERSION_STRING_SIZE = 120
RADIO_CONTROL_BASE = 0x4000c684
YK_MODEM_CONFIG_BASE = 0x4000c004


class Ex10Expert(object):
    """
    A class to allow helper functions for the Ex10 library.
    """
    def __init__(self, py2c_wrapper):
        """
        @param py2c_wrapper: The py2c wrapper object. This is the class
        into which all calls to the C lib must be made. We make calls into
        this class which are then forwarded to the C libs.
        """
        from utils.peek_poker import PeekPoker
        from utils.yukon_peripherals import get_yukon_peripherals
        from py2c_interface.py2c_sjc_accessor import RxSjcAccessor

        self._py2c           = py2c_wrapper
        self._ex10_helpers   = py2c_wrapper.get_ex10_helpers()
        self._ex10_reader    = py2c_wrapper.get_ex10_reader()
        self._ex10_ops       = py2c_wrapper.get_ex10_ops()
        self._ex10_protocol  = py2c_wrapper.get_ex10_protocol()
        self._ex10_sjc       = RxSjcAccessor(py2c_wrapper)
        self._yk_peripherals = get_yukon_peripherals(PeekPoker(self._py2c), 4)

    def get_app_version(self):
        ver_info                    = (c_char * VERSION_STRING_SIZE)()
        image_validity              = ImageValidityFields(1, 1)
        remain_reason               = RemainReasonFields()
        remain_reason.remain_reason = 0
        self._py2c.get_ex10_version().get_application_info(ver_info,
                                                           VERSION_STRING_SIZE,
                                                           pointer(image_validity),
                                                           pointer(remain_reason))

        return ver_info.value.decode()

    def get_sku(self):
        return self._py2c.get_ex10_version().get_sku()

    def configure_pga3_to_atest_output(self):
        """ Configure ATEST pins to output differential PGA3 I and Q signals """
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_0D |= {'RX_PGA3I_TSTSW_EN': 1,
                                                             'RX_PGA3Q_TSTSW_EN': 1}

        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_0E = {'AUX_MUXCTRL0': 'rx_pga3q_tst_out_p'}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_0F = {'AUX_MUXCTRL1': 'rx_pga3q_tst_out_n'}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_10 = {'AUX_MUXCTRL2': 'rx_pga3i_tst_out_p'}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_11 = {'AUX_MUXCTRL3': 'rx_pga3i_tst_out_n'}

    def configure_sdd_logs(self, log_speed_mhz=0, **log_names):
        """
        Configure yukon to dump out the specified logs.
        Log names can be found @ dev_kit/ex10_dev_kit/py2c_interface/py2c_python_auto_regs.py

        :param log_speed_mhz:   SPI clock speed in MHz
        :param log_names:       kwargs of the logs to enable
        """

        if log_speed_mhz:
            log_speed = LogSpeedFields()
            log_speed.speed_mhz = log_speed_mhz
            self._ex10_protocol.write('LogSpeed', log_speed)

        log_bits = LogEnablesFields()
        for log_name, value in log_names.items():
            getattr(log_bits, log_name)         # check if the log type exists / was spelled correctly
            setattr(log_bits, log_name, value)  # update log type value
        self._ex10_protocol.write('LogEnables', log_bits)

    def set_cdac_range(self, cdac_i, cdac_q):
        """
        Set the CDAC I and Q range registers prior to running the RxRunSjcOp op
        :param cdac_i: CdacRange value to set the search range for I.
        :param cdac_q: CdacRange value to set the search range for Q.
        """
        from py2c_interface.py2c_python_wrapper import CdacRange

        cdac_range_i = CdacRange()
        cdac_range_i.center     = cdac_i.center
        cdac_range_i.limit      = cdac_i.limit
        cdac_range_i.step_size  = cdac_i.step_size

        cdac_range_q = CdacRange()
        cdac_range_q.center     = cdac_q.center
        cdac_range_q.limit      = cdac_q.limit
        cdac_range_q.step_size  = cdac_q.step_size

        self._py2c.get_ex10_sjc().set_cdac_range(cdac_range_i, cdac_range_q)

    def set_external_lo(self):
        """ Use the External LO instead of the Internal RFSynth """

        # Turn off RFSynth
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_09 |= {'RFS_VCO_EN'        : 0,
                                                             'RFS_VCO_GMR_EN'    : 0}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_0A |= {'RFS_CT_EN'         : 0}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_12 |= {'RFS_BUFLDO_EN'     : 0,
                                                             'RFS_VCOLDO_EN'     : 0,
                                                             'RFS_LOGICLDO_EN'   : 0,
                                                             'RFS_ANALDO_EN'     : 0}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_19 |= {'RFS_LO_RX_BUF_EN'  : 0}

        # Enable the External LO signal -> RX/TX mixer switches
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_05 |= {'RX_SJC_LO_EN'      : 1}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_06 |= {'RX_LO_EXT_SEL'     : 1,
                                                             'RX_LO_SEL'         : 1,
                                                             'TX_PI_EN'          : 1,
                                                             'TX_PI_VDIV_DISC'   : 1,
                                                             'TX_MIXER_ALLI_EN'  : 1}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_14 |= {'BIAS_EN_IB20_FIX_0': 0xFFFFFFFF}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_15 |= {'BIAS_EN_IB20_FIX_1': 0x1FFFFFFF}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_16 |= {'BIAS_EN_IB20_TUNE' : 0x3FFFFFFF}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_18 |= {'BIAS_EN_IB4_TUNE33': 0xFFFFF}
        self._yk_peripherals.RADIO_CONTROL.RADIO_CTRL_1B |= {'RX_EXTLOLDO_EN'    : 1,
                                                             'RX_SJC_TX_LO_EN'   : 1}

    def reset_layers(self):
        self._ex10_helpers = self._py2c.get_ex10_helpers()
        self._ex10_reader = self._py2c.get_ex10_reader()
        self._ex10_ops = self._py2c.get_ex10_ops()

    def __getattr__(self, name):
        """ Allows for passing functions directly to the reader layer """
        try:
            return getattr(self._ex10_reader, name)
        except:
            assert(sys.exc_info()[0])

    def from_app_to_bootloader(self):
        ex10_protocol = self._py2c.get_ex10_protocol()
        ex10_protocol.reset(Status.Bootloader)

    def from_bootloader_to_app(self):
        ex10_protocol = self._py2c.get_ex10_protocol()
        ex10_protocol.reset(Status.Application)

    def clear_fifo_packets(self, print_packets=False):
        self._ex10_helpers.discard_packets(print_packets, True, False)

    def set_region(self, region_name):
        """
        Set the region of operation.
        :param region_name: Must be one of the ShortName entries found in the
                            regions.yml file.
        """
        import py2c_interface.py2c_python_wrapper as ppw
        self._py2c.get_ex10_region().init(region_name.encode('ascii'), ppw.TCXO_FREQ_KHZ)

    def check_for_errors(self, error_status):
        import py2c_interface.py2c_python_auto_enums as ppae

        if error_status.error_occurred:
            if error_status.ops_status.error == ppae.OpsStatus.ErrorPowerControlTargetFailed:
                print("Error occurred in power ramp")
            else:
                raise Exception("Error occured in cw_test")

    def lock_synthesizer(self, antenna, rf_mode, frequency_khz):
        # Set up configs according to passed params
        import py2c_interface.py2c_python_wrapper as ppw

        config = ppw.CwConfig()
        ret_status = self._ex10_reader.build_cw_configs(
            antenna, rf_mode, 3000, int(frequency_khz), False, pointer(config))
        self.check_for_errors(ret_status)
        ret_status = self._ex10_ops.lock_synthesizer(config.synth.r_divider,
                                                     config.synth.n_divider)
        self.check_for_errors(ret_status)

    def radio_setup(self, antenna, rf_mode, frequency_khz, rf_filter, pa_bias_enable=True):
        # Set up configs according to passed params
        import py2c_interface.py2c_python_wrapper as ppw
        from py2c_interface.py2c_python_auto_enums import AuxAdcResultsAdcResult
        config = ppw.CwConfig()
        ret_status = self._ex10_reader.build_cw_configs(
            antenna, rf_mode, 3000, int(frequency_khz), False, pointer(config))
        self.check_for_errors(ret_status)

        self.wait_op_completion()

        ex10_cal = self._py2c.get_ex10_calibration(
            self._py2c.get_ex10_protocol().ex10_protocol).contents
        ex10_board_spec = self._py2c.get_ex10_board_spec()
        # Grab current temp
        temp_ret = c_uint16(0)
        ret_status = self._ex10_ops.measure_aux_adc(
            AuxAdcResultsAdcResult.AdcResultTemperature, 1, pointer(temp_ret))
        assert ret_status.error_occurred == False
        temp_adc = struct.unpack('H', temp_ret)[0]
        # Override some config stuff based on passed rf filter to not be reliant on c sdk region
        config.power = ex10_cal.get_power_control_params(
                3000, int(frequency_khz), temp_adc, False, rf_filter)
        config.gpio.output_level = ex10_board_spec.get_gpio_output_levels(
                antenna, rf_mode, int(frequency_khz), rf_filter)
        # Rest of board setup based on passed params

        self._ex10_ops.set_gpio(config.gpio.output_level,
                                config.gpio.output_enable)
        self.wait_op_completion()
        self._ex10_ops.radio_power_control(pa_bias_enable)
        self.wait_op_completion()
        self._ex10_ops.set_rf_mode(config.rf_mode)
        self._ex10_ops.lock_synthesizer(config.synth.r_divider,
                                        config.synth.n_divider)

    def run_ber_op(self,
                   antenna,
                   rf_mode,
                   tx_power_dbm,
                   num_bits,
                   num_packets,
                   delimiter_only=False,
                   freq_mhz=0,
                   timeout_s=10):
        """
        Run a packetized BER test.

        :param antenna:         r807 antenna port
        :param rf_mode:         yukon RF mode
        :param tx_power_dbm:    transmit power
        :param num_bits:        number of bits per packet
        :param num_packets:     number of packets to receive
        :param delimiter_only:  use a delimiter instead of a query to trigger a packet
        :param freq_mhz:        carrier frequency. If 0 or not provided, will choose a random frequency
        :param timeout_s:       time to wait for BER test completion
        """
        self.stop_transmitting()
        self._ex10_reader.cw_test(antenna, rf_mode, int(tx_power_dbm*100), int(freq_mhz * 1e3), True)
        self._ex10_ops.start_ber_test(num_bits, num_packets, delimiter_only)
        self._ex10_ops.wait_op_completion_with_timeout(int(1000 * timeout_s))
        self.clear_fifo_packets()

    def tx_ramp_up(self, dc_offset=0):
        """
        Ramp up the transmitter.
        :param dc_offset: An unsigned integer obtained during calibration to
        remove the DcOffset from the modulation waveform.
        """
        import py2c_interface.py2c_python_wrapper as ppw

        reg_timers = ppw.RegulatoryTimers()
        reg_timers.nominal = 0
        reg_timers.extended = 0
        reg_timers.regulatory = 0
        reg_timers.off = 0
        reg_timers.off_same_channel = 0

        self._ex10_ops.tx_ramp_down()
        self.wait_op_completion()
        self._ex10_ops.tx_ramp_up(dc_offset, reg_timers)
        self.wait_op_completion()

    def radio_power_control(self, enable):
        """
        Enable or disable the Radio Power supplies (LDO) within the Ex10.
        """
        bool_return = self._ex10_ops.radio_power_control(enable)
        assert bool_return == True
        self.wait_op_completion()

    def set_tx_fine_gain(self, tx_scalar):
        # pylint: disable=locally-disabled, missing-function-docstring
        self._ex10_ops.set_tx_fine_gain(tx_scalar)
        self.wait_op_completion()

    def set_coarse_gain(self, tx_atten):
        # pylint: disable=locally-disabled, missing-function-docstring
        self._ex10_ops.set_tx_coarse_gain(tx_atten)
        self.wait_op_completion()

    def stop_transmitting(self):
        """
        Stop transmitting, ramp down power
        :return:
        """
        self._ex10_reader.stop_transmitting()

    def cw_test(self,
                antenna,
                rf_mode,
                tx_power_dbm,
                freq_mhz=None,
                remain_on=False,
                timeout_s=2):
        """
        Ramp up power and transmit CW using the py2c layer and C SDK.
        See ex10_reader.py inventory() for parameter documentation.
        If freq_mhz is None then the frequency is chosen from the region's
        hopping table.
        This expert interface calls cw_off(), and waits for op completion.
        and for _cw_is_on flag to be reset before proceeding.
        Note that cw_off() in C SDK does not disable PA.
        """
        self._ex10_ops.cw_off()
        self.wait_op_completion()
        self._ex10_helpers.discard_packets(False, True, False)
        start_time = datetime.now()
        while (self._ex10_ops.get_cw_is_on()):
            # Do nothing until cw flag is reset or timeout
            if (datetime.now() - start_time).total_seconds() > timeout_s:
                print('Timeout waiting on _cw_is_on: {} > {} seconds. '
                      'The state of _cw_is_on is {}'.format(
                          datetime.now() - start_time,
                          timeout_s,
                          self._ex10_ops.get_cw_is_on()))
            time.sleep(0.2)
        time.sleep(0.2)
        self.wait_op_completion()
        self._ex10_helpers.discard_packets(False, True, False)
        ret_status = self._ex10_reader.cw_test(antenna,
                                               rf_mode,
                                               tx_power_dbm,
                                               int(freq_mhz),
                                               remain_on)
        self.check_for_errors(ret_status)

    def measure_adc(self, adc_select):
        """
        Measures sum of power detector ADCs for either 'lo' or 'rx'.
        """
        adc_count = c_uint16(0)
        ret_status = self._ex10_ops.measure_aux_adc(adc_select, 1, pointer(adc_count))
        self.check_for_errors(ret_status)
        return struct.unpack('H', adc_count)[0]

    def get_last_op_run(self):
        status = self._ex10_ops.read_ops_status()
        return status.op_id

    def wait_op_completion(self):
        ret_status = self._ex10_ops.wait_op_completion()
        self.check_for_errors(ret_status)
