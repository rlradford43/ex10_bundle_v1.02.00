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
"""
Example code to execute a single ramp up and down
"""

from __future__ import (division, absolute_import, print_function,
                        unicode_literals)
import time
import struct

from py2c_interface.py2c_python_wrapper import *

TRANSMIT_POWER_DBM = 3000  # 30 dBm
RF_MODE = RfModes.mode_5
REGION = 'FCC'             # Regulatory region
R807_ANTENNA_PORT = 1      # Which R807 antenna port will be used
FREQUENCY_MHZ = 0          # 0 utilizes the stack defined frequency hop table.


def run_ramping(py2c, ex10_ifaces):
    # pylint: disable=missing-docstring
    ex10_reader = ex10_ifaces.reader
    ex10_ops = ex10_ifaces.ops
    ex10_protocol = ex10_ifaces.protocol
    helper = ex10_ifaces.helpers

    # @todo JIRA PI-22454 [YK FW] EX10 should assert Halted interrupt
    # again when Access commands are done
    ex10_protocol.set_event_fifo_threshold(0)

    print('Starting ramp test')
    try:
        lo_power_return = c_uint16(0)
        ret_status = ex10_ops.measure_aux_adc(
            AuxAdcResultsAdcResult.AdcResultPowerLoSum, 1, pointer(lo_power_return))
        assert ret_status.error_occurred == False
        lo_power = struct.unpack('H', lo_power_return)[0]
        assert lo_power < 200

        # Ramp up and output CW
        ret_status = ex10_ops.wait_op_completion();
        assert ret_status.error_occurred == False
        ret_status =  ex10_reader.cw_test(R807_ANTENNA_PORT,
                                        RF_MODE,
                                        TRANSMIT_POWER_DBM,
                                        FREQUENCY_MHZ,
                                        False)
        assert ret_status.error_occurred == False

        ret_status = ex10_ops.measure_aux_adc(
            AuxAdcResultsAdcResult.AdcResultPowerLoSum, 1, lo_power_return)
        assert ret_status.error_occurred == False
        lo_power = struct.unpack('H', lo_power_return)[0]
        assert lo_power > 500

        # Setup a delay to wait for the  ramp down
        region = py2c.get_ex10_regions_table().contents.get_region(REGION.encode('ascii'))
        # Use the ms timer for the regulatory timer for the delay
        delay_us = region.contents.regulatory_timers.nominal * 1000
        # Set up the delay for the regional nominal time
        # Start the timer delay op
        ex10_ops.start_timer_op(delay_us)
        ret_status = ex10_ops.wait_op_completion();
        assert ret_status.error_occurred == False

        # Wait out the timer
        ex10_ops.wait_timer_op();
        ret_status = ex10_ops.wait_op_completion();
        assert ret_status.error_occurred == False

        # Wait for regulatory timer to cause ramp down
        # timeout after 4 seconds, lest the test fail and waits forever
        start_time = time.time()
        while True:
            if ex10_reader.packets_available():
                packet = ex10_reader.packet_peek().contents
                packet_type = packet.packet_type
                helper.print_packets(packet)
                ex10_reader.packet_remove()

                if packet_type == EventPacketType.TxRampDown:
                    break

            elapsed_time = time.time() - start_time
            if elapsed_time > 4:
                raise RuntimeError('timeout')
    finally:
        # Turn off radio
        ex10_reader.stop_transmitting()
        print('Ending ramp test')

def run_ramping_example():
    # pylint: disable=missing-docstring
    try:
        # Init the python to C layer
        py2c = Ex10Py2CWrapper()
        ex10_ifaces = py2c.ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, REGION.encode('ascii'))
        
        # Run a test of ramp up through the shim
        run_ramping(py2c, ex10_ifaces)

    finally:
        py2c.ex10_typical_board_teardown()


if __name__ == "__main__":
    run_ramping_example()
