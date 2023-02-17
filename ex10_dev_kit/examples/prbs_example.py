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
Example code to use the PRBS functionality
"""

from __future__ import (division, absolute_import, print_function,
                        unicode_literals)
from datetime import datetime

from py2c_interface.py2c_python_wrapper import *


PRBS_TIME_ON = 6                    # Duration of PRBS operation (seconds)
TRANSMIT_POWER_DBM = 3000           # 30 dBm
RF_MODE = RfModes.mode_5
REGION = 'FCC'                      # Regulatory region
R807_ANTENNA_PORT = 1               # Which R807 antenna port will be used
FREQUENCY_USED = 0                  # Set to 0 to use the jump table
REMAIN_ON = True


def run_prbs(ex10_ifaces):
    # pylint: disable=missing-docstring
    ex10_reader = ex10_ifaces.reader
    helper = ex10_ifaces.helpers

    print('Starting prbs test')

    try:
        transmitting = False
        start_time = datetime.now()
        time_delta = datetime.now() - start_time

        # Run PRBS
        while time_delta.total_seconds() <  PRBS_TIME_ON:
            if not transmitting:
                transmitting = True
                # Run PRBS with no regulatory timers to allow easier
                # viewing on a spectrum analyzer
                op_status = ex10_reader.prbs_test(
                    R807_ANTENNA_PORT,
                    RF_MODE,
                    TRANSMIT_POWER_DBM,
                    FREQUENCY_USED,
                    REMAIN_ON)
                assert op_status.error_occurred == False

            while ex10_reader.packets_available():
                packet = ex10_reader.packet_peek().contents
                helper.print_packets(packet)
                
                if packet.packet_type == EventPacketType.TxRampDown:
                    transmitting = False

                ex10_reader.packet_remove()

            time_delta = datetime.now() - start_time
    
    finally:
        ex10_reader.stop_transmitting()
        print('Ending prbs test')

def run_prbs_example():
    # pylint: disable=missing-docstring
    try:
        # Init the python to C layer
        py2c = Ex10Py2CWrapper()
        ex10_ifaces = py2c.ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, REGION.encode('ascii'))
        run_prbs(ex10_ifaces)
    finally:
        py2c.ex10_typical_board_teardown()


if __name__ == "__main__":
    run_prbs_example()
