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

# Top level C SDK makefile

# if CODE_COVERAGE is not defined, then define it as 0
CODE_COVERAGE ?= 0

$(info VERBOSE:       ${VERBOSE})
$(info CODE_COVERAGE: ${CODE_COVERAGE})

Q       := $(if $(VERBOSE),,@)    # Set VERBOSE=1 for additional build output
CC      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar
OBJDUMP := $(CROSS_COMPILE)objdump

EX10_CSDK_ROOT   := $(realpath .)
BOARD_TARGET     ?= e710_ref_design
BUILD_BASE       ?= ./build
BUILD_DIR        := $(abspath $(BUILD_BASE)/$(BOARD_TARGET))

EXAMPLES_DIR     := $(BUILD_DIR)/examples
TESTS_FUNC_DIR   := $(BUILD_DIR)/tests/tests_func
PY2C_INSTALL_DIR := ../ex10_dev_kit/py2c_interface

# Export configuration for use in submake targets
export Q CC AR OBJDUMP CFLAGS CPPFLAGS LDFLAGS BOARD_TARGET EX10_CSDK_ROOT BUILD_DIR

CFLAGS += -std=gnu11
CFLAGS += -fno-common
CFLAGS += -Wall
CFLAGS += -Wpedantic
CFLAGS += -Wextra
CFLAGS += -Wformat=2
CFLAGS += -Wswitch-default
CFLAGS += -Wswitch-enum
CFLAGS += -Wpointer-arith
CFLAGS += -Wstrict-overflow=5
CFLAGS += -Winline
CFLAGS += -Wundef
CFLAGS += -Wcast-qual
CFLAGS += -Wshadow
CFLAGS += -Wunreachable-code
CFLAGS += -Wlogical-op
CFLAGS += -Wfloat-equal
CFLAGS += -Wredundant-decls
CFLAGS += -fstrict-aliasing
CFLAGS += -fstrict-overflow
CFLAGS += -D_FORTIFY_SOURCE=2
CFLAGS += -fstack-protector-all
CFLAGS += -Wbad-function-cast
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wnested-externs
CFLAGS += -Wold-style-definition
CFLAGS += -Wno-unknown-pragmas

ifdef GPROF_ENABLE
    CFLAGS += -g
    CFLAGS += -pg
    CFLAGS += -fno-reorder-functions
endif
ifdef SIM
    CFLAGS += -fPIC
endif

CFLAGS += -I$(realpath .)
CFLAGS += -I$(realpath ./include)
CFLAGS += -I$(realpath ./board/$(BOARD_TARGET))

ifneq ($(CODE_COVERAGE), 0)
    $(info CODE_COVERAGE is set to $(CODE_COVERAGE))
    CFLAGS += -O0 -g
    CXXFLAGS += -O0 -0g
    CPPFLAGS += -coverage -fprofile-abs-path
    LDFLAGS += -coverage
    LIB_PY2C_DEST := lib_py2c-coverage.so
else ifdef DEBUG
    $(info DEBUG Is set)
    CFLAGS += -Og -g
    CXXFLAGS += -Og -g
    LIB_PY2C_DEST := lib_py2c.so
else
    CFLAGS += -O3 -ffunction-sections
    LDFLAGS += -Wl,--gc-sections
    LIB_PY2C_DEST := lib_py2c.so
endif

LDFLAGS += -lpthread

ifndef SIM
    LDFLAGS += -lgpiod
endif

# Set LTTNG=1 to enable lttng tracepoints
ifeq ($(LTTNG),1)
CPPFLAGS += -DLTTNG_ENABLE=$(LTTNG)
LDFLAGS += -llttng-ust -ldl
else
# LTTNG macros creates a compiler warning, so Werror needs to be disabled only
# when compiling LTTNG.
CFLAGS += -Werror
endif

# Build the e710_reference_design target by default
# Note that target tests is not run by default since functional tests are not
# part of the distribution bundle to customers.
.PHONY: all
all: libs examples

.PHONY: examples
examples: static_libs
	$(MAKE) -C examples

.PHONY: test
test: static_libs
	cmake -S tests/tests_func -B $(TESTS_FUNC_DIR) -DBOARD_TARGET=$(BOARD_TARGET) -DDEBUG=$(DEBUG) -DCODE_COVERAGE=$(CODE_COVERAGE) -DEX10_CSDK_ROOT=$(EX10_CSDK_ROOT)
	$(MAKE) -C $(TESTS_FUNC_DIR)

.PHONY: clean
clean:
	$(Q)rm -rf $(BUILD_BASE)
	$(Q)rm -f coverage_report_*.info test_*.xml

.PHONY: clean_test
clean_test:
	$(Q)rm -rf $(TESTS_FUNC_DIR)

.PHONY: relink_test
relink_test:
	$(Q)rm -f $(TESTS_FUNC_DIR)/*.bin
	$(MAKE) -C . test

.PHONY: clean_examples
clean_examples:
	$(Q)rm -rf $(EXAMPLES_DIR)

.PHONY: relink_examples
relink_examples:
	$(Q)rm -f $(EXAMPLES_DIR)/*.bin
	$(MAKE) -C . examples

.PHONY: clean_host
clean_host:
	$(Q)$(MAKE) -C src clean

.PHONY: clean_board
clean_board:
	$(Q)$(MAKE) -C $(BOARD_TARGET) clean

$(EXAMPLES_DIR)/%.bin: $(BUILD_DIR)/libboard.a  $(BUILD_DIR)/libhost.a
	$(MAKE) -C . examples

$(TESTS_FUNC_DIR)/%.bin:
	$(MAKE) -C . test

CFLAGS_SHARED += -fPIC -shared

SHARED_LIBS_TO_LINK = -lboard -lhost
ifndef SIM
SHARED_LIBS_TO_LINK += -lgpiod
endif

LDFLAGS_SHARED += -Wl,--whole-archive -L $(BUILD_DIR) $(SHARED_LIBS_TO_LINK) -Wl,--no-whole-archive

ifdef GPERF_ENABLE
LDFLAGS_SHARED += -lprofiler -Wl,--no-as-needed -lprofiler -Wl,--as-needed
endif

.PHONY: lib_py2c
lib_py2c: $(BUILD_DIR)/lib_py2c.so

.PHONY: libs
libs: static_libs lib_py2c

$(BUILD_DIR)/lib_py2c.so: $(BUILD_DIR)/libboard.a  $(BUILD_DIR)/libhost.a | static_libs
	@echo linking $@ from $^
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_SHARED) $(LDFLAGS) $(LDFLAGS_SHARED) -o $@

$(BUILD_DIR)/libboard.a:
	$(MAKE) -C board/$(BOARD_TARGET)

$(BUILD_DIR)/libhost.a:
	$(MAKE) -C src

.PHONY: static_libs
static_libs:
	$(MAKE) -C board/$(BOARD_TARGET)
	$(MAKE) -C src

# If you have a webserver on this machine, you can install and inspect
# the html coverage report.
.PHONY: install-coverage
install-coverage:
	sudo cp -a coverage_all/ /var/www/html/

# Typically used by humans to put the lib_py2c.so into the correct directory.
.PHONY: install-py2c
install-py2c: libs
	@cp -v build/e710_ref_design/lib_py2c.so $(PY2C_INSTALL_DIR)

# The copy-py2c rule is used by bamboo scripts to copy lib_py2c.so into the
# ex10_dev_kit/py2c_interface with a specific name based on whether coverage
# is enabled.
.PHONY: copy-py2c
copy-py2c: libs
	@cp -v build/e710_ref_design/lib_py2c.so $(PY2C_INSTALL_DIR)/$(LIB_PY2C_DEST)

.PHONY: help
help:
	@echo "Variables that affect the build:"
	@echo " BUILD_BASE        : output directory for build artifacts"
	@echo " BOARD_TARGET=rxxx : Target name matching board/ subdir"
	@echo " CODE_COVERAGE=1   : Include coverage info in binaries"
	@echo " DEBUG=1           : Enable debug symbols"
	@echo " LTTNG=1           : Enable lttng tracepoints"
	@echo " VERBOSE=1         : Show compile and link commands"
	@echo "Example: DEBUG=1 VERBOSE=1 make"

.PHONY: install-app
install-app: yk_app_yx4.yk_image
	./build/e710_ref_design/examples/app_upload_example.bin yk_app_yx4.yk_image

# Target for debugging makefiles
.PHONY: vars
vars::
	@$(foreach V,$(sort $(.VARIABLES)), \
		$(if \
			$(filter-out environment% default automatic, $(origin $V)), \
				$(info $V = [$(value $V)] = $($V))))

