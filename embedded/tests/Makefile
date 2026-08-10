#===============================================================================
# Makefile — Vehicle C-Code Test Suite (Unity-based)
#
# Protocol source files compile against stub HAL implementations on host.
# Each test file has its own main() and builds a standalone binary.
#===============================================================================

SHELL := /bin/bash
ROOT    := $(abspath ..)
TESTDIR := $(abspath .)
VENDOR  := $(TESTDIR)/vendor
BUILD   := $(TESTDIR)/build

# Source directories
ICCOA_SRC := $(ROOT)/iccoa_protocol/src
CCC_SRC   := $(ROOT)/ccc_protocol/src
ICCE_SRC  := $(ROOT)/icce_protocol/src
UNI_SRC   := $(ROOT)/unified_protocol/src

# Source files
ICCOA_SRCS := $(ICCOA_SRC)/iccoa/iccoa_dk_core.c \
              $(ICCOA_SRC)/iccoa/dk30/iccoa_dk30.c \
              $(ICCOA_SRC)/iccoa/dk40/iccoa_dk40.c \
              $(ICCOA_SRC)/ble/iccoa_ble.c \
              $(ICCOA_SRC)/auth/iccoa_auth.c \
              $(ICCOA_SRC)/service/iccoa_service.c

CCC_SRCS   := $(CCC_SRC)/core/ccc_dk_core.c \
              $(CCC_SRC)/ble/ble_kw47a.c \
              $(CCC_SRC)/nfc/nfc_st25r501.c \
              $(CCC_SRC)/security/security.c \
              $(CCC_SRC)/security/se050_scp03.c \
              $(CCC_SRC)/security/crypto_random.c \
              $(CCC_SRC)/keymgmt/key_mgmt.c

# uwb_ncj29d6.c calls classify_distance_impl() before its static definition.
# The forward declaration needs ccc_digital_key.h types. Use -include via a
# tiny wrapper that first includes the required header.
$(BUILD)/uwb_ncj29d6.o: $(CCC_SRC)/uwb/uwb_ncj29d6.c | $(BUILD)
	@echo '#include "ccc_digital_key.h"' > $(BUILD)/_uwb_fwd.h
	@echo 'static distance_zone_e classify_distance_impl(uint16_t dist_cm);' >> $(BUILD)/_uwb_fwd.h
	$(CC) $(CFLAGS) $(INCLUDES) -include $(BUILD)/_uwb_fwd.h -c -o $@ $<

ICCE_SRCS  := $(ICCE_SRC)/icce_dk_core.c \
              $(ICCE_SRC)/icce_edge.c \
              $(ICCE_SRC)/icce_security.c \
              $(ICCE_SRC)/icce_uwb.c \
              $(ICCE_SRC)/icce_vehicle.c \
              $(ICCE_SRC)/icce_zone.c \
              $(ICCE_SRC)/decision/edge_condition.c \
              $(ICCE_SRC)/crypto/crypto_engine.c \
              $(ICCE_SRC)/crypto/crypto_utils.c \
              $(ICCE_SRC)/crypto/sm2.c \
              $(ICCE_SRC)/crypto/sm3.c \
              $(ICCE_SRC)/crypto/sm4.c

UNI_SRC_FILE := $(UNI_SRC)/dk_unified.c

# Test files (each has its own main())
TEST_DIR     := $(TESTDIR)/test
TEST_ICCOA   := $(TEST_DIR)/test_iccoa_dk_core.c $(TEST_DIR)/test_iccoa_ble.c
TEST_CCC     := $(TEST_DIR)/test_ccc_dk_core.c
TEST_ICCE    := $(TEST_DIR)/test_icce.c
TEST_UNIFIED := $(TEST_DIR)/test_unified.c
TEST_ALL     := $(TESTDIR)/support/test_suite.c

# Include paths
INCLUDES := -I$(TESTDIR)/support -I$(TESTDIR)/support/icce \
            -I$(ROOT)/iccoa_protocol/include -I$(ROOT)/ccc_protocol/include \
            -I$(ROOT)/icce_protocol/include -I$(ROOT)/unified_protocol/include \
            -I$(ROOT)/system_architecture \
            -I$(ROOT)/ccc_protocol/src/logger \
            -I$(ROOT)/icce_protocol/src/crypto \
            -I$(ROOT)/icce_protocol/src/decision \
            -I$(VENDOR)/unity

# Real dk_logger.h declares dk_logger_log with va_list but DK_LOG macro
# passes raw args; the UNIT_TEST branch fixes the declaration for tests.
CFLAGS   := -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
            -Wno-unused-function -Wno-return-type -Wno-format -g -O0 \
            -Wno-implicit-function-declaration \
            -DCONFIG_ENABLE_CRYPTO -DUSE_SM_CRYPTO \
            -DUNIT_TEST -DTEST_MODE -DBLE_SUPERVISION_TIMEOUT_MS=4000 \
            -DKW47A_CS_PORT=0 -DKW47A_CS_PIN=4 \
            -DKW47A_RST_PORT=0 -DKW47A_RST_PIN=1 \
            -DKW47A_WAKE_PORT=0 -DKW47A_WAKE_PIN=2 \
            -DKW47A_IRQ_PORT=0 -DKW47A_IRQ_PIN=0 \
            -DUWB_WAKE_PIN=3 \
            -DST25R501_CS_PORT=0 -DST25R501_CS_PIN=5 \
            -DST25R501_RST_PORT=0 -DST25R501_RST_PIN=6 \
            -DST25R501_IRQ_PORT=0 -DST25R501_IRQ_PIN=7 \
            -DNCJ29D6_CS_PORT=0 -DNCJ29D6_CS_PIN=8 \
            -DNCJ29D6_IRQ_PORT=0 -DNCJ29D6_IRQ_PIN=9 \
            -DNCJ29D6_RST_PORT=0 -DNCJ29D6_RST_PIN=10 \
            -DNCJ29D6_WAKE_PORT=0 -DNCJ29D6_WAKE_PIN=11 \
            -DCCC_SERVICE_UUID=0xFFD1 \
            -DNFC_SERVICE_UUID=0xFFD2 \
            -DUWB_CONFIG_UUID=0xFFD3
LDFLAGS  := -lm

#---------------------------------------------------------------------------
# Generic compile rule
#---------------------------------------------------------------------------
$(BUILD):
	mkdir -p $@

define cc_src
$(BUILD)/$(notdir $(basename $(1))).o: $(1) | $(BUILD)
	$$(CC) $$(CFLAGS) $$(INCLUDES) -c -o $$@ $$<
endef

$(foreach f,$(ICCOA_SRCS),$(eval $(call cc_src,$(f))))
$(foreach f,$(CCC_SRCS),  $(eval $(call cc_src,$(f))))
$(foreach f,$(ICCE_SRCS), $(eval $(call cc_src,$(f))))

$(BUILD)/dk_unified.o: $(UNI_SRC_FILE) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/unity.o: $(VENDOR)/unity/unity.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/stubs_hal.o: $(TESTDIR)/support/stubs_hal.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/freertos_stubs.o: $(TESTDIR)/support/freertos_stubs.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

#---------------------------------------------------------------------------
# Test objects (standalone — each has main())
#---------------------------------------------------------------------------
$(BUILD)/test_iccoa_dk_core.o: $(TEST_DIR)/test_iccoa_dk_core.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_iccoa_ble.o: $(TEST_DIR)/test_iccoa_ble.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_ccc_dk_core.o: $(TEST_DIR)/test_ccc_dk_core.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_icce.o: $(TEST_DIR)/test_icce.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_unified.o: $(TEST_DIR)/test_unified.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_suite.o: $(TEST_ALL) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<

#---------------------------------------------------------------------------
# Non-main test objects (for linking into combined binaries)
#---------------------------------------------------------------------------
$(BUILD)/test_iccoa_dk_core_lib.o: $(TEST_DIR)/test_iccoa_dk_core.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DICCOA_CORE_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_iccoa_ble_lib.o: $(TEST_DIR)/test_iccoa_ble.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DICCOA_CORE_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_ccc_lib.o: $(TEST_DIR)/test_ccc_dk_core.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_CCC_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_icce_lib.o: $(TEST_DIR)/test_icce.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_ICCE_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_unified_lib.o: $(TEST_DIR)/test_unified.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_UNIFIED_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_edge_condition.o: $(TEST_DIR)/test_edge_condition.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_edge_condition_lib.o: $(TEST_DIR)/test_edge_condition.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_EDGE_COND_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_icce_edge.o: $(TEST_DIR)/test_icce_edge.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_icce_edge_lib.o: $(TEST_DIR)/test_icce_edge.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_ICCE_EDGE_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_sm_crypto.o: $(TEST_DIR)/test_sm_crypto.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_sm_crypto_lib.o: $(TEST_DIR)/test_sm_crypto.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_SM_CRYPTO_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<
$(BUILD)/test_se050_scp03.o: $(TEST_DIR)/test_se050_scp03.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -ggdb -c -o $@ $<
$(BUILD)/test_se050_scp03_lib.o: $(TEST_DIR)/test_se050_scp03.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -DTEST_SE050_SCP03_NO_MAIN -DTEST_LIB_MODE -c -o $@ $<

#---------------------------------------------------------------------------
# Standalone test binaries
#---------------------------------------------------------------------------
.PHONY: all clean test_iccoa test_ccc test_icce test_unified test_all

all: test_all

test_iccoa: $(BUILD)/test_iccoa_dk_core.o \
            $(BUILD)/iccoa_dk_core.o $(BUILD)/iccoa_dk30.o \
            $(BUILD)/iccoa_dk40.o $(BUILD)/iccoa_ble.o \
            $(BUILD)/iccoa_auth.o $(BUILD)/iccoa_service.o \
            $(BUILD)/stubs_hal.o $(BUILD)/unity.o
	$(CC) -o $(BUILD)/$@ $^ $(LDFLAGS)
	$(BUILD)/$@

test_ccc: $(BUILD)/test_ccc_dk_core.o \
          $(BUILD)/ccc_dk_core.o $(BUILD)/ble_kw47a.o \
          $(BUILD)/nfc_st25r501.o $(BUILD)/uwb_ncj29d6.o \
          $(BUILD)/security.o $(BUILD)/se050_scp03.o $(BUILD)/crypto_random.o \
          $(BUILD)/key_mgmt.o \
          $(BUILD)/stubs_hal.o $(BUILD)/freertos_stubs.o $(BUILD)/unity.o
	$(CC) -o $(BUILD)/$@ $^ $(LDFLAGS)
	$(BUILD)/$@

test_icce: $(BUILD)/test_icce.o \
          $(BUILD)/icce_dk_core.o $(BUILD)/icce_edge.o \
          $(BUILD)/icce_security.o $(BUILD)/icce_uwb.o \
          $(BUILD)/icce_vehicle.o $(BUILD)/icce_zone.o \
          $(BUILD)/edge_condition.o \
          $(BUILD)/crypto_engine.o $(BUILD)/crypto_utils.o \
          $(BUILD)/sm2.o $(BUILD)/sm3.o $(BUILD)/sm4.o \
          $(BUILD)/stubs_hal.o $(BUILD)/unity.o
	$(CC) -o $(BUILD)/$@ $^ $(LDFLAGS)
	$(BUILD)/$@

test_unified: $(BUILD)/test_unified.o \
              $(BUILD)/iccoa_dk_core.o $(BUILD)/iccoa_dk30.o \
              $(BUILD)/iccoa_dk40.o $(BUILD)/iccoa_ble.o \
              $(BUILD)/iccoa_auth.o $(BUILD)/iccoa_service.o \
              $(BUILD)/ccc_dk_core.o $(BUILD)/ble_kw47a.o \
              $(BUILD)/nfc_st25r501.o $(BUILD)/uwb_ncj29d6.o \
              $(BUILD)/security.o $(BUILD)/se050_scp03.o $(BUILD)/crypto_random.o \
              $(BUILD)/key_mgmt.o \
              $(BUILD)/icce_dk_core.o $(BUILD)/icce_edge.o \
              $(BUILD)/icce_security.o $(BUILD)/icce_uwb.o \
              $(BUILD)/icce_vehicle.o $(BUILD)/icce_zone.o \
              $(BUILD)/edge_condition.o \
              $(BUILD)/crypto_engine.o $(BUILD)/crypto_utils.o \
              $(BUILD)/sm2.o $(BUILD)/sm3.o $(BUILD)/sm4.o \
              $(BUILD)/dk_unified.o $(BUILD)/stubs_hal.o $(BUILD)/freertos_stubs.o $(BUILD)/unity.o
	$(CC) -o $(BUILD)/$@ $^ $(LDFLAGS)
	$(BUILD)/$@

test_all: $(BUILD)/test_suite.o \
          $(BUILD)/test_iccoa_dk_core_lib.o $(BUILD)/test_iccoa_ble_lib.o \
          $(BUILD)/test_ccc_lib.o $(BUILD)/test_icce_lib.o \
          $(BUILD)/test_unified_lib.o $(BUILD)/test_edge_condition_lib.o \
          $(BUILD)/test_icce_edge_lib.o $(BUILD)/test_sm_crypto_lib.o \
          $(BUILD)/test_se050_scp03_lib.o \
          $(BUILD)/iccoa_dk_core.o $(BUILD)/iccoa_dk30.o \
          $(BUILD)/iccoa_dk40.o $(BUILD)/iccoa_ble.o \
          $(BUILD)/iccoa_auth.o $(BUILD)/iccoa_service.o \
          $(BUILD)/ccc_dk_core.o $(BUILD)/ble_kw47a.o \
          $(BUILD)/nfc_st25r501.o $(BUILD)/uwb_ncj29d6.o \
          $(BUILD)/security.o $(BUILD)/crypto_random.o \
          $(BUILD)/key_mgmt.o \
          $(BUILD)/icce_dk_core.o $(BUILD)/icce_security.o \
          $(BUILD)/icce_uwb.o \
          $(BUILD)/icce_vehicle.o $(BUILD)/icce_zone.o \
          $(BUILD)/edge_condition.o \
          $(BUILD)/crypto_engine.o $(BUILD)/crypto_utils.o \
          $(BUILD)/sm2.o $(BUILD)/sm3.o $(BUILD)/sm4.o \
          $(BUILD)/dk_unified.o $(BUILD)/stubs_hal.o $(BUILD)/freertos_stubs.o $(BUILD)/unity.o
	$(CC) -o $(BUILD)/$@ $^ $(LDFLAGS)
	$(BUILD)/$@

clean:
	rm -rf $(BUILD)

CC ?= gcc
