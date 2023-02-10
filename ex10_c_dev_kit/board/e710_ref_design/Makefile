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
#############################################################################

# Builds a static library for interfacing with the this specific board design.
#
# This Makefile is typically called by the Makefile in the root of this
# project. The following settings are inhereted from that Makefile:
# Q CC AR OBJDUMP CPPFLAGS CFLAGS LDFLAGS EX10_CSDK_ROOT
# Note: Although BOARD_TARGET is defined in the top level Makefile,
# it would be self-referential to use it here.

Q              ?= $(if $(VERBOSE),,@)
EX10_CSDK_ROOT ?= ../..

BUILD_DIR      ?= $(EX10_CSDK_ROOT)/build/e710_ref_design
OBJ_DIR        := $(BUILD_DIR)/obj
BOARD_LIB      := libboard.a

SRCS := board_spec.c
SRCS += calibration.c
SRCS += rssi_compensation_lut.c
SRCS += calibration_v4.c
SRCS += calibration_v5.c
SRCS += driver_list.c
SRCS += ex10_gpio.c
SRCS += fifo_buffer_pool.c
SRCS += region.c
SRCS += spi_driver.c
SRCS += uart_driver.c
SRCS += uart_helpers.c
ifeq ($(SIM), 1)
	SRCS += gpio_driver_sim.c
	SRCS += time_helpers_sim.c
else
	SRCS += gpio_driver.c
	SRCS += time_helpers.c
endif

OBJS := $(patsubst %.c,%.o,$(SRCS))
OBJS := $(addprefix $(OBJ_DIR)/,$(OBJS))

all: $(BUILD_DIR)/$(BOARD_LIB)

clean:
	$(Q)rm -fv $(BUILD_DIR)/$(BOARD_LIB)
	$(Q)rm -fv $(OBJS) $(OBJS:.o=.d)

# Make sure build output directoy exists
$(BUILD_DIR):
	$(Q)mkdir -pv $@

# Make sure obj directoy exists
$(OBJ_DIR):
	$(Q)mkdir -pv $@

# Build each .o file from each .c file
$(OBJ_DIR)/%.o: $(notdir %.c) | $(OBJ_DIR)
	@echo compiling $<
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MD -c $< -o $@

# Build a static library for a specific board
$(BUILD_DIR)/$(BOARD_LIB): $(OBJS) | $(BUILD_DIR) $(OBJ_DIR)
	@echo updating archive $@
	$(Q)$(AR) -rcs $@ $?

-include $(OBJS:.o=.d)

# Target for debugging makefiles
.PHONY: vars
vars::
	@$(foreach V,$(sort $(.VARIABLES)), \
		$(if \
			$(filter-out environment% default automatic, $(origin $V)), \
				$(info $V = [$(value $V)] = $($V))))

