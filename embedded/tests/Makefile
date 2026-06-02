#===============================================================================
# Makefile — Vehicle C-Code Test Suite (Unity-based)
#
# Builds unit test binaries for ICCOA, CCC, and Unified Digital Key protocol stacks.
# Compiles test files against:
#   - Real protocol source files + stub HAL implementations
#   - Unity test framework
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

# Include paths
# Include paths: test support FIRST to override real dk_logger.h
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

.PHONY: all clean test_iccoa test_ccc test_unified test_all

all: test_all

#---------------------------------------------------------------------------
# Single monolithic test binary — simplest approach
#---------------------------------------------------------------------------
TEST_ALL_OBJ := $(TESTDIR)/build/test_all.o
STUBS_OBJ    := $(TESTDIR)/build/stubs.o

TEST_ALL_BIN := $(TESTDIR)/build/test_all

$(TESTDIR)/build:
	mkdir -p $@

# Compile test all
$(TEST_ALL_OBJ): $(TESTDIR)/test/test_iccoa_dk_core.c \
                 $(TESTDIR)/test/test_iccoa_ble.c \
                 $(TESTDIR)/test/test_ccc_dk_core.c \
                 $(TESTDIR)/test/test_unified.c \
                 | $(TESTDIR)/build
	$(CC) $(CFLAGS) $(INCLUDES) -c \
	  -DTEST_ALL_MONOLITHIC \
	  -o $@ $<

# Actually, let's just compile all tests together
TEST_MAIN := $(TESTDIR)/build/test_main.o
$(TEST_MAIN): | $(TESTDIR)/build

$(STUBS_OBJ): $(TESTDIR)/support/stubs.c | $(TESTDIR)/build
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#===============================================================================
#  Strategy: Build a single binary with all test files
#===============================================================================

BUILD_DIR := $(TESTDIR)/build
SRC_DIR   := $(ROOT)

define compile-c
$(BUILD_DIR)/$(notdir $(basename $(1))).o: $(1) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $$@ $$<
endef

# Real source objects
ICCOA_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(ICCOA_SRCS:.c=.o)))
CCC_OBJS   := $(addprefix $(BUILD_DIR)/, $(notdir $(CCC_SRCS:.c=.o)))
ICCE_OBJS  := $(addprefix $(BUILD_DIR)/, $(notdir $(ICCE_SRCS:.c=.o)))
UNIFIED_OBJ := $(BUILD_DIR)/dk_unified.o

# Test objects
TEST_OBJS := $(addprefix $(BUILD_DIR)/, $(notdir $(TEST_SRCS:.c=.o)))

# All objects
ALL_OBJS := $(TEST_OBJS) $(ICCOA_OBJS) $(CCC_OBJS) $(ICCE_OBJS) $(UNIFIED_OBJ) $(STUBS_OBJ)

# Compile rules for each source
$(foreach src,$(ICCOA_SRCS),$(eval $(call compile-c,$(src))))
$(foreach src,$(CCC_SRCS),$(eval $(call compile-c,$(src))))
$(foreach src,$(ICCE_SRCS),$(eval $(call compile-c,$(src))))

$(UNIFIED_OBJ): $(UNIFIED_SRC_FILE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Compile test files into separate objects
$(BUILD_DIR)/test_iccoa_ble.o: $(TESTDIR)/test/test_iccoa_ble.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_iccoa_dk_core.o: $(TESTDIR)/test/test_iccoa_dk_core.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_ccc_dk_core.o: $(TESTDIR)/test/test_ccc_dk_core.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_DIR)/test_unified.o: $(TESTDIR)/test/test_unified.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#===============================================================================
# Test: ICCOA
#===============================================================================
ICCOA_LINK := $(BUILD_DIR)/test_iccoa_dk_core.o $(BUILD_DIR)/test_iccoa_ble.o \
             $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
             $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
             $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
             $(BUILD_DIR)/stubs.o

test_iccoa: $(ICCOA_LINK)
	$(CC) -o $(BUILD_DIR)/$@ $(ICCOA_LINK) $(LDFLAGS)
	$(BUILD_DIR)/$@

#===============================================================================
# Test: CCC
#===============================================================================
CCC_LINK := $(BUILD_DIR)/test_ccc_dk_core.o \
            $(BUILD_DIR)/ccc_dk_core.o $(BUILD_DIR)/ble_kw47a.o \
            $(BUILD_DIR)/nfc_st25r501.o $(BUILD_DIR)/uwb_ncj29d6.o \
            $(BUILD_DIR)/security.o $(BUILD_DIR)/key_mgmt.o \
            $(BUILD_DIR)/stubs.o

test_ccc: $(CCC_LINK)
	$(CC) -o $(BUILD_DIR)/$@ $(CCC_LINK) $(LDFLAGS)
	$(BUILD_DIR)/$@

#===============================================================================
# Test: Unified
#===============================================================================
test_unified: $(BUILD_DIR)/test_unified.o $(ICCOA_LINK) $(CCC_LINK) $(UNIFIED_OBJ)
	$(CC) -o $(BUILD_DIR)/$@ $(BUILD_DIR)/test_unified.o \
	       $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
	       $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
	       $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
	       $(BUILD_DIR)/ccc_dk_core.o $(BUILD_DIR)/ble_kw47a.o \
	       $(BUILD_DIR)/nfc_st25r501.o $(BUILD_DIR)/uwb_ncj29d6.o \
	       $(BUILD_DIR)/security.o $(BUILD_DIR)/key_mgmt.o \
	       $(BUILD_DIR)/dk_unified.o $(BUILD_DIR)/stubs.o $(LDFLAGS)
	$(BUILD_DIR)/$@

#===============================================================================
# Test: All — run from a single monolithic binary (no source compilation)
#===============================================================================
MONOLITHIC_SRC := $(TESTDIR)/support/test_suite.c

$(BUILD_DIR)/test_suite.o: $(MONOLITHIC_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

test_all: $(BUILD_DIR)/test_suite.o \
         $(BUILD_DIR)/iccoa_dk_core.o $(BUILD_DIR)/iccoa_dk30.o \
         $(BUILD_DIR)/iccoa_dk40.o $(BUILD_DIR)/iccoa_ble.o \
         $(BUILD_DIR)/iccoa_auth.o $(BUILD_DIR)/iccoa_service.o \
         $(BUILD_DIR)/ccc_dk_core.o $(BUILD_DIR)/ble_kw47a.o \
         $(BUILD_DIR)/nfc_st25r501.o $(BUILD_DIR)/uwb_ncj29d6.o \
         $(BUILD_DIR)/security.o $(BUILD_DIR)/key_mgmt.o \
         $(BUILD_DIR)/dk_unified.o $(BUILD_DIR)/stubs.o
	$(CC) -o $(TEST_ALL_BIN) $^ $(LDFLAGS)
	$(TEST_ALL_BIN)

#===============================================================================
#  Initial compilation attempt
#===============================================================================
init_test: $(BUILD_DIR)
	@echo "=== Compiling with CC=$(CC) ==="
	@echo "Building test objects..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TESTDIR)/test/test_iccoa_dk_core.c -o $(BUILD_DIR)/test_iccoa_dk_core.o 2>&1 || true
	@echo "=== Build try done ==="

#===============================================================================
#  Clean
#===============================================================================
clean:
	rm -rf $(BUILD_DIR)

# Override default CC if not set
CC ?= gcc
