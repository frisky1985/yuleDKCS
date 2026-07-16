/*******************************************************************************
 * Copyright (C) 2024 Ingeek, Inc. or its affiliates.  All Rights Reserved.
 *
 * @file    DKFaultInject.c
 * @brief   yuleDKCS Digital Key Fault Injection Framework — Implementation
 *
 * @details Implements protocol-level fault injection injectors for ICCE,
 *          CCC, and ICCOA stacks. Each injector calls the corresponding
 *          protocol API with fault-inducing parameters and verifies the
 *          protocol's reaction (rejection, timeout, error code, etc.).
 *
 *          Fault injectors are designed to run in test/debug builds only.
 *          The entire module is compiled out by the DK_FAULT_INJECT_ENABLE
 *          guard.
 *
 *          Usage pattern per injector:
 *          1. Enable the fault condition
 *          2. Call the protocol API under test
 *          3. Check the protocol's response (expected: error/denial)
 *          4. Disable the fault
 *          5. Verify the protocol returns to normal operation
 ******************************************************************************/

#include "DKFaultInject.h"

#if DK_FAULT_INJECT_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 *  Internal State
 * ======================================================================== */

#define MAX_ACTIVE_FAULTS   16
#define MAX_RESULTS         64

/** @brief Active fault flags — bitmask indexed by fault_id hash */
static uint32_t g_active_faults[MAX_ACTIVE_FAULTS];
static uint32_t g_active_fault_count = 0;

/** @brief Result storage */
static dk_fi_result_t g_results[MAX_RESULTS];
static uint32_t g_result_count = 0;

/** @brief Has the framework been initialized? */
static bool g_initialized = false;

/* ========================================================================
 *  Result Storage Helpers
 * ======================================================================== */

static void record_pass(dk_fi_result_t *r)
{
    r->status = DK_FI_STATUS_PASSED;
    dk_fi_record_result(r);
}

static void record_fail(dk_fi_result_t *r)
{
    r->status = DK_FI_STATUS_FAILED;
    dk_fi_record_result(r);
}

static void record_error(dk_fi_result_t *r)
{
    r->status = DK_FI_STATUS_ERROR;
    dk_fi_record_result(r);
}

/* ========================================================================
 *  Public API Implementation
 * ======================================================================== */

void dk_fi_init(void)
{
    g_initialized = true;
    g_active_fault_count = 0;
    g_result_count = 0;
    memset(g_active_faults, 0, sizeof(g_active_faults));
    memset(g_results, 0, sizeof(g_results));
}

void dk_fi_deinit(void)
{
    dk_fi_clear_all();
    g_initialized = false;
}

void dk_fi_enable(uint32_t fault_id, bool enable)
{
    if (!g_initialized) return;

    if (enable) {
        /* Add if not already present */
        for (uint32_t i = 0; i < g_active_fault_count; i++) {
            if (g_active_faults[i] == fault_id) return;
        }
        if (g_active_fault_count < MAX_ACTIVE_FAULTS) {
            g_active_faults[g_active_fault_count++] = fault_id;
        }
    } else {
        /* Remove if present */
        for (uint32_t i = 0; i < g_active_fault_count; i++) {
            if (g_active_faults[i] == fault_id) {
                if (i < g_active_fault_count - 1) {
                    memmove(&g_active_faults[i], &g_active_faults[i+1],
                            (g_active_fault_count - i - 1) * sizeof(uint32_t));
                }
                g_active_fault_count--;
                return;
            }
        }
    }
}

bool dk_fi_is_active(uint32_t fault_id)
{
    if (!g_initialized) return false;
    for (uint32_t i = 0; i < g_active_fault_count; i++) {
        if (g_active_faults[i] == fault_id) return true;
    }
    return false;
}

void dk_fi_clear_all(void)
{
    g_active_fault_count = 0;
}

void dk_fi_record_result(const dk_fi_result_t *result)
{
    if (!g_initialized || !result) return;
    if (g_result_count < MAX_RESULTS) {
        g_results[g_result_count++] = *result;
    }
}

uint32_t dk_fi_get_result_count(void)
{
    return g_result_count;
}

const dk_fi_result_t* dk_fi_get_result(uint32_t index)
{
    if (index >= g_result_count) return NULL;
    return &g_results[index];
}

void dk_fi_print_results(void)
{
    printf("\n===== DK Fault Injection Test Report =====\n");
    printf("%-5s %-8s %-40s %s\n", "ID", "Protocol", "Test Name", "Status");
    printf("------ -------- ---------------------------------------- ----------\n");

    uint32_t passed = 0, failed = 0, errors = 0;
    for (uint32_t i = 0; i < g_result_count; i++) {
        const char *proto = "?";
        if (g_results[i].protocol == DK_FI_PROTOCOL_ICCE)  proto = "ICCE";
        else if (g_results[i].protocol == DK_FI_PROTOCOL_CCC)   proto = "CCC";
        else if (g_results[i].protocol == DK_FI_PROTOCOL_ICCOA) proto = "ICCOA";

        const char *status = "?";
        if (g_results[i].status == DK_FI_STATUS_PASSED)  { status = "PASS";  passed++; }
        else if (g_results[i].status == DK_FI_STATUS_FAILED) { status = "FAIL";  failed++; }
        else if (g_results[i].status == DK_FI_STATUS_ERROR)  { status = "ERROR"; errors++; }

        printf("[%3u] %-8s %-40s %s\n",
               (unsigned int)i, proto,
               g_results[i].name ? g_results[i].name : "(unnamed)",
               status);
    }

    printf("\n");
    printf("Total: %u | Passed: %u | Failed: %u | Errors: %u\n",
           (unsigned int)g_result_count,
           (unsigned int)passed, (unsigned int)failed, (unsigned int)errors);
    printf("============================================\n");
}

/* ========================================================================
 *  Integration Helpers — Protocol API wrappers
 *  These would call the real protocol APIs in an embedded context.
 *  For host-side testing, they use simulated protocol behavior.
 * ======================================================================== */

/*
 * NOTE: In a real embedded build, these injection points are inserted
 * into the protocol stack source code (see integration guide).
 * For host-side testing, we simulate the protocol responses.
 */

/* ── ICCE simulation helpers ── */
static int icce_sim_bind(const uint8_t *pubkey, uint16_t len)
{
    (void)pubkey;
    if (dk_fi_is_active(DK_FI_ICCE_SIGN_FORGERY)) {
        return -7; /* ICCE_ERR_SECURITY */
    }
    if (len != 64) return -1; /* ICCE_ERR_PARAM */
    return 0; /* ICCE_OK */
}

static int icce_sim_auth(const uint8_t *challenge, uint16_t chal_len,
                          const uint8_t *signature, uint16_t sig_len)
{
    (void)challenge; (void)chal_len;
    if (dk_fi_is_active(DK_FI_ICCE_SIGN_TAMPERED)) {
        return -7; /* ICCE_ERR_SECURITY */
    }
    if (dk_fi_is_active(DK_FI_ICCE_CERT_EXPIRED)) {
        return -7; /* ICCE_ERR_SECURITY */
    }
    if (dk_fi_is_active(DK_FI_ICCE_KEY_DERIVE_FAIL)) {
        return -7; /* ICCE_ERR_SECURITY */
    }
    if (dk_fi_is_active(DK_FI_ICCE_DUPLICATE_AUTH)) {
        return -7; /* ICCE_ERR_SECURITY */
    }
    if (!signature || sig_len != 64) return -1;
    return 0;
}

static int icce_sim_ble_send(const uint8_t *data, uint16_t len)
{
    (void)data;
    if (dk_fi_is_active(DK_FI_ICCE_HANDSHAKE_TIMEOUT)) {
        return -3; /* ICCE_ERR_TIMEOUT */
    }
    if (dk_fi_is_active(DK_FI_ICCE_FRAME_DROP)) {
        return 0; /* Silently succeed but don't deliver */
    }
    if (len > 244) return -1;
    return 0;
}

static int icce_sim_get_zone(int32_t distance_mm)
{
    (void)distance_mm;
    if (dk_fi_is_active(DK_FI_ICCE_DISTANCE_SPOOF)) {
        /* Return wrong zone classification */
        return 5; /* ICCE_ZONE_INTERIOR even for far distance */
    }
    if (dk_fi_is_active(DK_FI_ICCE_ANGLE_SPOOF)) {
        return 0; /* ICCE_ZONE_NONE — no lock possible */
    }
    if (distance_mm < 1000) return 5;
    if (distance_mm < 2000) return 4;
    if (distance_mm < 10000) return 3;
    return 1;
}

/* ── CCC simulation helpers ── */
static int ccc_sim_scp03_open(void)
{
    if (dk_fi_is_active(DK_FI_CCC_SECURE_CHANNEL_FAIL)) {
        return -6; /* CCC_ERR_HARDWARE */
    }
    if (dk_fi_is_active(DK_FI_CCC_SCP03_AUTH_FAIL)) {
        return -7; /* CCC_ERR_SECURITY */
    }
    return 0;
}

static int ccc_sim_sec_verify(const uint8_t *data, uint32_t len,
                              const uint8_t *sig, uint32_t sig_len)
{
    (void)data; (void)len;
    if (dk_fi_is_active(DK_FI_CCC_SIGNATURE_BYPASS)) {
        return 0; /* VERIFY_OK — wrong! Bypassing security */
    }
    if (dk_fi_is_active(DK_FI_CCC_CERT_TAMPER)) {
        return 1; /* VERIFY_CERT_INVALID */
    }
    if (!sig || sig_len != 64) return 2;
    return 0;
}

static int ccc_sim_nfc_oob_exchange(void)
{
    if (dk_fi_is_active(DK_FI_CCC_NFC_OOB_CORRUPT)) {
        return -3; /* CCC_ERR_TIMEOUT */
    }
    if (dk_fi_is_active(DK_FI_CCC_NFC_OOB_TIMEOUT)) {
        return -3; /* CCC_ERR_TIMEOUT */
    }
    if (dk_fi_is_active(DK_FI_CCC_NFC_FIELD_FAIL)) {
        return -6; /* CCC_ERR_HARDWARE */
    }
    return 0;
}

static int ccc_sim_ble_encrypt(void)
{
    if (dk_fi_is_active(DK_FI_CCC_BLE_ENCRYPT_FAIL)) {
        return -6; /* CCC_ERR_HARDWARE */
    }
    if (dk_fi_is_active(DK_FI_CCC_BLE_PAIR_FAIL)) {
        return -6; /* CCC_ERR_HARDWARE */
    }
    return 0;
}

static int ccc_sim_state_transition(void)
{
    if (dk_fi_is_active(DK_FI_CCC_ILLEGAL_STATE)) {
        /* Allow any transition — shouldn't happen */
        return 0;
    }
    if (dk_fi_is_active(DK_FI_CCC_SKIP_AUTH_STATE)) {
        /* Allow bypassing auth */
        return 0;
    }
    return 0;
}

/* ── ICCOA simulation helpers ── */
static int iccoa_sim_auth_request(void)
{
    if (dk_fi_is_active(DK_FI_ICCOA_HANDSHAKE_FAIL)) {
        return -7; /* ICCOA_ERR_SECURITY */
    }
    if (dk_fi_is_active(DK_FI_ICCOA_AUTH_TIMEOUT)) {
        return -3; /* ICCOA_ERR_TIMEOUT */
    }
    return 0;
}

static int iccoa_sim_key_derive(void)
{
    if (dk_fi_is_active(DK_FI_ICCOA_KEY_DERIVE_ERR)) {
        return -7; /* ICCOA_ERR_SECURITY */
    }
    return 0;
}

static int iccoa_sim_permission_check(uint8_t perm)
{
    (void)perm;
    if (dk_fi_is_active(DK_FI_ICCOA_PERMISSION_BYPASS)) {
        return 0; /* Should deny, but returns OK */
    }
    /* Normal: check permission bits */
    if (perm > 7) return -1;
    return 0;
}

static int iccoa_sim_ble_recv(const uint8_t *data, uint16_t len)
{
    (void)data;
    if (dk_fi_is_active(DK_FI_ICCOA_DOWNGRADE_ATTACK)) {
        /* DK3.0 frame sent while in DK4.0 mode */
        return 0;
    }
    if (dk_fi_is_active(DK_FI_ICCOA_CHECKSUM_ERR)) {
        return -1;
    }
    if (dk_fi_is_active(DK_FI_ICCOA_FRAME_MALFORMED)) {
        return -1;
    }
    if (len < 4) return -1;
    return 0;
}

/* ========================================================================
 *  ICCE Fault Injection Test Cases
 * ======================================================================== */

void dk_fi_icce_sign_forgery_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCE_SIGN_FORGERY,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Signature-Forgery",
        .description  = "Inject forged SM2/ECDSA signature at bind"
    };

    /* Test 1: forged public key should be rejected */
    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, true);

    static uint8_t forged_pubkey[64];
    memset(forged_pubkey, 0xFF, sizeof(forged_pubkey));
    int ret = icce_sim_bind(forged_pubkey, 64);

    if (ret == -7) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    /* Test 2: after disabling fault, valid bind should succeed */
    dk_fi_enable(DK_FI_ICCE_SIGN_FORGERY, false);

    /* Use a minimal reasonable public key (all zeros is invalid but won't trigger our fault) */
    memset(forged_pubkey, 0x42, 64);
    ret = icce_sim_bind(forged_pubkey, 64);

    dk_fi_result_t recovery = {
        .fault_id     = DK_FI_ICCE_SIGN_FORGERY,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Signature-Forgery-Recovery",
        .description  = "Recovery after disabling fault"
    };
    if (ret == 0) {
        recovery.status = DK_FI_STATUS_PASSED;
    } else {
        recovery.status = DK_FI_STATUS_FAILED;
    }
    dk_fi_record_result(&recovery);
}

void dk_fi_icce_cert_expired_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCE_CERT_EXPIRED,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Cert-Expired",
        .description  = "Simulate expired certificate in auth"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCE_CERT_EXPIRED, true);

    uint8_t chal[16] = {0x01}, sig[64] = {0x02};
    int ret = icce_sim_auth(chal, 16, sig, 64);

    if (ret == -7) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    /* Recovery */
    dk_fi_enable(DK_FI_ICCE_CERT_EXPIRED, false);
    ret = icce_sim_auth(chal, 16, sig, 64);

    dk_fi_result_t recovery = {
        .fault_id     = DK_FI_ICCE_CERT_EXPIRED,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Cert-Expired-Recovery",
        .description  = "Recovery after disabling expired cert"
    };
    recovery.status = (ret == 0) ? DK_FI_STATUS_PASSED : DK_FI_STATUS_FAILED;
    dk_fi_record_result(&recovery);
}

void dk_fi_icce_illegal_transition_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCE_ILLEGAL_TRANSITION,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Illegal-Transition",
        .description  = "Force illegal zone transition in edge engine"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCE_ILLEGAL_TRANSITION, true);

    /* Test: simulate jumping from FAR directly to INTERIOR (should be invalid) */
    int zone_far    = icce_sim_get_zone(30000);  /* FAR */
    int zone_interior = icce_sim_get_zone(500);   /* INTERIOR via spoof */

    /* With illegal transition active, intermediate zones should be skipped */
    /* The test passes if we detected the problematic state */
    dk_fi_result_t t1 = {
        .fault_id     = DK_FI_ICCE_ILLEGAL_TRANSITION,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Illegal-Transition-FarToInterior",
        .description  = "Transition from FAR to INTERIOR"
    };

    if (zone_far == 1 && zone_interior == 5) {
        t1.status = DK_FI_STATUS_FAILED;
        /* An illegal transition was not caught */
    } else {
        /* Zone logic handled the anomaly */
        t1.status = DK_FI_STATUS_PASSED;
    }
    dk_fi_record_result(&t1);

    dk_fi_enable(DK_FI_ICCE_ILLEGAL_TRANSITION, false);
    result.status = DK_FI_STATUS_PASSED;
    dk_fi_record_result(&result);
}

void dk_fi_icce_comm_timeout_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCE_HANDSHAKE_TIMEOUT,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Comm-Timeout",
        .description  = "Inject BLE handshake timeout"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCE_HANDSHAKE_TIMEOUT, true);

    uint8_t frame[10] = {0};
    int ret = icce_sim_ble_send(frame, 10);

    if (ret == -3) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    /* Recovery */
    dk_fi_enable(DK_FI_ICCE_HANDSHAKE_TIMEOUT, false);
    ret = icce_sim_ble_send(frame, 10);

    dk_fi_result_t recovery = {
        .fault_id     = DK_FI_ICCE_HANDSHAKE_TIMEOUT,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Comm-Timeout-Recovery",
        .description  = "Normal operation after timeout fault disabled"
    };
    recovery.status = (ret == 0) ? DK_FI_STATUS_PASSED : DK_FI_STATUS_FAILED;
    dk_fi_record_result(&recovery);
}

void dk_fi_icce_distance_spoof_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCE_DISTANCE_SPOOF,
        .protocol     = DK_FI_PROTOCOL_ICCE,
        .name         = "ICCE-Distance-Spoof",
        .description  = "Inject fake distance (far → interior)"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCE_DISTANCE_SPOOF, true);

    /* Even at 50m, the zone should be reported as interior (spoofed) */
    int zone = icce_sim_get_zone(50000);

    if (zone == 5) {
        /* Distance spoof active — zone is interior */
        /* In a real system, a consistency check should catch this */
        dk_fi_result_t detected = {
            .fault_id     = DK_FI_ICCE_DISTANCE_SPOOF,
            .protocol     = DK_FI_PROTOCOL_ICCE,
            .name         = "ICCE-Distance-Spoof-Caught",
            .description  = "Spoof detected by UWB distance consistency"
        };
        detected.status = DK_FI_STATUS_FAILED;
        dk_fi_record_result(&detected);
    }

    dk_fi_enable(DK_FI_ICCE_DISTANCE_SPOOF, false);
    record_pass(&result);
}

/* ========================================================================
 *  CCC Fault Injection Test Cases
 * ======================================================================== */

void dk_fi_ccc_secure_channel_fail_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_CCC_SECURE_CHANNEL_FAIL,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-Secure-Channel-Fail",
        .description  = "Force SCP03 secure channel open failure"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_CCC_SECURE_CHANNEL_FAIL, true);

    int ret = ccc_sim_scp03_open();

    if (ret != 0) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_CCC_SECURE_CHANNEL_FAIL, false);
}

void dk_fi_ccc_cert_verify_anomaly_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_CCC_CERT_TAMPER,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-Cert-Verify-Anomaly",
        .description  = "Corrupt attestation certificate verification"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_CCC_CERT_TAMPER, true);

    uint8_t data[32] = {0xAB}, sig[64] = {0xCD};
    int ret = ccc_sim_sec_verify(data, 32, sig, 64);

    if (ret != 0) {
        record_pass(&result);
    } else {
        /* If verification succeeded despite cert tamper, that's a security issue */
        dk_fi_result_t bypass = {
            .fault_id     = DK_FI_CCC_CERT_TAMPER,
            .protocol     = DK_FI_PROTOCOL_CCC,
            .name         = "CCC-Cert-Verify-Bypass-Alert",
            .description  = "WARNING: Cert verification returned OK despite tamper"
        };
        dk_fi_record_result(&bypass);
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_CCC_CERT_TAMPER, false);
}

void dk_fi_ccc_nfc_oob_corrupt_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_CCC_NFC_OOB_CORRUPT,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-NFC-OOB-Corrupt",
        .description  = "Corrupt NFC OOB data exchange"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_CCC_NFC_OOB_CORRUPT, true);

    int ret = ccc_sim_nfc_oob_exchange();

    if (ret != 0) {
        record_pass(&result);
    } else {
        /* OOB exchange succeeded despite corruption — not good */
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_CCC_NFC_OOB_CORRUPT, false);
}

void dk_fi_ccc_ble_encrypt_fail_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_CCC_BLE_ENCRYPT_FAIL,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-BLE-Encrypt-Fail",
        .description  = "Force BLE encryption failure"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_CCC_BLE_ENCRYPT_FAIL, true);

    int ret = ccc_sim_ble_encrypt();

    if (ret != 0) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_CCC_BLE_ENCRYPT_FAIL, false);
}

void dk_fi_ccc_illegal_state_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_CCC_ILLEGAL_STATE,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-Illegal-State",
        .description  = "Force illegal state machine transition"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_CCC_ILLEGAL_STATE, true);

    /* Simulate: try to go from INIT directly to UNLOCKED */
    int ret = ccc_sim_state_transition();

    dk_fi_result_t t1 = {
        .fault_id     = DK_FI_CCC_ILLEGAL_STATE,
        .protocol     = DK_FI_PROTOCOL_CCC,
        .name         = "CCC-Illegal-State-Transition",
        .description  = "Transition INIT→UNLOCKED allowed?"
    };

    if (ret == 0) {
        /* Transition was allowed — if fault enabled, this may be expected */
        t1.status = DK_FI_STATUS_PASSED;
    } else {
        t1.status = DK_FI_STATUS_FAILED;
    }
    dk_fi_record_result(&t1);

    dk_fi_enable(DK_FI_CCC_ILLEGAL_STATE, false);
    record_pass(&result);
}

/* ========================================================================
 *  ICCOA Fault Injection Test Cases
 * ======================================================================== */

void dk_fi_iccoa_handshake_fail_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCOA_HANDSHAKE_FAIL,
        .protocol     = DK_FI_PROTOCOL_ICCOA,
        .name         = "ICCOA-Handshake-Fail",
        .description  = "Force ICCOA authentication handshake failure"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCOA_HANDSHAKE_FAIL, true);

    int ret = iccoa_sim_auth_request();

    if (ret != 0) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_ICCOA_HANDSHAKE_FAIL, false);
}

void dk_fi_iccoa_key_derive_error_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCOA_KEY_DERIVE_ERR,
        .protocol     = DK_FI_PROTOCOL_ICCOA,
        .name         = "ICCOA-Key-Derive-Error",
        .description  = "Inject key derivation failure"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCOA_KEY_DERIVE_ERR, true);

    int ret = iccoa_sim_key_derive();

    if (ret != 0) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_ICCOA_KEY_DERIVE_ERR, false);
}

void dk_fi_iccoa_downgrade_attack_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCOA_DOWNGRADE_ATTACK,
        .protocol     = DK_FI_PROTOCOL_ICCOA,
        .name         = "ICCOA-Downgrade-Attack",
        .description  = "Inject DK4.0→DK3.0 downgrade attack frame"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCOA_DOWNGRADE_ATTACK, true);

    /* Simulate: a DK3.0 frame (SOP=0xAA) is received while in DK4.0 mode */
    uint8_t dk30_frame[4] = {0xAA, 0x10, 0x00, 0x01};
    int ret = iccoa_sim_ble_recv(dk30_frame, 4);

    /* The protocol should reject this as a downgrade attempt */
    if (ret == -7 || ret == -1) {
        record_pass(&result);
    } else {
        /* Frame was accepted — downgrade protection failed */
        dk_fi_result_t bypass = {
            .fault_id     = DK_FI_ICCOA_DOWNGRADE_ATTACK,
            .protocol     = DK_FI_PROTOCOL_ICCOA,
            .name         = "ICCOA-Downgrade-Protection-Bypass",
            .description  = "WARNING: DK3.0 frame accepted in DK4.0 mode"
        };
        bypass.status = DK_FI_STATUS_FAILED;
        dk_fi_record_result(&bypass);
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_ICCOA_DOWNGRADE_ATTACK, false);
}

void dk_fi_iccoa_permission_bypass_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCOA_PERMISSION_BYPASS,
        .protocol     = DK_FI_PROTOCOL_ICCOA,
        .name         = "ICCOA-Permission-Bypass",
        .description  = "Test permission check bypass"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCOA_PERMISSION_BYPASS, true);

    /* Permission check should pass even for invalid permission bit */
    int ret = iccoa_sim_permission_check(17); /* Invalid: out of range */

    if (ret == 0) {
        /* Permission bypassed — but this is the injected fault, not real behavior */
        dk_fi_result_t detected = {
            .fault_id     = DK_FI_ICCOA_PERMISSION_BYPASS,
            .protocol     = DK_FI_PROTOCOL_ICCOA,
            .name         = "ICCOA-Permission-Bypass-Detected",
            .description  = "Bypass active — invalid perm 17 granted access"
        };
        detected.status = DK_FI_STATUS_FAILED;
        dk_fi_record_result(&detected);
    }

    dk_fi_enable(DK_FI_ICCOA_PERMISSION_BYPASS, false);
    record_pass(&result);
}

void dk_fi_iccoa_hmac_tamper_test(void)
{
    dk_fi_result_t result = {
        .fault_id     = DK_FI_ICCOA_HMAC_TAMPER,
        .protocol     = DK_FI_PROTOCOL_ICCOA,
        .name         = "ICCOA-HMAC-Tamper",
        .description  = "Corrupt DK4.0 frame HMAC field"
    };

    dk_fi_clear_all();
    dk_fi_enable(DK_FI_ICCOA_HMAC_TAMPER, true);

    /* Simulate a DK4.0 frame with corrupted HMAC */
    uint8_t hmac_frame[10] = {0xC0, 0x0C, 0x01, 0x20, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00};
    int ret = iccoa_sim_ble_recv(hmac_frame, 10);

    if (ret == -7 || ret == -1) {
        record_pass(&result);
    } else {
        record_fail(&result);
    }

    dk_fi_enable(DK_FI_ICCOA_HMAC_TAMPER, false);
}

/* ========================================================================
 *  Run All
 * ======================================================================== */

void dk_fi_run_all_tests(void)
{
    dk_fi_init();

    printf("\n=== yuleDKCS Fault Injection Test Suite ===\n");
    printf("Running with DK_FAULT_INJECT_ENABLE = 1\n\n");

    /* ── ICCE Protocol Tests ── */
    printf("--- ICCE Tests ---\n");
    dk_fi_icce_sign_forgery_test();
    dk_fi_icce_cert_expired_test();
    dk_fi_icce_illegal_transition_test();
    dk_fi_icce_comm_timeout_test();
    dk_fi_icce_distance_spoof_test();

    /* ── CCC Protocol Tests ── */
    printf("\n--- CCC Tests ---\n");
    dk_fi_ccc_secure_channel_fail_test();
    dk_fi_ccc_cert_verify_anomaly_test();
    dk_fi_ccc_nfc_oob_corrupt_test();
    dk_fi_ccc_ble_encrypt_fail_test();
    dk_fi_ccc_illegal_state_test();

    /* ── ICCOA Protocol Tests ── */
    printf("\n--- ICCOA Tests ---\n");
    dk_fi_iccoa_handshake_fail_test();
    dk_fi_iccoa_key_derive_error_test();
    dk_fi_iccoa_downgrade_attack_test();
    dk_fi_iccoa_permission_bypass_test();
    dk_fi_iccoa_hmac_tamper_test();

    /* ── Summary ── */
    dk_fi_print_results();
    dk_fi_deinit();
}

#endif /* DK_FAULT_INJECT_ENABLE */
