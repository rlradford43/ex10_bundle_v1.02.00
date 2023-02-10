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

# Builds the Ex10 API host static library.
#
# This Makefile is typically called by the Makefile in the root of this
# project. The following settings are inhereted from that Makefile:
# Q CC AR OBJDUMP CPPFLAGS CFLAGS LDFLAGS BOARD_TARGET EX10_CSDK_ROOT BUILD_DIR

Q              ?= $(if $(VERBOSE),,@)
EX10_CSDK_ROOT ?= ..
BOARD_TARGET   ?= e710_ref_design

BUILD_DIR      ?= $(EX10_CSDK_ROOT)/build/$(BOARD_TARGET)
OBJ_DIR        := $(BUILD_DIR)/obj
HOST_LIB       := libhost.a

SRCS := $(wildcard **/*.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
OBJS := $(addprefix $(OBJ_DIR)/,$(OBJS))

all: $(BUILD_DIR)/$(HOST_LIB)

clean:
	$(Q)rm -fv $(BUILD_DIR)/$(HOST_LIB)
	$(Q)rm -fv $(OBJS) $(OBJS:.o=.d)

# Build each .o file from each .c file
$(OBJ_DIR)/%.o: $(notdir %.c)
	@echo compiling $<
	$(Q)mkdir -pv $(dir $@)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MD -c $< -o $@

# Build the host library
$(BUILD_DIR)/$(HOST_LIB): $(OBJS) | $(BUILD_DIR) $(OBJ_DIR)
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

