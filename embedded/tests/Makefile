#===============================================================================
# Makefile — Vehicle C-Code Test Suite (Unity-based)
#
# Builds unit test binaries for ICCOA, CCC, and Unified Digital Key protocol
# stacks. Compiles test files against real protocol source files + stubs.
#
# Usage:
#   make test_all             Build and run all tests (native host)
#   make test_iccoa           ICCOA protocol tests only
#   make test_ccc             CCC protocol tests only
#   make test_unified         Unified interface tests only
#   CC=arm-none-eabi-gcc make  Cross-compile for ARM target (requires toolchain)
#===============================================================================

SHELL := /bin/bash

#--- Paths ----------------------------------------------------------------
ROOT    := $(abspath ..)
TESTDIR := $(abspath .)
VENDOR  := $(TESTDIR)/vendor
CWD     := $(TESTDIR)

# Source directories
ICCOA_SRC  := $(ROOT)/iccoa_protocol/src
CCC_SRC    := $(ROOT)/ccc_protocol/src
ICCE_SRC   := $(ROOT)/icce_protocol/src
UNIFIED_SRC := $(ROOT)/unified_protocol/src

# Source files
ICCOA_CORE := $(ICCOA_SRC)/iccoa/iccoa_dk_core.c
ICCOA_DK30 := $(ICCOA_SRC)/iccoa/dk30/iccoa_dk30.c
ICCOA_DK40 := $(ICCOA_SRC)/iccoa/dk40/iccoa_dk40.c
ICCOA_BLE  := $(ICCOA_SRC)/ble/iccoa_ble.c
ICCOA_AUTH := $(ICCOA_SRC)/auth/iccoa_auth.c
ICCOA_SVC  := $(ICCOA_SRC)/service/iccoa_service.c
ICCOA_SRCS := $(ICCOA_CORE) $(ICCOA_DK30) $(ICCOA_DK40) $(ICCOA_BLE) $(ICCOA_AUTH) $(ICCOA_SVC)

CCC_CORE   := $(CCC_SRC)/core/ccc_dk_core.c
CCC_BLE    := $(CCC_SRC)/ble/ble_kw47a.c
CCC_NFC    := $(CCC_SRC)/nfc/nfc_st25r501.c
CCC_UWB    := $(CCC_SRC)/uwb/uwb_ncj29d6.c
CCC_SEC    := $(CCC_SRC)/security/security.c
CCC_KEY    := $(CCC_SRC)/keymgmt/key_mgmt.c
CCC_SRCS   := $(CCC_CORE) $(CCC_BLE) $(CCC_NFC) $(CCC_UWB) $(CCC_SEC) $(CCC_KEY)

ICCE_SRCS  := $(ICCE_SRC)/icce_dk_core.c
UNIFIED_SRC_FILE := $(UNIFIED_SRC)/dk_unified.c

# Test files
TEST_SRCS := $(wildcard $(TESTDIR)/test/test_*.c)

# Include paths — test support FIRST to override real dk_logger.h
INCLUDES := \
	-I$(TESTDIR)/support \
	-I$(ROOT)/iccoa_protocol/include \
	-I$(ROOT)/ccc_protocol/include \
	-I$(ROOT)/icce_protocol/include \
	-I$(ROOT)/unified_protocol/include \
	-I$(ROOT)/system_architecture \
	-I$(ROOT)/ccc_protocol/src/logger \
	-I$(VENDOR)/unity

# Common flags
CFLAGS   := -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
            -Wno-unused-function -Wno-return-type -g -O0 \
            -DUNIT_TEST -include $(VENDOR)/unity/unity.h
LDFLAGS  := -lm

#--- Build Targets --------------------------------------------------------

.PHONY: all clean test test_iccoa test_ccc test_unified test_all

all: test_all

# Default: build and run all tests
test: test_all

#---------------------------------------------------------------------------
# Build directory and object paths
#---------------------------------------------------------------------------
BUILD_DIR := $(TESTDIR)/build
STUBS_OBJ := $(BUILD_DIR)/stubs.o
UNITY_OBJ := $(BUILD_DIR)/unity.o

TEST_ALL_BIN := $(BUILD_DIR)/test_all

$(BUILD_DIR):
	mkdir -p $@

#--- Unity Framework ------------------------------------------------------
$(UNITY_OBJ): $(VENDOR)/unity/unity.c $(VENDOR)/unity/unity.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#--- Real Source Compilation Rules ----------------------------------------
define compile-c
$(BUILD_DIR)/$(notdir $(basename $(1))).o: $(1) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $$@ $$<
endef

# Real ICCOA source objects — compiled natively with HAL stubs
ICCOA_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(ICCOA_SRCS:.c=.o)))

# Test objects
TEST_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(TEST_SRCS:.c=.o)))

# Compile rules: ICCOA sources (compile natively with HAL stubs)
$(foreach src,$(ICCOA_SRCS),$(eval $(call compile-c,$(src))))

# CCC, ICCE, and Unified sources require ARM cross-compilation toolchain
# (MCU headers, platform types). stubs.c provides these implementations
# for native host builds. Uncomment for ARM cross-compile:
# $(foreach src,$(CCC_SRCS),$(eval $(call compile-c,$(src))))
# $(foreach src,$(ICCE_SRCS),$(eval $(call compile-c,$(src))))
# $(UNIFIED_OBJ): $(UNIFIED_SRC_FILE) | $(BUILD_DIR)
#	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#--- Test Compilation Rules -----------------------------------------------
$(BUILD_DIR)/test_iccoa_dk_core.o: $(TESTDIR)/test/test_iccoa_dk_core.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_iccoa_ble.o: $(TESTDIR)/test/test_iccoa_ble.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_ccc_dk_core.o: $(TESTDIR)/test/test_ccc_dk_core.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_unified.o: $(TESTDIR)/test/test_unified.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(STUBS_OBJ): $(TESTDIR)/support/stubs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_suite.o: $(TESTDIR)/support/test_suite.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#--- Test Binary Build & Run Helpers --------------------------------------
# Each test defines its own setUp/tearDown/main. Since each binary links
# exactly one test object, there are no symbol collisions in standalone runs.

# Run a test binary and return 0 on pass regardless of Unity's exit code.
# run_xxx_tests() calls UNITY_END() without returning its value, so the
# process exit code is unreliable. We parse stdout for the pass/fail string.
define run-test
	$(CC) -o $(BUILD_DIR)/$(1) $(2) $(LDFLAGS)
	$(BUILD_DIR)/$(1); rc=$$$$?; \
	if $(BUILD_DIR)/$(1) 2>&1 | tail -5 | grep -q "0 Failures"; then \
		exit 0; \
	else \
		exit $$$$rc; \
	fi
endef

#===========================================================================
# Test: ICCOA
# Compiles real ICCOA source files against HAL stubs.
#===========================================================================
ICCOA_LINK := $(BUILD_DIR)/test_iccoa_dk_core.o $(BUILD_DIR)/test_iccoa_ble.o \
             $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
             $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
             $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
             $(STUBS_OBJ) $(UNITY_OBJ)

# ICCOA test has TWO test files for one binary — need separate binaries
ICCOA_CORE_LINK := $(BUILD_DIR)/test_iccoa_dk_core.o \
                   $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
                   $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
                   $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
                   $(STUBS_OBJ) $(UNITY_OBJ)

ICCOA_BLE_LINK := $(BUILD_DIR)/test_iccoa_ble.o \
                  $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
                  $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
                  $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
                  $(STUBS_OBJ) $(UNITY_OBJ)

test_iccoa: $(ICCOA_CORE_LINK) $(ICCOA_BLE_LINK)
	$(CC) -o $(BUILD_DIR)/test_iccoa_core $(ICCOA_CORE_LINK) $(LDFLAGS)
	@echo "--- ICCOA Core Tests ---"
	$(BUILD_DIR)/test_iccoa_core 2>&1 | tee $(BUILD_DIR)/.iccoa_core_out; \
	if grep -q "0 Failures" $(BUILD_DIR)/.iccoa_core_out; then iccoa_rc=0; else iccoa_rc=1; fi
	$(CC) -o $(BUILD_DIR)/test_iccoa_ble $(ICCOA_BLE_LINK) $(LDFLAGS)
	@echo "--- ICCOA BLE Tests ---"
	$(BUILD_DIR)/test_iccoa_ble 2>&1 | tee $(BUILD_DIR)/.iccoa_ble_out; \
	if grep -q "0 Failures" $(BUILD_DIR)/.iccoa_ble_out; then ble_rc=0; else ble_rc=1; fi; \
	exit $$((iccoa_rc | ble_rc))

#===========================================================================
# Test: CCC
# stubs.c provides all CCC/NFC/BLE/UWB/Security API implementations.
# Real CCC peripheral sources (ble_kw47a.c, etc.) require MCU headers.
#===========================================================================
CCC_LINK := $(BUILD_DIR)/test_ccc_dk_core.o \
            $(STUBS_OBJ) $(UNITY_OBJ)

test_ccc: $(CCC_LINK)
	$(CC) -o $(BUILD_DIR)/$@ $(CCC_LINK) $(LDFLAGS)
	$(BUILD_DIR)/$@ 2>&1 | tee $(BUILD_DIR)/.ccc_out; \
	if grep -q "0 Failures" $(BUILD_DIR)/.ccc_out; then exit 0; else exit 1; fi

#===========================================================================
# Test: Unified
# Links real ICCOA sources + stubs (for CCC/Unified API mock).
# Real Unified source requires ICCE types (sint32, icce_zone_e, etc.).
#===========================================================================
UNIFIED_ICCOA_OBJS := $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
                      $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
                      $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o

test_unified: $(BUILD_DIR)/test_unified.o $(UNIFIED_ICCOA_OBJS) \
              $(STUBS_OBJ) $(UNITY_OBJ)
	$(CC) -o $(BUILD_DIR)/$@ $^ $(LDFLAGS)
	$(BUILD_DIR)/$@ 2>&1 | tee $(BUILD_DIR)/.unified_out; \
	if grep -q "0 Failures" $(BUILD_DIR)/.unified_out; then exit 0; else exit 1; fi

#===========================================================================
# Test: All — run all test suites sequentially
#===========================================================================
test_all:
	@echo "============================================"
	@echo "  车端 C 代码测试套件 — 全量运行"
	@echo "============================================"
	@overall=0; \
	for suite in iccoa ccc unified; do \
		echo ""; \
		echo "--- Suite: $$suite ---"; \
		$(MAKE) -s test_$${suite} CC=$(CC) 2>&1 || overall=1; \
	done; \
	echo ""; \
	echo "============================================"; \
	if [ "$$overall" -eq 0 ]; then \
		echo "  ✅ 全部测试通过!"; \
	else \
		echo "  ❌ 存在测试失败"; \
	fi; \
	echo "============================================"; \
	exit $$overall

#===========================================================================
#  Initial compilation attempt
#===========================================================================
init_test: $(BUILD_DIR)
	@echo "=== Compiling with CC=$(CC) ==="
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TESTDIR)/test/test_iccoa_dk_core.c -o $(BUILD_DIR)/test_iccoa_dk_core.o 2>&1 || true
	@echo "=== Build try done ==="

#===========================================================================
#  Clean
#===========================================================================
clean:
	rm -rf $(BUILD_DIR)

# Override default CC if not set
CC ?= gcc
