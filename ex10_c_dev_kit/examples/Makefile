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
#############################################################################

# Build the C SDK examples.
#
# This Makefile is typically called by the Makefile in the root of this
# project. The following settings are inhereted from that Makefile:
# Q CC AR OBJDUMP CPPFLAGS CFLAGS LDFLAGS BOARD_TARGET EX10_CSDK_ROOT

Q              ?= $(if $(VERBOSE),,@)
EX10_CSDK_ROOT ?= ..
BOARD_TARGET   ?= e710_ref_design
OBJDUMP        ?= objdump

BUILD_DIR      ?= $(EX10_CSDK_ROOT)/build/$(BOARD_TARGET)
BIN_DIR        := $(BUILD_DIR)/examples
OBJ_DIR        := $(BUILD_DIR)/obj

LDFLAGS        ?= --coverage -lpthread -lgpiod
LDFLAGS        += -lm
LDFLAGS        += -L $(BUILD_DIR)
LDFLAGS        += -Wl,--start-group -lhost -lboard -Wl,--end-group

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
OBJS := $(addprefix $(OBJ_DIR)/,$(OBJS))
DEPS := $(patsubst %.o,%.d,$(OBJS))
BINS := $(patsubst %.c,%.bin,$(SRCS))
BINS := $(addprefix $(BIN_DIR)/,$(BINS))

.PHONY: all
all: $(BIN_DIR) $(OBJ_DIR) $(BINS)

$(BINS): | $(BIN_DIR)
$(OBJS): | $(OBJ_DIR)

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)
	@rm -fv $(DEPS) $(OBJS)

$(BIN_DIR):
	@mkdir -pv $@

$(OBJ_DIR):
	@mkdir -pv $@

$(OBJ_DIR)/%.o: %.c
	@echo compiling $<
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MD -c $< -o $@

$(BIN_DIR)/%.bin: $(OBJ_DIR)/%.o $(BUILD_DIR)/libboard.a $(BUILD_DIR)/libhost.a
	@echo linking $@
	$(Q)$(CC) -o $@ $< $(LDFLAGS)
	$(Q)$(OBJDUMP) -Sd $@ > $@.list
	$(Q)$(OBJDUMP) -tC $@ > $@.symbols.txt

# Target for debugging makefiles
.PHONY: vars
vars::
	@$(foreach V,$(sort $(.VARIABLES)), \
		$(if \
			$(filter-out environment% default automatic, $(origin $V)), \
				$(info $V = [$(value $V)] = $($V))))

-include $(DEPS)

