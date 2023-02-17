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
 The inventory example below demonstrates inventory operation managed by the
 top level application.
 This example calls simple_inventory() helper function, which repeatedly calls
 inventory() function from the Ex10Reader layer. Meaning the application layer
 is responsible for starting each inventory round, allowing closer control over
 Ex10 inventory operation.
 The inventory example below is optimized for approximately 256 tags in FOV.
 To adjust dynamic Q algorithm for other tag populations, the following
 parameters should be updated:
 - INITIAL_Q
 - MAX_Q
 - MIN_Q
 - MIN_Q_CYCLES
 - MAX_QUERIES_SINCE_VALID_EPC
 For additional details regarding these parameters please refer
 to 'InventoryRoundControl' and 'InventoryRoundControl_2' registers
 descriptions in the Ex10 Reader Chip SDK documentation.
"""

from __future__ import (division, absolute_import, print_function,
                        unicode_literals)
import argparse

from py2c_interface.py2c_python_wrapper import *


# The configuration of the inventory example is set using the definitions below
INVENTORY_DURATION_S = 10           # Duration of inventory operation (seconds)

TRANSMIT_POWER_DBM = 3000           # 30 dBm
RF_MODE = RfModes.mode_11
R807_ANTENNA_PORT = 1               # Which R807 antenna port will be used

INITIAL_Q = 8                       # Q field in the Query command
                                    # (starting Q for dynamic Q algorithm)
MAX_Q = 15                          # Max Q value for dynamic Q
MIN_Q = 0                           # Min Q value for dynamic Q
MIN_Q_CYCLES = 1                    # Number of times we stay on the minimum Q
                                    # before ending the inventory round
MAX_QUERIES_SINCE_VALID_EPC = 16    # Number of queries seen since last good
                                    # EPC before ending the inventory round
SELECT_ALL = 0                      # SELECT field in the Query command
SESSION = 0                         # Session field in the Query command
TARGET = 0                          # Target field in the Query command
DUAL_TARGET = True                  # If True, the Target of subsequent Query
                                    # commands will flip
HALT_ON_ALL_TAGS = False            # If True, the Ex10 will halt on each
                                    # inventoried tag
TAG_FOCUS_ENABLE = False            # Tells a tag to be silent after inventoried
                                    # once.
FAST_ID_ENABLE = False              # Tells a tag to backscatter the TID during
                                    # inventory


def run_inventory_example(min_read_rate=0):
    # pylint: disable=missing-docstring
    print('Starting inventory example')

    try:
        # Init the python to C layer
        py2c = Ex10Py2CWrapper()
        ex10_ifaces = py2c.ex10_typical_board_setup(DEFAULT_SPI_CLOCK_HZ, 'FCC'.encode('ascii'))
        
        total_singulations = run_inventory(ex10_ifaces, INVENTORY_DURATION_S,
                                           INITIAL_Q, RF_MODE, min_read_rate)
        if total_singulations <= 0:
            raise RuntimeError('No tags found in inventory')

        print('Ending inventory example')
    finally:
        py2c.ex10_typical_board_teardown()


def run_inventory(ex10_ifaces, duration_s, initial_q, rf_mode, min_read_rate):
    """ Run inventory for the specified amount of time """
    ex10_reader = ex10_ifaces.reader
    helper = ex10_ifaces.helpers

    # Put together configurations for the inventory round
    inventory_config = InventoryRoundControlFields()
    inventory_config.initial_q = initial_q
    inventory_config.max_q = MAX_Q
    inventory_config.min_q = MIN_Q
    inventory_config.num_min_q_cycles = MIN_Q_CYCLES
    inventory_config.fixed_q_mode = False
    inventory_config.q_increase_use_query = False
    inventory_config.q_decrease_use_query = False
    inventory_config.session = SESSION
    inventory_config.select = SELECT_ALL
    inventory_config.target = 0
    inventory_config.halt_on_all_tags = HALT_ON_ALL_TAGS
    inventory_config.fast_id_enable = FAST_ID_ENABLE
    inventory_config.tag_focus_enable = TAG_FOCUS_ENABLE

    inventory_config_2 = InventoryRoundControl_2Fields()
    inventory_config_2.max_queries_since_valid_epc = MAX_QUERIES_SINCE_VALID_EPC

    packet_info = InfoFromPackets(0, 0, 0, TagReadData(None, None, 0, None, None, 0))

    ihp = InventoryHelperParams()
    ihp.antenna               = R807_ANTENNA_PORT
    ihp.rf_mode               = rf_mode
    ihp.tx_power_dbm          = TRANSMIT_POWER_DBM
    ihp.inventory_config      = pointer(inventory_config)
    ihp.inventory_config_2    = pointer(inventory_config_2)
    ihp.send_selects          = False
    ihp.remain_on             = False
    ihp.dual_target           = DUAL_TARGET
    ihp.inventory_duration_ms = duration_s * 1000
    ihp.packet_info           = pointer(packet_info)
    ihp.enforce_gen2_response = False
    ihp.verbose               = True

    assert 0 == helper.simple_inventory(ihp)

    read_rate = packet_info.total_singulations/duration_s

    print("Read rate = {} - tags: {} / seconds {:.3f} (Mode {})".format(
          round(read_rate),
          packet_info.total_singulations,
          duration_s,
          RF_MODE))

    if min_read_rate > 0 and read_rate < min_read_rate:
        print('Read rate of {} below minimal threshold of {}'.
              format(round(read_rate), min_read_rate))
        assert read_rate >= min_read_rate

    return packet_info.total_singulations


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Inventory example')

    default_read_rate = 0

    parser.add_argument('-r', '--min_read_rate', type=int,
                        default=default_read_rate,
                        help='Minimal read rate threshold'
                        ' , default = {}'.format(default_read_rate))

    args = parser.parse_args()

    run_inventory_example(args.min_read_rate)
