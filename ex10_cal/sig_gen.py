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
This contains a wrapper for working with a signal generator.
"""

import pyvisa as visa
import time


class KeysightSigGen(object):
    def __init__(self, address=19):
        self.address = address
        self.freq = None
        self.power = None
        rm = visa.ResourceManager()
        self.KeysightSigGen = rm.open_resource(
            "GPIB0::{}::INSTR".format(address))

    def initialize(self, freq, power):
        self.freq = freq
        self.power = power
        self.KeysightSigGen.write("FREQ " + str(freq) + " MHz\n")
        time.sleep(0.01)
        self.KeysightSigGen.write("POW:AMPL " + str(power) + " dBm\n")
        time.sleep(0.01)
        self.on()

    def set_freq_mhz(self, freq):
        """
        :param freq: This value should be in MHz
        :return:
        """
        self.freq = freq
        self.KeysightSigGen.write("FREQ " + str(freq) + " MHz\n")

    def set_power_dbm(self, power):
        """
        :param power: This value should be in dBm
        :return:
        """
        self.power = power
        self.KeysightSigGen.write("POW:AMPL "+ str(power) + " dBm\n")

    def on(self):
        self.KeysightSigGen.write("OUTP:STAT ON\n")

    def off(self):
        self.KeysightSigGen.write("OUTP:STAT OFF\n")
