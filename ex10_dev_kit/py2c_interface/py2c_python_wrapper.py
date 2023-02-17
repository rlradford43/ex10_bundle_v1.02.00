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
import os
import resource as resource
import ctypes
from ctypes import *
from enum import IntEnum
from py2c_interface.py2c_python_auto_enums import *
from py2c_interface.py2c_python_auto_fifo import *
from py2c_interface.py2c_python_auto_regs import *
from py2c_interface.py2c_python_auto_boot_regs import *
from py2c_interface.py2c_python_auto_gen2_commands import *
from py2c_interface.py2c_python_byte_span import *
from py2c_interface.py2c_ex10_expert import *
from py2c_interface.py2c_python_cal import *
from py2c_interface.py2c_python_auto_reg_instances import *


c_size_t = ctypes.c_uint32
TCXO_FREQ_KHZ = 24000
VERSION_STRING_SIZE = 120
REMAIN_REASON_STRING_MAX_SIZE = 25

BOOTLOADER_SPI_CLOCK_HZ = 1000000
DEFAULT_SPI_CLOCK_HZ = 4000000


class Ex10Py2CWrapper(object):
    """
    Takes care of importing the c lib, exchanging python data with
    ctypes, and passing data back and forth.
    """

    def __init__(self, so_path=None):
        """
        Instantiate the py2c object
        """
        if not so_path:
            so_path = os.path.dirname(__file__) + "/lib_py2c.so"
        py2c_so = ctypes.cdll.LoadLibrary(so_path)

        self.py2c_so_path = so_path
        self.py2c_so = py2c_so

        # Set up return types for c defined functions we use in python
        py2c_so.ex10_typical_board_setup.argtypes = c_uint32, c_char_p
        py2c_so.ex10_typical_board_setup.restype = Ex10Interfaces
        py2c_so.ex10_bootloader_board_setup.argtypes = (c_uint32,)
        py2c_so.ex10_bootloader_board_setup.restype = ctypes.POINTER(Ex10Protocol)
        py2c_so.get_ex10_board_driver_list.restype = ctypes.POINTER(Ex10DriverList)
        py2c_so.get_ex10_protocol.restype = ctypes.POINTER(Ex10Protocol)
        py2c_so.get_ex10_ops.restype = ctypes.POINTER(Ex10Ops)
        py2c_so.get_ex10_reader.restype = ctypes.POINTER(Ex10Reader)
        py2c_so.get_ex10_gen2_tx_command_manager.restype = ctypes.POINTER(Ex10Gen2TxCommandManager)
        py2c_so.get_ex10_time_helpers.restype = ctypes.POINTER(Ex10TimeHelpers)
        py2c_so.get_ex10_helpers.restype = ctypes.POINTER(Ex10Helpers)
        py2c_so.get_ex10_commands.restype = ctypes.POINTER(Ex10Commands)
        py2c_so.get_ex10_gen2_commands.restype = ctypes.POINTER(Ex10Gen2Commands)
        py2c_so.get_ex10_event_parser.restype = ctypes.POINTER(Ex10EventParser)
        py2c_so.get_ex10_version.restype = ctypes.POINTER(Ex10Version)
        py2c_so.get_ex10_sjc.restype = ctypes.POINTER(Ex10SjcAccessor)
        py2c_so.get_ex10_event_fifo_buffer_pool.restype = ctypes.POINTER(FifoBufferPool)
        py2c_so.get_ex10_fifo_buffer_list.restype = ctypes.POINTER(FifoBufferList)
        py2c_so.get_ex10_region.restype = ctypes.POINTER(Ex10Region)
        py2c_so.get_ex10_regions_table.restype = ctypes.POINTER(Ex10RegionsTable)
        py2c_so.get_ex10_board_spec.restype = ctypes.POINTER(Ex10BoardSpec)
        py2c_so.get_ex10_gpio_helpers.restype = ctypes.POINTER(Ex10GpioHelpers)
        py2c_so.get_ex10_aggregate_op_builder.restype = ctypes.POINTER(Ex10AggregateOpBuilder)

        py2c_so.get_ex10_calibration.argtypes = (POINTER(Ex10Protocol),)
        py2c_so.get_ex10_calibration.restype = POINTER(Ex10Calibration)
        py2c_so.get_ex10_cal_v4.restype = POINTER(Ex10CalibrationV4)
        py2c_so.get_ex10_cal_v5.restype = POINTER(Ex10CalibrationV5)

    def __getattr__(self, name):
        try:
            ret_val = getattr(self.py2c_so, name)
            if name == 'get_ex10_protocol':
                ret_val = GetEx10ProtocolIntercept(ret_val)
            elif name == 'get_ex10_ops':
                ret_val = GetEx10OpsIntercept(ret_val)
            elif name == 'get_ex10_reader':
                ret_val = GetEx10ReaderIntercept(ret_val)
            elif name == 'ex10_typical_board_setup':
                ret_val = GetEx10InterfacesIntercept(ret_val)    
            elif name == 'ex10_bootloader_board_setup':
                ret_val = GetBootloaderBoardIntercept(ret_val)
            elif name == 'get_ex10_sjc':
                ret_val = GetEx10SjcAccessorIntercept(ret_val)
            elif name == 'get_ex10_helpers':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_commands':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_gen2_commands':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_event_parser':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_version':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_aggregate_op_builder':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_region':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_board_spec':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_gpio_helpers':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_event_fifo_buffer_pool':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_fifo_buffer_list':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_board_driver_list':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_cal_v4':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_cal_v5':
                ret_val = GetGenericIntercept(ret_val)
            elif name == 'get_ex10_gen2_tx_command_manager':
                ret_val = GetGenericIntercept(ret_val)
            return ret_val
        except:
            assert(sys.exc_info()[0])


class Ex10InterfaceIntercept():
    protocol = None
    ops = None
    reader = None
    helpers = None
    gen2_commands = None
    event_parser = None
    version = None


class GetEx10InterfacesIntercept():
    def __init__(self, get_interfaces_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_interfaces_function = get_interfaces_function

    def __call__(self, frequency, region):
        # Return the contents of the call
        ex10_interfaces = self.get_interfaces_function(frequency, region)
        interfaces_intercept = Ex10InterfaceIntercept()
        interfaces_intercept.protocol = Ex10ProtocolIntercept(ex10_interfaces.protocol)
        interfaces_intercept.ops = Ex10OpsIntercept(ex10_interfaces.ops)
        interfaces_intercept.reader = Ex10ReaderIntercept(ex10_interfaces.reader)
        interfaces_intercept.helpers = ex10_interfaces.helpers.contents
        interfaces_intercept.gen2_commands = ex10_interfaces.gen2_commands.contents
        interfaces_intercept.event_parser = ex10_interfaces.event_parser.contents
        interfaces_intercept.version = ex10_interfaces.version.contents
        return interfaces_intercept


class GetEx10SjcAccessorIntercept():
    def __init__(self, get_sjc_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_sjc_function = get_sjc_function

    def __call__(self):
        # The normal call function
        sjc = self.get_sjc_function()
        return Ex10SjcAccessorIntercept(sjc)


class Ex10SjcAccessorIntercept():
    def __init__(self, sjc_layer):
        """
        Intercepts calls to the reader layer
        """
        self.sjc = sjc_layer.contents

    def __getattr__(self, name):
        attr = getattr(self.sjc, name)
        # Ensure this is a callable function
        if hasattr(attr, '__call__'):
            # Intercept the C function to make 'pythonic'
            def newfunc(*args, **kwargs):
                if name == 'init':
                    # User passes in Ex10ProtocolIntercept, but we want ex10_protocol
                    result = attr(pointer(args[0].ex10_protocol))
                else:
                    result = attr(*args, **kwargs)
                return result
            return newfunc
        else:
            return attr


class GetBootloaderBoardIntercept():
    def __init__(self, get_setup_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_setup_function = get_setup_function

    def __call__(self, frequency):
        # The normal call function
        ex10_protocol = self.get_setup_function(frequency)
        return Ex10ProtocolIntercept(ex10_protocol)


class GetGenericIntercept():
    def __init__(self, generic_function_pointer):
        """
        Fakes being a function to intercept the return from the c lib.
        Purely calls the contents to dereference the function pointer.
        """
        self.generic_function_pointer = generic_function_pointer

    def __call__(self):
        # Return the contents of the call
        return self.generic_function_pointer().contents


class GetEx10ReaderIntercept():
    def __init__(self, get_reader_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_reader_function = get_reader_function

    def __call__(self):
        # The normal call function
        ex10_reader = self.get_reader_function()
        return Ex10ReaderIntercept(ex10_reader)


class Ex10ReaderIntercept():
    def __init__(self, reader_layer):
        """
        Intercepts calls to the reader layer
        """
        self.ex10_reader = reader_layer.contents

    def __getattr__(self, name):
        attr = getattr(self.ex10_reader, name)
        # Ensure this is a callable function
        if hasattr(attr, '__call__'):
            # Intercept the C function to make 'pythonic'
            def newfunc(*args, **kwargs):
                if name == 'init':
                    # User passes in Ex10OpsIntercept, but we want ex10_ops
                    result = attr(pointer(args[0].ex10_ops), args[1])
                else:
                    result = attr(*args, **kwargs)
                return result
            return newfunc
        else:
            return attr


class GetEx10OpsIntercept():
    def __init__(self, get_ops_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_ops_function = get_ops_function

    def __call__(self):
        # The normal call function
        ex10_ops = self.get_ops_function()
        return Ex10OpsIntercept(ex10_ops)


class Ex10OpsIntercept():
    def __init__(self, ops_layer):
        """
        Intercepts calls to the ops layer
        """
        self.ex10_ops = ops_layer.contents

    def __getattr__(self, name):
        attr = getattr(self.ex10_ops, name)
        # Ensure this is a callable function
        if hasattr(attr, '__call__'):
            # Intercept the C function to make 'pythonic'
            def newfunc(*args, **kwargs):
                if name == 'init':
                    # User passes in Ex10ProtocolIntercept, but we want ex10_protocol
                    result = attr(pointer(args[0].ex10_protocol))
                else:
                    result = attr(*args, **kwargs)
                return result
            return newfunc
        else:
            return attr


class GetEx10ProtocolIntercept():
    def __init__(self, get_protocol_function):
        """
        Fakes being a function to intercept the return from the c lib
        """
        self.get_protocol_function = get_protocol_function

    def __call__(self):
        # The normal call function
        ex10_protocol = self.get_protocol_function()
        return Ex10ProtocolIntercept(ex10_protocol)


class Ex10ProtocolIntercept():
    def __init__(self, protocol_layer):
        """
        Intercepts calls to the protocol layer
        """
        self.ex10_protocol = protocol_layer.contents
        self.reg_list = [getattr(RegInstances, attr) for attr in dir(RegInstances) if not callable(getattr(RegInstances, attr)) and not attr.startswith("__")]

    def _get_reg_from_str(self, reg_str):
        return [reg for reg in self.reg_list if reg.name == reg_str][0]
 
    def _get_return_struct_from_reg(self, reg_to_use):
        # Name passed in plus 'Fields' is the structure to read data into
        # EX: User wants reg 'OpsStatusFields', so we read the reg into a OpsStatusFields() structure
        read_struct_name = reg_to_use.name
        # Deal with special naming cases of return structs versus register naming
        if 'SjcResult' in read_struct_name:
            read_struct_name = read_struct_name[:-1]
        read_struct_name = read_struct_name + 'Fields'
        read_object = globals()[read_struct_name]()
        return read_object

    def _class_contains_bitfield(self, class_obj):
        num_args_to_unpack = len(class_obj._fields_[0])
        contains_bit_pack = True if num_args_to_unpack == 3 else False
        return contains_bit_pack

    def __getattr__(self, name):
        attr = getattr(self.ex10_protocol, name)
        # Ensure this is a callable function
        if hasattr(attr, '__call__'):
            # Intercept the C function to make 'pythonic'
            def newfunc(*args, **kwargs):
                if name == 'write' or name == 'write_index':
                    # User passes in (reg string name, buffer to write)
                    reg_to_use = self._get_reg_from_str(args[0])
                    if len(args) == 3: # Means this is write_index
                        result = attr(reg_to_use, ctypes.cast(pointer(args[1]), c_void_p), args[2])
                    else:
                        result = attr(reg_to_use, ctypes.cast(pointer(args[1]), c_void_p))
                elif name == 'read' or name == 'read_index':
                    # User passes in the reg string name and wants a buffer back
                    reg_to_use = self._get_reg_from_str(args[0])
                    read_object = self._get_return_struct_from_reg(reg_to_use)
                    contains_bit_pack = self._class_contains_bitfield(read_object)

                    # Find out if return type is a pointer - if so, read into a buffer
                    is_pointer = False
                    if not contains_bit_pack:
                        for field_name, field_type in read_object._fields_:
                            is_pointer = isinstance(getattr(read_object, field_name), POINTER(c_uint8))
                    if is_pointer:
                        read_object = (c_uint8 * reg_to_use.length)()

                    # Decide whether to read with read or read_index
                    if len(args) == 3:
                        attr(reg_to_use, ctypes.cast(pointer(read_object), c_void_p), args[2])
                    else:
                        attr(reg_to_use, ctypes.cast(pointer(read_object), c_void_p))
                    # Return a bytearray if the class was a buffer
                    result = bytearray(read_object) if is_pointer else read_object
                else:
                    result = attr(*args, **kwargs)
                return result
            return newfunc
        else:
            return attr


class Ex10ListNode(Structure):
    """ Forward declare referencing structs prior to their autogen """
    pass


# IPJ_autogen | generate_c2python_hand_picked {
# Required enums from C
class AuxAdcControlChannelEnableBits(IntEnum):
    ChannelEnableBitsNone = 0
    ChannelEnableBitsPowerLo0 = 1
    ChannelEnableBitsPowerLo1 = 2
    ChannelEnableBitsPowerLo2 = 4
    ChannelEnableBitsPowerLo3 = 8
    ChannelEnableBitsPowerRx0 = 16
    ChannelEnableBitsPowerRx1 = 32
    ChannelEnableBitsPowerRx2 = 64
    ChannelEnableBitsPowerRx3 = 128
    ChannelEnableBitsTestMux0 = 256
    ChannelEnableBitsTestMux1 = 512
    ChannelEnableBitsTestMux2 = 1024
    ChannelEnableBitsTestMux3 = 2048
    ChannelEnableBitsTemperature = 4096
    ChannelEnableBitsPowerLoSum = 8192
    ChannelEnableBitsPowerRxSum = 16384


class FifoSelection(IntEnum):
    EventFifo = 0


class StopReason(IntEnum):
    SRNone = 0
    SRHost = 1
    SRMaxNumberOfRounds = 2
    SRMaxNumberOfTags = 3
    SRMaxDuration = 4
    SROpError = 5
    SRSdkTimeoutError = 6
    SRDeviceCommandError = 7


class ProductSku(IntEnum):
    SkuUnknown = 0x0
    SkuE310 = 0x0310
    SkuE510 = 0x0510
    SkuE710 = 0x0710


class RfFilter(IntEnum):
    LOWER_BAND = 1
    UPPER_BAND = 2


class RfModes(IntEnum):
    mode_1 = 1
    mode_3 = 3
    mode_5 = 5
    mode_7 = 7
    mode_11 = 11
    mode_12 = 12
    mode_13 = 13
    mode_15 = 15
    mode_323 = 323
    mode_222 = 222
    mode_241 = 241
    mode_244 = 244
    mode_302 = 302
    mode_223 = 223
    mode_285 = 285
    mode_344 = 344
    mode_103 = 103
    mode_345 = 345
    mode_120 = 120


class BasebandFilterType(IntEnum):
    BasebandFilterHighpass = 0
    BasebandFilterBandpass = 1


class AggregateOpInstructionType(IntEnum):
    InstructionTypeReserved = 0x00
    InstructionTypeWrite = 0x02
    InstructionTypeReset = 0x08
    InstructionTypeInsertFifoEvent = 0x0E
    InstructionTypeRunOp = 0x30
    InstructionTypeGoToIndex = 0x31
    InstructionTypeExitInstruction = 0x32


class Gen2TxCommandManagerErrorType(IntEnum):
    Gen2CommandManagerErrorNone = 0
    Gen2CommandManagerErrorBufferLength = 1
    Gen2CommandManagerErrorNumCommands = 2
    Gen2CommandManagerErrorCommandEncode = 3
    Gen2CommandManagerErrorCommandDecode = 4
    Gen2CommandManagerErrorTxnControls = 5
    Gen2CommandManagerErrorCommandEnableMismatch = 6
    Gen2CommandManagerErrorEnabledEmptyCommand = 7


# Required Structs from c code
class Ex10GpioConfig(Structure):
    _fields_ = [
        ('antenna', c_uint8),
        ('baseband_filter', c_uint32),
        ('dio_0', c_bool),
        ('dio_1', c_bool),
        ('dio_6', c_bool),
        ('dio_8', c_bool),
        ('dio_13', c_bool),
        ('pa_bias_enable', c_bool),
        ('power_range', c_uint32),
        ('rf_enable', c_bool),
        ('rf_filter', c_uint32),
    ]


class EventFifoPacket(Structure):
    _fields_ = [
        ('packet_type', c_uint32),
        ('us_counter', c_uint32),
        ('static_data', POINTER(PacketData)),
        ('static_data_length', c_size_t),
        ('dynamic_data', POINTER(c_uint8)),
        ('dynamic_data_length', c_size_t),
        ('is_valid', c_bool),
    ]


class Ex10ListNode(Structure):
    _fields_ = [
        ('data', c_void_p),
        ('next', POINTER(Ex10ListNode)),
        ('prev', POINTER(Ex10ListNode)),
    ]


class Ex10LinkedList(Structure):
    _fields_ = [
        ('sentinal', Ex10ListNode),
    ]


class FifoBufferNode(Structure):
    _fields_ = [
        ('fifo_data', ConstByteSpan),
        ('raw_buffer', ByteSpan),
        ('list_node', Ex10ListNode),
    ]


class FifoBufferPool(Structure):
    _fields_ = [
        ('fifo_buffer_nodes', POINTER(FifoBufferNode)),
        ('fifo_buffers', POINTER(ByteSpan)),
        ('buffer_count', c_size_t),
    ]


class FifoBufferList(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(FifoBufferNode), POINTER(ByteSpan), c_size_t)),
        ('free_list_put', CFUNCTYPE(c_bool, POINTER(FifoBufferNode))),
        ('free_list_get', CFUNCTYPE(POINTER(FifoBufferNode))),
        ('free_list_size', CFUNCTYPE(c_size_t)),
    ]


class OpCompletionStatus(Structure):
    _fields_ = [
        ('error_occurred', c_bool),
        ('ops_status', OpsStatusFields),
        ('command_error', c_uint32),
        ('timeout_error', c_uint32),
        ('aggregate_buffer_overflow', c_bool),
    ]


class TagReadFields(Structure):
    _fields_ = [
        ('pc', POINTER(c_uint16)),
        ('epc', POINTER(c_uint8)),
        ('epc_length', c_size_t),
        ('crc', POINTER(c_uint16)),
        ('tid', POINTER(c_uint8)),
        ('tid_length', c_size_t),
    ]


class StopConditions(Structure):
    _fields_ = [
        ('max_number_of_rounds', c_uint32),
        ('max_number_of_tags', c_uint32),
        ('max_duration_us', c_uint32),
    ]


class ContinuousInventorySummary(Structure):
    _fields_ = [
        ('duration_us', c_uint32),
        ('number_of_inventory_rounds', c_uint32),
        ('number_of_tags', c_uint32),
        ('reason', c_uint8),
        ('last_op_id', c_uint8),
        ('last_op_error', c_uint8),
        ('packet_rfu_1', c_uint8),
    ]


class GpioControlFields(Structure):
    _fields_ = [
        ('output_enable', c_uint32),
        ('output_level', c_uint32),
    ]


class PowerConfigs(Structure):
    _fields_ = [
        ('tx_atten', c_uint8),
        ('tx_scalar', c_int16),
        ('dc_offset', c_uint32),
        ('adc_target', c_uint32),
        ('loop_stop_threshold', c_uint16),
        ('op_error_threshold', c_uint16),
        ('loop_gain_divisor', c_uint16),
        ('max_iterations', c_uint32),
        ('power_detector_adc', c_uint32),
    ]


class RegulatoryTimers(Structure):
    _fields_ = [
        ('nominal', c_uint32),
        ('extended', c_uint32),
        ('regulatory', c_uint32),
        ('off', c_uint32),
        ('off_same_channel', c_uint32),
    ]


class CwConfig(Structure):
    _fields_ = [
        ('gpio', GpioControlFields),
        ('rf_mode', c_uint16),
        ('power', PowerConfigs),
        ('synth', RfSynthesizerControlFields),
        ('timer', RegulatoryTimers),
    ]


class TagReadData(Structure):
    _fields_ = [
        ('pc', POINTER(c_uint16)),
        ('epc', POINTER(c_uint8)),
        ('epc_length', c_size_t),
        ('crc', POINTER(c_uint16)),
        ('tid', POINTER(c_uint8)),
        ('tid_length', c_size_t),
    ]


class InfoFromPackets(Structure):
    _fields_ = [
        ('gen2_transactions', c_size_t),
        ('total_singulations', c_size_t),
        ('total_tid_count', c_size_t),
        ('access_tag', TagReadData),
    ]


class InventoryHelperParams(Structure):
    _fields_ = [
        ('antenna', c_uint8),
        ('rf_mode', c_uint16),
        ('tx_power_dbm', c_uint16),
        ('inventory_config', POINTER(InventoryRoundControlFields)),
        ('inventory_config_2', POINTER(InventoryRoundControl_2Fields)),
        ('send_selects', c_bool),
        ('remain_on', c_bool),
        ('dual_target', c_bool),
        ('inventory_duration_ms', c_uint32),
        ('packet_info', POINTER(InfoFromPackets)),
        ('verbose', c_bool),
        ('enforce_gen2_response', c_bool),
    ]


class ContInventoryHelperParams(Structure):
    _fields_ = [
        ('inventory_params', POINTER(InventoryHelperParams)),
        ('stop_conditions', POINTER(StopConditions)),
        ('summary_packet', POINTER(ContinuousInventorySummary)),
    ]


class Ex10GpioInterface(Structure):
    _fields_ = [
        ('initialize', CFUNCTYPE(None, c_bool, c_bool, c_bool)),
        ('cleanup', CFUNCTYPE(None)),
        ('set_board_power', CFUNCTYPE(None, c_bool)),
        ('set_ex10_enable', CFUNCTYPE(None, c_bool)),
        ('register_irq_callback', CFUNCTYPE(None, CFUNCTYPE(None))),
        ('deregister_irq_callback', CFUNCTYPE(None)),
        ('irq_enable', CFUNCTYPE(None, c_bool)),
        ('assert_reset_n', CFUNCTYPE(None)),
        ('deassert_reset_n', CFUNCTYPE(None)),
        ('assert_ready_n', CFUNCTYPE(None)),
        ('release_ready_n', CFUNCTYPE(None)),
        ('busy_wait_ready_n', CFUNCTYPE(None, c_uint32)),
        ('ready_n_pin_get', CFUNCTYPE(c_int)),
        ('reset_device', CFUNCTYPE(None)),
    ]


class HostInterface(Structure):
    _fields_ = [
        ('open', CFUNCTYPE(c_int32, c_uint32)),
        ('close', CFUNCTYPE(None)),
        ('read', CFUNCTYPE(c_int32, c_void_p, c_size_t)),
        ('write', CFUNCTYPE(c_int32, c_void_p, c_size_t)),
    ]


class UartInterface(Structure):
    _fields_ = [
        ('open', CFUNCTYPE(c_int32, c_uint32)),
        ('close', CFUNCTYPE(None)),
        ('read', CFUNCTYPE(c_int32, c_void_p, c_size_t)),
        ('write', CFUNCTYPE(c_int32, c_void_p, c_size_t)),
    ]


class Ex10DriverList(Structure):
    _fields_ = [
        ('gpio_if', Ex10GpioInterface),
        ('host_if', HostInterface),
        ('uart_if', UartInterface),
    ]


class Ex10Protocol(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10DriverList))),
        ('deinit', CFUNCTYPE(None)),
        ('register_fifo_data_callback', CFUNCTYPE(None, CFUNCTYPE(None, POINTER(FifoBufferNode)))),
        ('register_interrupt_callback', CFUNCTYPE(None, InterruptMaskFields, CFUNCTYPE(c_bool, InterruptStatusFields))),
        ('unregister_fifo_data_callback', CFUNCTYPE(None)),
        ('unregister_interrupt_callback', CFUNCTYPE(None)),
        ('read', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p)),
        ('test_read', CFUNCTYPE(None, c_uint32, c_uint16, c_void_p)),
        ('read_index', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p, c_uint8)),
        ('write', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p)),
        ('write_index', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p, c_uint8)),
        ('read_partial', CFUNCTYPE(None, c_uint16, c_uint16, c_void_p)),
        ('write_partial', CFUNCTYPE(None, c_uint16, c_uint16, c_void_p)),
        ('write_multiple', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p, c_size_t)),
        ('read_multiple', CFUNCTYPE(None, POINTER(RegisterInfo), c_void_p, c_size_t)),
        ('start_op', CFUNCTYPE(None, c_uint32)),
        ('stop_op', CFUNCTYPE(None)),
        ('wait_op_completion', CFUNCTYPE(OpCompletionStatus)),
        ('wait_op_completion_with_timeout', CFUNCTYPE(OpCompletionStatus, c_uint32)),
        ('reset', CFUNCTYPE(None, c_uint32)),
        ('set_event_fifo_threshold', CFUNCTYPE(None, c_size_t)),
        ('insert_fifo_event', CFUNCTYPE(None, c_bool, POINTER(EventFifoPacket))),
        ('get_running_location', CFUNCTYPE(c_uint32)),
        ('write_info_page', CFUNCTYPE(None, c_uint32, c_void_p, c_size_t, c_uint32)),
        ('write_calibration_page', CFUNCTYPE(None, POINTER(c_uint8), c_size_t)),
        ('erase_info_page', CFUNCTYPE(None, c_uint32, c_uint32)),
        ('erase_calibration_page', CFUNCTYPE(None)),
        ('write_stored_settings_page', CFUNCTYPE(None, POINTER(c_uint8), c_size_t)),
        ('upload_image', CFUNCTYPE(None, c_uint8, ConstByteSpan)),
        ('upload_start', CFUNCTYPE(None, c_uint8, c_size_t, ConstByteSpan)),
        ('upload_continue', CFUNCTYPE(None, ConstByteSpan)),
        ('upload_complete', CFUNCTYPE(None)),
        ('revalidate_image', CFUNCTYPE(ImageValidityFields)),
        ('test_transfer', CFUNCTYPE(c_uint32, POINTER(ConstByteSpan), POINTER(ByteSpan), c_bool)),
    ]


class CdacRange(Structure):
    _fields_ = [
        ('center', c_int8),
        ('limit', c_uint8),
        ('step_size', c_uint8),
    ]


class SjcResult(Structure):
    _fields_ = [
        ('residue', c_int32),
        ('cdac', c_int8),
        ('cdac_limited', c_bool),
    ]


class SjcResultPair(Structure):
    _fields_ = [
        ('i', SjcResult),
        ('q', SjcResult),
    ]


class Ex10SjcAccessor(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10Protocol))),
        ('set_sjc_control', CFUNCTYPE(None, c_uint8, c_uint8, c_bool, c_bool, c_uint8)),
        ('set_analog_rx_config', CFUNCTYPE(None)),
        ('set_settling_time', CFUNCTYPE(None, c_uint16, c_uint16)),
        ('set_cdac_range', CFUNCTYPE(None, CdacRange, CdacRange)),
        ('set_residue_threshold', CFUNCTYPE(None, c_uint16)),
        ('set_cdac_to_find_solution', CFUNCTYPE(None)),
        ('get_sjc_results', CFUNCTYPE(SjcResultPair)),
    ]


class SynthesizerParams(Structure):
    _fields_ = [
        ('freq_khz', c_uint32),
        ('r_divider_index', c_uint8),
        ('n_divider', c_uint16),
    ]


class AggregateRunOpFormat(Structure):
    _fields_ = [
        ('op_to_run', c_uint8),
    ]


class AggregateGoToIndexFormat(Structure):
    _fields_ = [
        ('jump_index', c_uint16),
        ('repeat_counter', c_uint8),
    ]


class Gen2TxCommandManagerError(Structure):
    _fields_ = [
        ('error_occurred', c_bool),
        ('error', c_uint32),
        ('current_index', c_size_t),
    ]


class TxCommandInfo(Structure):
    _fields_ = [
        ('decoded_buffer[40]', c_uint8),
        ('encoded_buffer[40]', c_uint8),
        ('encoded_command', BitSpan),
        ('decoded_command', Gen2CommandSpec),
        ('valid', c_bool),
        ('transaction_id', c_uint8),
    ]


class ContinuousInventoryState(Structure):
    _fields_ = [
        ('state', c_uint32),
        ('done_reason', c_uint32),
        ('initial_inventory_config', InventoryRoundControlFields),
        ('previous_q', c_uint8),
        ('min_q_count', c_uint8),
        ('queries_since_valid_epc_count', c_uint8),
        ('stop_reason', c_uint32),
        ('round_count', c_size_t),
        ('tag_count', c_size_t),
        ('target', c_uint8),
    ]


class Ex10WriteFormat(Structure):
    _pack_ = 1
    _fields_ = [
        ('address', c_uint16),
        ('length', c_uint16),
        ('data', POINTER(c_uint8)),
    ]


class Ex10InsertFifoEventFormat(Structure):
    _pack_ = 1
    _fields_ = [
        ('trigger_irq', c_uint8),
        ('packet', POINTER(c_uint8)),
    ]


class Ex10ResetFormat(Structure):
    _pack_ = 1
    _fields_ = [
        ('destination', c_uint8),
    ]


class AggregateInstructionData(Union):
    _fields_ = [
        ('write_format', Ex10WriteFormat),
        ('reset_format', Ex10ResetFormat),
        ('insert_fifo_event_format', Ex10InsertFifoEventFormat),
        ('run_op_format', AggregateRunOpFormat),
        ('go_to_index_format', AggregateGoToIndexFormat),
    ]


class AggregateOpInstruction(Structure):
    _fields_ = [
        ('instruction_type', c_uint32),
        ('instruction_data', POINTER(AggregateInstructionData)),
    ]


class Ex10AggregateOpBuilder(Structure):
    _fields_ = [
        ('append_instruction', CFUNCTYPE(c_bool, AggregateOpInstruction, POINTER(ByteSpan))),
        ('clear_buffer', CFUNCTYPE(None)),
        ('set_buffer', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('get_instruction_from_index', CFUNCTYPE(c_size_t, c_size_t, POINTER(ByteSpan), POINTER(AggregateOpInstruction))),
        ('print_buffer', CFUNCTYPE(None, POINTER(ByteSpan))),
        ('append_reg_write', CFUNCTYPE(c_bool, POINTER(RegisterInfo), POINTER(ConstByteSpan), POINTER(ByteSpan))),
        ('append_reset', CFUNCTYPE(c_bool, c_uint8, POINTER(ByteSpan))),
        ('append_insert_fifo_event', CFUNCTYPE(c_bool, c_bool, POINTER(EventFifoPacket), POINTER(ByteSpan))),
        ('append_op_run', CFUNCTYPE(c_bool, c_uint32, POINTER(ByteSpan))),
        ('append_go_to_instruction', CFUNCTYPE(c_bool, c_uint16, c_uint8, POINTER(ByteSpan))),
        ('append_exit_instruction', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_set_rf_mode', CFUNCTYPE(c_bool, c_uint16, POINTER(ByteSpan))),
        ('append_set_gpio', CFUNCTYPE(c_bool, c_uint32, c_uint32, POINTER(ByteSpan))),
        ('append_lock_synthesizer', CFUNCTYPE(c_bool, c_uint8, c_uint16, POINTER(ByteSpan))),
        ('append_run_sjc', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_set_tx_coarse_gain', CFUNCTYPE(c_bool, c_uint8, POINTER(ByteSpan))),
        ('append_set_tx_fine_gain', CFUNCTYPE(c_bool, c_int16, POINTER(ByteSpan))),
        ('append_set_regulatory_timers', CFUNCTYPE(c_bool, POINTER(RegulatoryTimers), POINTER(ByteSpan))),
        ('append_tx_ramp_up', CFUNCTYPE(c_bool, c_uint32, POINTER(ByteSpan))),
        ('append_power_control', CFUNCTYPE(c_bool, POINTER(PowerConfigs), POINTER(ByteSpan))),
        ('append_start_log_test', CFUNCTYPE(c_bool, c_uint32, c_uint32, POINTER(ByteSpan))),
        ('append_set_atest_mux', CFUNCTYPE(c_bool, c_uint32, c_uint32, c_uint32, c_uint32, POINTER(ByteSpan))),
        ('append_set_aux_dac', CFUNCTYPE(c_bool, c_uint16, c_uint16, POINTER(c_uint16), POINTER(ByteSpan))),
        ('append_tx_ramp_down', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_radio_power_control', CFUNCTYPE(c_bool, c_bool, POINTER(ByteSpan))),
        ('append_set_analog_rx_config', CFUNCTYPE(c_bool, POINTER(RxGainControlFields), POINTER(ByteSpan))),
        ('append_measure_rssi', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_start_timer_op', CFUNCTYPE(c_bool, c_uint32, POINTER(ByteSpan))),
        ('append_wait_timer_op', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_start_event_fifo_test', CFUNCTYPE(c_bool, c_uint32, c_uint8, POINTER(ByteSpan))),
        ('append_enable_sdd_logs', CFUNCTYPE(c_bool, LogEnablesFields, c_uint8, POINTER(ByteSpan))),
        ('append_start_inventory_round', CFUNCTYPE(c_bool, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), POINTER(ByteSpan))),
        ('append_start_prbs', CFUNCTYPE(c_bool, POINTER(ByteSpan))),
        ('append_start_ber_test', CFUNCTYPE(c_bool, c_uint16, c_uint16, c_bool, POINTER(ByteSpan))),
        ('append_ramp_transmit_power', CFUNCTYPE(c_bool, POINTER(PowerConfigs), POINTER(RegulatoryTimers), POINTER(ByteSpan))),
    ]


class Ex10Helpers(Structure):
    _fields_ = [
        ('check_gen2_error', CFUNCTYPE(None, POINTER(Gen2Reply))),
        ('dump_gen2_access_registers', CFUNCTYPE(None)),
        ('print_aggregate_op_errors', CFUNCTYPE(None, AggregateOpSummary)),
        ('discard_packets', CFUNCTYPE(None, c_bool, c_bool, c_bool)),
        ('inventory_halted', CFUNCTYPE(c_bool)),
        ('check_ops_status_errors', CFUNCTYPE(c_bool, OpCompletionStatus)),
        ('examine_packets', CFUNCTYPE(None, POINTER(EventFifoPacket), POINTER(InfoFromPackets))),
        ('print_packets', CFUNCTYPE(c_size_t, POINTER(EventFifoPacket))),
        ('deep_copy_packet', CFUNCTYPE(c_bool, POINTER(EventFifoPacket), POINTER(PacketData), ByteSpan, POINTER(EventFifoPacket))),
        ('simple_inventory', CFUNCTYPE(c_int, POINTER(InventoryHelperParams))),
        ('continuous_inventory', CFUNCTYPE(c_int, POINTER(ContInventoryHelperParams))),
        ('copy_tag_read_data', CFUNCTYPE(None, POINTER(TagReadData), POINTER(TagReadFields))),
        ('get_remain_reason_string', CFUNCTYPE(c_char_p, c_uint32)),
        ('swap_bytes', CFUNCTYPE(c_uint16, c_uint16)),
        ('read_rssi_value_from_op', CFUNCTYPE(c_uint16)),
        ('send_single_halted_command', CFUNCTYPE(Gen2TxCommandManagerError, POINTER(Gen2CommandSpec))),
    ]


class Ex10Ops(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(OpCompletionStatus, POINTER(Ex10Protocol))),
        ('release', CFUNCTYPE(None)),
        ('register_fifo_data_callback', CFUNCTYPE(None, CFUNCTYPE(None, POINTER(FifoBufferNode)))),
        ('register_interrupt_callback', CFUNCTYPE(None, InterruptMaskFields, CFUNCTYPE(c_bool, InterruptStatusFields))),
        ('unregister_fifo_data_callback', CFUNCTYPE(None)),
        ('unregister_interrupt_callback', CFUNCTYPE(None)),
        ('ex10_protocol', CFUNCTYPE(POINTER(Ex10Protocol))),
        ('get_cw_is_on', CFUNCTYPE(c_bool)),
        ('read_ops_status', CFUNCTYPE(OpsStatusFields)),
        ('start_log_test', CFUNCTYPE(None, c_uint32, c_uint32)),
        ('set_atest_mux', CFUNCTYPE(None, c_uint32, c_uint32, c_uint32, c_uint32)),
        ('measure_aux_adc', CFUNCTYPE(OpCompletionStatus, c_uint32, c_uint16, POINTER(c_uint16))),
        ('set_aux_dac', CFUNCTYPE(None, c_uint16, c_uint16, POINTER(c_uint16))),
        ('set_rf_mode', CFUNCTYPE(None, c_uint16)),
        ('tx_ramp_up', CFUNCTYPE(None, c_int32, POINTER(RegulatoryTimers))),
        ('tx_ramp_down', CFUNCTYPE(None)),
        ('set_tx_coarse_gain', CFUNCTYPE(None, c_uint8)),
        ('set_tx_fine_gain', CFUNCTYPE(None, c_int16)),
        ('radio_power_control', CFUNCTYPE(c_bool, c_bool)),
        ('get_current_analog_rx_config', CFUNCTYPE(POINTER(RxGainControlFields))),
        ('set_analog_rx_config', CFUNCTYPE(None, POINTER(RxGainControlFields))),
        ('measure_rssi', CFUNCTYPE(None)),
        ('start_timer_op', CFUNCTYPE(None, c_uint32)),
        ('wait_timer_op', CFUNCTYPE(None)),
        ('lock_synthesizer', CFUNCTYPE(None, c_uint8, c_uint16)),
        ('start_event_fifo_test', CFUNCTYPE(None, c_uint32, c_uint8)),
        ('insert_fifo_event', CFUNCTYPE(None, c_bool, POINTER(EventFifoPacket))),
        ('enable_sdd_logs', CFUNCTYPE(None, LogEnablesFields, c_uint8)),
        ('send_gen2_halted_sequence', CFUNCTYPE(None)),
        ('continue_from_halted', CFUNCTYPE(None, c_bool)),
        ('run_sjc', CFUNCTYPE(OpCompletionStatus)),
        ('get_sjc', CFUNCTYPE(POINTER(Ex10SjcAccessor))),
        ('wait_op_completion', CFUNCTYPE(OpCompletionStatus)),
        ('run_aggregate_op', CFUNCTYPE(None)),
        ('wait_op_completion_with_timeout', CFUNCTYPE(OpCompletionStatus, c_uint32)),
        ('stop_op', CFUNCTYPE(None)),
        ('get_gpio', CFUNCTYPE(GpioControlFields)),
        ('set_gpio', CFUNCTYPE(None, c_uint32, c_uint32)),
        ('start_inventory_round', CFUNCTYPE(None, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields))),
        ('start_prbs', CFUNCTYPE(None)),
        ('start_etsi_burst', CFUNCTYPE(OpCompletionStatus, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), POINTER(GpioControlFields), c_uint16, POINTER(PowerConfigs), POINTER(RfSynthesizerControlFields), POINTER(RegulatoryTimers), c_uint16, c_uint16)),
        ('start_ber_test', CFUNCTYPE(None, c_uint16, c_uint16, c_bool)),
        ('send_select', CFUNCTYPE(None)),
        ('write_calibration_page', CFUNCTYPE(None, POINTER(c_uint8), c_size_t)),
        ('cw_off', CFUNCTYPE(None)),
        ('cw_on', CFUNCTYPE(OpCompletionStatus, POINTER(GpioControlFields), c_uint16, POINTER(PowerConfigs), POINTER(RfSynthesizerControlFields), POINTER(RegulatoryTimers))),
        ('ramp_transmit_power', CFUNCTYPE(OpCompletionStatus, POINTER(PowerConfigs), POINTER(RegulatoryTimers))),
        ('inventory', CFUNCTYPE(OpCompletionStatus, POINTER(GpioControlFields), c_uint16, POINTER(PowerConfigs), POINTER(RfSynthesizerControlFields), POINTER(RegulatoryTimers), POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), c_bool)),
        ('get_device_time', CFUNCTYPE(c_uint32)),
    ]


class Ex10Reader(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10Ops), c_char_p)),
        ('deinit', CFUNCTYPE(None)),
        ('continuous_inventory', CFUNCTYPE(OpCompletionStatus, c_uint8, c_uint16, c_uint16, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), c_bool, POINTER(StopConditions), c_bool, c_uint32, c_bool)),
        ('inventory', CFUNCTYPE(OpCompletionStatus, c_uint8, c_uint16, c_uint16, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), c_bool, c_uint32, c_bool)),
        ('fifo_data_handler', CFUNCTYPE(None, POINTER(FifoBufferNode))),
        ('interrupt_handler', CFUNCTYPE(c_bool, InterruptStatusFields)),
        ('packet_peek', CFUNCTYPE(POINTER(EventFifoPacket))),
        ('packet_remove', CFUNCTYPE(None)),
        ('packets_available', CFUNCTYPE(c_bool)),
        ('continue_from_halted', CFUNCTYPE(None, c_bool)),
        ('send_access_commands', CFUNCTYPE(None)),
        ('cw_test', CFUNCTYPE(OpCompletionStatus, c_uint8, c_uint16, c_uint16, c_uint32, c_bool)),
        ('write_cal_info_page', CFUNCTYPE(None, POINTER(c_uint8), c_size_t)),
        ('prbs_test', CFUNCTYPE(OpCompletionStatus, c_uint8, c_uint16, c_uint16, c_uint32, c_bool)),
        ('etsi_burst_test', CFUNCTYPE(OpCompletionStatus, POINTER(InventoryRoundControlFields), POINTER(InventoryRoundControl_2Fields), c_uint8, c_uint16, c_uint16, c_uint16, c_uint16, c_uint32)),
        ('insert_fifo_event', CFUNCTYPE(None, c_bool, POINTER(EventFifoPacket))),
        ('enable_sdd_logs', CFUNCTYPE(None, LogEnablesFields, c_uint8)),
        ('stop_transmitting', CFUNCTYPE(OpCompletionStatus)),
        ('build_cw_configs', CFUNCTYPE(OpCompletionStatus, c_uint8, c_uint16, c_uint16, c_uint32, c_bool, POINTER(CwConfig))),
        ('get_current_compensated_rssi', CFUNCTYPE(c_int16, c_uint16)),
        ('get_current_rssi_log2', CFUNCTYPE(c_uint16, c_int16)),
        ('get_continuous_inventory_state', CFUNCTYPE(POINTER(ContinuousInventoryState))),
    ]


class Ex10Gen2TxCommandManager(Structure):
    _fields_ = [
        ('clear_local_sequence', CFUNCTYPE(None)),
        ('clear_command_in_local_sequence', CFUNCTYPE(Gen2TxCommandManagerError, c_uint8)),
        ('clear_sequence', CFUNCTYPE(None)),
        ('init', CFUNCTYPE(None)),
        ('write_sequence', CFUNCTYPE(Gen2TxCommandManagerError)),
        ('write_select_enables', CFUNCTYPE(Gen2TxCommandManagerError, c_void_p, c_uint8)),
        ('write_halted_enables', CFUNCTYPE(Gen2TxCommandManagerError, c_void_p, c_uint8)),
        ('write_auto_access_enables', CFUNCTYPE(Gen2TxCommandManagerError, c_void_p, c_uint8)),
        ('append_encoded_command', CFUNCTYPE(Gen2TxCommandManagerError, POINTER(BitSpan), c_uint8)),
        ('encode_and_append_command', CFUNCTYPE(Gen2TxCommandManagerError, POINTER(Gen2CommandSpec), c_uint8)),
        ('read_device_to_local_sequence', CFUNCTYPE(None)),
        ('print_local_sequence', CFUNCTYPE(None)),
        ('get_local_sequence', CFUNCTYPE(POINTER(TxCommandInfo))),
    ]


class Ex10TimeHelpers(Structure):
    _fields_ = [
        ('time_now', CFUNCTYPE(c_uint32)),
        ('time_elapsed', CFUNCTYPE(c_uint32, c_uint32)),
        ('busy_wait_ms', CFUNCTYPE(None, c_uint32)),
        ('wait_ms', CFUNCTYPE(None, c_uint32)),
    ]


class Ex10Gen2Commands(Structure):
    _fields_ = [
        ('encode_gen2_command', CFUNCTYPE(c_bool, POINTER(Gen2CommandSpec), POINTER(BitSpan))),
        ('decode_gen2_command', CFUNCTYPE(c_bool, POINTER(Gen2CommandSpec), POINTER(BitSpan))),
        ('decode_reply', CFUNCTYPE(c_bool, c_uint32, POINTER(EventFifoPacket), POINTER(Gen2Reply))),
        ('get_gen2_tx_control_config', CFUNCTYPE(c_bool, POINTER(Gen2CommandSpec), POINTER(Gen2TxnControlsFields))),
    ]


class Ex10EventParser(Structure):
    _fields_ = [
        ('get_tag_read_fields', CFUNCTYPE(TagReadFields, c_void_p, c_size_t, c_uint32, c_uint8)),
        ('get_static_payload_length', CFUNCTYPE(c_size_t, c_uint32)),
        ('get_packet_type_valid', CFUNCTYPE(c_bool, c_uint32)),
        ('parse_event_packet', CFUNCTYPE(EventFifoPacket, POINTER(ConstByteSpan))),
        ('make_packet_header', CFUNCTYPE(PacketHeader, c_uint32)),
    ]


class Ex10Version(Structure):
    _fields_ = [
        ('get_bootloader_info', CFUNCTYPE(c_size_t, c_char_p, c_size_t)),
        ('get_application_info', CFUNCTYPE(c_size_t, c_char_p, c_size_t, POINTER(ImageValidityFields), POINTER(RemainReasonFields))),
        ('get_sku', CFUNCTYPE(c_uint32)),
        ('get_device_info', CFUNCTYPE(c_char_p)),
    ]


class RegulatoryChannels(Structure):
    _fields_ = [
        ('start_freq', c_uint32),
        ('spacing', c_uint32),
        ('count', c_size_t),
        ('usable', POINTER(c_uint16)),
        ('usable_count', c_size_t),
        ('random_hop', c_bool),
    ]


class Region(Structure):
    _fields_ = [
        ('name', c_char_p),
        ('region_enum', c_uint8),
        ('regulatory_timers', RegulatoryTimers),
        ('regulatory_channels', RegulatoryChannels),
        ('pll_divider', c_uint32),
        ('rf_filter', c_uint32),
        ('max_power_mbm', c_int32),
    ]


class Ex10Region(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, c_char_p, c_uint32)),
        ('get_name', CFUNCTYPE(c_char_p)),
        ('update_current_channel', CFUNCTYPE(None)),
        ('get_channel_table_size', CFUNCTYPE(c_size_t)),
        ('get_current_channel_khz', CFUNCTYPE(c_uint32)),
        ('get_regulatory_timers', CFUNCTYPE(None, POINTER(RegulatoryTimers))),
        ('get_synthesizer_params', CFUNCTYPE(None, c_uint32, POINTER(SynthesizerParams))),
        ('get_synthesizer_frequency_khz', CFUNCTYPE(c_uint32, c_uint8, c_uint16)),
        ('get_rf_filter', CFUNCTYPE(c_uint32)),
        ('get_pll_r_divider', CFUNCTYPE(c_uint32)),
        ('calculate_n_divider', CFUNCTYPE(c_uint16, c_uint32, c_uint8)),
        ('calculate_r_divider_index', CFUNCTYPE(c_uint8, c_uint8)),
    ]


class Ex10RegionsTable(Structure):
    _fields_ = [
        ('get_region', CFUNCTYPE(POINTER(Region), c_char_p)),
        ('build_channel_table', CFUNCTYPE(c_size_t, POINTER(RegulatoryChannels), POINTER(c_uint16))),
    ]


class Ex10Interfaces(Structure):
    _fields_ = [
        ('protocol', POINTER(Ex10Protocol)),
        ('ops', POINTER(Ex10Ops)),
        ('reader', POINTER(Ex10Reader)),
        ('helpers', POINTER(Ex10Helpers)),
        ('gen2_commands', POINTER(Ex10Gen2Commands)),
        ('event_parser', POINTER(Ex10EventParser)),
        ('version', POINTER(Ex10Version)),
    ]


class Ex10Commands(Structure):
    _fields_ = [
        ('read', CFUNCTYPE(c_uint32, POINTER(RegisterInfo), POINTER(ByteSpan), c_size_t, c_uint32)),
        ('test_read', CFUNCTYPE(c_uint32, c_uint32, c_uint16, c_void_p)),
        ('write', CFUNCTYPE(None, POINTER(ConstByteSpan), c_size_t, c_uint32)),
        ('read_fifo', CFUNCTYPE(c_uint32, c_uint32, POINTER(ByteSpan))),
        ('write_info_page', CFUNCTYPE(None, c_uint8, POINTER(ConstByteSpan), c_uint16)),
        ('start_upload', CFUNCTYPE(None, c_uint8, POINTER(ConstByteSpan))),
        ('continue_upload', CFUNCTYPE(None, POINTER(ConstByteSpan))),
        ('complete_upload', CFUNCTYPE(None)),
        ('revalidate_main_image', CFUNCTYPE(None)),
        ('reset', CFUNCTYPE(None, c_uint32)),
        ('test_transfer', CFUNCTYPE(c_uint32, POINTER(ConstByteSpan), POINTER(ByteSpan), c_bool)),
        ('create_fifo_event', CFUNCTYPE(None, POINTER(EventFifoPacket), POINTER(c_uint8), c_size_t, c_size_t)),
        ('insert_fifo_event', CFUNCTYPE(None, c_bool, POINTER(EventFifoPacket))),
    ]


class Ex10BoardSpec(Structure):
    _fields_ = [
        ('get_default_gpio_output_levels', CFUNCTYPE(c_uint32)),
        ('get_gpio_output_levels', CFUNCTYPE(c_uint32, c_uint8, c_uint16, c_uint32, c_uint32)),
        ('get_gpio_output_enables', CFUNCTYPE(c_uint32)),
        ('get_default_rx_analog_config', CFUNCTYPE(POINTER(RxGainControlFields))),
        ('get_sjc_residue_threshold', CFUNCTYPE(c_uint16)),
    ]


class Ex10GpioHelpers(Structure):
    _fields_ = [
        ('get_levels', CFUNCTYPE(c_uint32, POINTER(Ex10GpioConfig))),
        ('get_config', CFUNCTYPE(None, c_uint32, POINTER(Ex10GpioConfig))),
        ('get_output_enables', CFUNCTYPE(c_uint32)),
    ]


class Ex10CalibrationV4(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10Protocol))),
        ('get_params', CFUNCTYPE(POINTER(Ex10CalibrationParamsV4))),
    ]


class Ex10CalibrationV5(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10Protocol))),
        ('get_params', CFUNCTYPE(POINTER(Ex10CalibrationParamsV5))),
    ]


class Ex10Calibration(Structure):
    _fields_ = [
        ('init', CFUNCTYPE(None, POINTER(Ex10Protocol))),
        ('deinit', CFUNCTYPE(None)),
        ('power_to_adc', CFUNCTYPE(c_uint16, c_uint16, c_uint32, c_uint16, c_bool, c_uint32, POINTER(c_uint32))),
        ('get_power_control_params', CFUNCTYPE(PowerConfigs, c_int16, c_uint32, c_uint16, c_bool, c_uint32)),
        ('get_compensated_rssi', CFUNCTYPE(c_int16, c_uint16, c_uint16, POINTER(RxGainControlFields), c_uint8, c_uint32, c_uint16)),
        ('get_rssi_log2', CFUNCTYPE(c_uint16, c_int16, c_uint16, POINTER(RxGainControlFields), c_uint8, c_uint32, c_uint16)),
        ('get_cal_version', CFUNCTYPE(c_uint8)),
    ]
# IPJ_autogen }
