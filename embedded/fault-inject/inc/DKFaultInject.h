/*******************************************************************************
 * Copyright (C) 2024 Ingeek, Inc. or its affiliates.  All Rights Reserved.
 *
 * @file    DKFaultInject.h
 * @brief   yuleDKCS Digital Key Fault Injection Framework — Protocol-Level API
 *
 * @details Extends the yuleOSH FaultInject framework with protocol-level
 *          fault injection for ICCE, CCC, and ICCOA digital key stacks.
 *
 *          Architecture:
 *          ┌───────────────────────┐
 *          │ yuleOSH FaultInject   │  Layer 1: CPU exception injection
 *          │ (src/fault-inject/)   │  Layer 2: Task-level fault injection
 *          └──────┬────────────────┘
 *                 │ extends
 *          ┌──────┴────────────────┐
 *          │ yuleDKCS DK Fault     │  Protocol-level fault injection
 *          │ Inject Extension      │  (signature, handshake, state machine)
 *          └──────┬────────────────┘
 *                 │ injects into
 *          ┌──────┼────────┬───────┐
 *          │ ICCE │  CCC   │ ICCOA │
 *          └──────┴────────┴───────┘
 *
 *          Three fault categories:
 *          1. CRYPTO_FAULT — Signature forgery, certificate tampering
 *          2. COMM_FAULT  — Communication timeouts, data corruption
 *          3. STATE_FAULT — Illegal state transitions, sequencing errors
 *          4. RANGING_FAULT — Distance spoofing, signal anomalies
 *
 * @note    Compile-time guarded by A66T_FAULT_INJECTION_TEST_ENABLE.
 *          NEVER enable in production builds.
 ******************************************************************************/

#ifndef DK_FAULT_INJECT_H
#define DK_FAULT_INJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 *  Master Compile-Time Guard
 *  Include FaultInject.h for the embedded framework configurations.
 *  Stubs are used when DK_FAULT_INJECT_ENABLE is not defined.
 * ======================================================================== */
#if !defined(DK_FAULT_INJECT_ENABLE)
#define DK_FAULT_INJECT_ENABLE 0
#endif

/* ========================================================================
 *  Protocol Fault Type Identifiers
 * ======================================================================== */

/** @brief ICCE-specific fault injection types */
typedef enum {
    DK_FI_ICCE_NONE                = 0,

    /* ── Crypto / Signature ── */
    DK_FI_ICCE_SIGN_FORGERY        = 0x0101, /**< Inject forged SM2/ECDSA signature */
    DK_FI_ICCE_SIGN_TAMPERED       = 0x0102, /**< Tamper with valid signature bytes */
    DK_FI_ICCE_CERT_EXPIRED        = 0x0103, /**< Return expired certificate status */
    DK_FI_ICCE_CERT_REVOKED        = 0x0104, /**< Simulate revoked certificate */
    DK_FI_ICCE_KEY_DERIVE_FAIL     = 0x0105, /**< Force session key derivation failure */

    /* ── Communication ── */
    DK_FI_ICCE_HANDSHAKE_TIMEOUT   = 0x0201, /**< Inject BLE handshake timeout */
    DK_FI_ICCE_DATA_CORRUPT        = 0x0202, /**< Corrupt BLE/GATT payload */
    DK_FI_ICCE_FRAME_REORDER       = 0x0203, /**< Reorder ICCE command frames */
    DK_FI_ICCE_FRAME_DROP          = 0x0204, /**< Drop specific ICCE frames */

    /* ── State Machine ── */
    DK_FI_ICCE_ILLEGAL_TRANSITION  = 0x0301, /**< Force illegal zone transition */
    DK_FI_ICCE_ILLEGAL_SEQUENCE    = 0x0302, /**< Force wrong command sequence */
    DK_FI_ICCE_DUPLICATE_AUTH      = 0x0303, /**< Replay previous auth challenge */

    /* ── UWB/Ranging ── */
    DK_FI_ICCE_DISTANCE_SPOOF      = 0x0401, /**< Inject fake distance measurement */
    DK_FI_ICCE_ANGLE_SPOOF         = 0x0402, /**< Inject fake angle measurement */
    DK_FI_ICCE_UWB_SIGNAL_TAMPER   = 0x0403, /**< Corrupt UWB session configuration */
    DK_FI_ICCE_SESSION_REPLAY      = 0x0404, /**< Replay old UWB session ID */

    DK_FI_ICCE_MAX
} dk_fi_icce_type_e;

/** @brief CCC-specific fault injection types */
typedef enum {
    DK_FI_CCC_NONE                 = 0,

    /* ── Crypto/Security ── */
    DK_FI_CCC_SECURE_CHANNEL_FAIL  = 0x1101, /**< Force SCP03 channel open failure */
    DK_FI_CCC_CERT_TAMPER          = 0x1102, /**< Corrupt attestation certificate */
    DK_FI_CCC_SIGNATURE_BYPASS     = 0x1103, /**< Bypass ECDSA signature verification */
    DK_FI_CCC_KEY_STORE_CORRUPT    = 0x1104, /**< Corrupt SE050 key storage */
    DK_FI_CCC_SCP03_AUTH_FAIL      = 0x1105, /**< Force SCP03 mutual auth failure */
    DK_FI_CCC_MASTER_KEY_MISMATCH  = 0x1106, /**< Inject mismatched master key */

    /* ── NFC Communication ── */
    DK_FI_CCC_NFC_OOB_CORRUPT      = 0x1201, /**< Corrupt NFC OOB data */
    DK_FI_CCC_NFC_OOB_TIMEOUT      = 0x1202, /**< Inject NFC read timeout */
    DK_FI_CCC_NFC_FIELD_FAIL       = 0x1203, /**< NFC field detect failure */

    /* ── BLE ── */
    DK_FI_CCC_BLE_PAIR_FAIL        = 0x1301, /**< Force BLE OOB pairing failure */
    DK_FI_CCC_BLE_ENCRYPT_FAIL     = 0x1302, /**< Force BLE encryption failure */
    DK_FI_CCC_BLE_MTU_NEG_FAIL     = 0x1303, /**< Inject MTU negotiation failure */
    DK_FI_CCC_BLE_DISCONNECT       = 0x1304, /**< Force unexpected BLE disconnect */

    /* ── State Machine ── */
    DK_FI_CCC_ILLEGAL_STATE        = 0x1401, /**< Force illegal state transition */
    DK_FI_CCC_SKIP_AUTH_STATE      = 0x1402, /**< Skip authentication phase */
    DK_FI_CCC_RACE_CONDITION       = 0x1403, /**< Inject concurrent NFC+BLE race */

    DK_FI_CCC_MAX
} dk_fi_ccc_type_e;

/** @brief ICCOA-specific fault injection types */
typedef enum {
    DK_FI_ICCOA_NONE               = 0,

    /* ── Authentication ── */
    DK_FI_ICCOA_HANDSHAKE_FAIL     = 0x2101, /**< Force ICCOA handshake failure */
    DK_FI_ICCOA_KEY_DERIVE_ERR     = 0x2102, /**< Inject key derivation error */
    DK_FI_ICCOA_AUTH_TIMEOUT       = 0x2103, /**< Simulate auth response timeout */
    DK_FI_ICCOA_AUTH_SEQ_VIOLATION = 0x2104, /**< Violate auth command sequence */
    DK_FI_ICCOA_PERMISSION_BYPASS  = 0x2105, /**< Bypass permission check */

    /* ── Downgrade Protection ── */
    DK_FI_ICCOA_DOWNGRADE_ATTACK   = 0x2201, /**< Inject DK4.0→DK3.0 downgrade frame */
    DK_FI_ICCOA_NO_DOWNGRADE_OFF   = 0x2202, /**< Disable no_downgrade protection */
    DK_FI_ICCOA_MIXED_VERSION      = 0x2203, /**< Send mixed version frames */

    /* ── Communication ── */
    DK_FI_ICCOA_BLE_DATA_DROP      = 0x2301, /**< Drop ICCOA BLE data packets */
    DK_FI_ICCOA_CHECKSUM_ERR       = 0x2302, /**< Inject DK3.0 checksum error */
    DK_FI_ICCOA_FRAME_MALFORMED    = 0x2303, /**< Send malformed frame header */
    DK_FI_ICCOA_HMAC_TAMPER        = 0x2304, /**< Corrupt DK4.0 HMAC field */
    DK_FI_ICCOA_FLAG_TAMPER        = 0x2305, /**< Tamper with DK4.0 frame flags */

    /* ── Session ── */
    DK_FI_ICCOA_SESSION_EXPIRED    = 0x2401, /**< Force session token expiry */
    DK_FI_ICCOA_SESSION_REUSE      = 0x2402, /**< Reuse old session token */
    DK_FI_ICCOA_SESSION_MISMATCH   = 0x2403, /**< Inject mismatched session token */

    DK_FI_ICCOA_MAX
} dk_fi_iccoa_type_e;

/* ========================================================================
 *  Test Result Structures
 * ======================================================================== */

/** @brief Generic protocol fault test result */
typedef struct {
    uint32_t        fault_id;       /**< The fault type ID that was injected */
    uint32_t        protocol;       /**< 0=ICCE, 1=CCC, 2=ICCOA */
    uint32_t        status;         /**< 0=PASSED, 1=FAILED, 2=ERROR */
    const char     *name;           /**< Human-readable fault name */
    const char     *description;    /**< What was tested */
    uint32_t        duration_us;    /**< Time to complete (0 = N/A) */
} dk_fi_result_t;

#define DK_FI_STATUS_PASSED  0
#define DK_FI_STATUS_FAILED  1
#define DK_FI_STATUS_ERROR   2

#define DK_FI_PROTOCOL_ICCE   0
#define DK_FI_PROTOCOL_CCC    1
#define DK_FI_PROTOCOL_ICCOA  2

/* ========================================================================
 *  Public API
 * ======================================================================== */

#if DK_FAULT_INJECT_ENABLE

/**
 * @brief Initialize the digital key fault injection framework.
 * @details Must be called before any other DK fault inject functions.
 */
void dk_fi_init(void);

/**
 * @brief Deinitialize and clear all fault injection state.
 */
void dk_fi_deinit(void);

/**
 * @brief Enable or disable a specific fault at next protocol operation.
 * @param fault_id  The fault type ID to enable/disable.
 * @param enable    true to enable, false to disable.
 */
void dk_fi_enable(uint32_t fault_id, bool enable);

/**
 * @brief Check if a specific fault is currently active.
 * @param fault_id  The fault type ID to check.
 * @return true if the fault is active.
 */
bool dk_fi_is_active(uint32_t fault_id);

/**
 * @brief Clear all active faults.
 */
void dk_fi_clear_all(void);

/**
 * @brief Record a fault injection test result.
 * @param result  Test result to record.
 */
void dk_fi_record_result(const dk_fi_result_t *result);

/**
 * @brief Get the total number of recorded results.
 * @return Number of results.
 */
uint32_t dk_fi_get_result_count(void);

/**
 * @brief Get a specific test result by index.
 * @param index  Result index (0-based).
 * @return Pointer to dk_fi_result_t, or NULL if out of range.
 */
const dk_fi_result_t* dk_fi_get_result(uint32_t index);

/**
 * @brief Print all results to console (test debug output).
 */
void dk_fi_print_results(void);

/* ========================================================================
 *  ICCE Protocol Fault Injectors
 * ======================================================================== */

/** @brief Inject ICCE signature forgery test */
void dk_fi_icce_sign_forgery_test(void);

/** @brief Inject ICCE certificate expiry test */
void dk_fi_icce_cert_expired_test(void);

/** @brief Inject ICCE state machine illegal transition test */
void dk_fi_icce_illegal_transition_test(void);

/** @brief Inject ICCE communication timeout test */
void dk_fi_icce_comm_timeout_test(void);

/** @brief Inject ICCE distance spoofing test */
void dk_fi_icce_distance_spoof_test(void);

/* ========================================================================
 *  CCC Protocol Fault Injectors
 * ======================================================================== */

/** @brief Inject CCC secure channel failure test */
void dk_fi_ccc_secure_channel_fail_test(void);

/** @brief Inject CCC certificate verification anomaly test */
void dk_fi_ccc_cert_verify_anomaly_test(void);

/** @brief Inject CCC NFC OOB corruption test */
void dk_fi_ccc_nfc_oob_corrupt_test(void);

/** @brief Inject CCC BLE encryption failure test */
void dk_fi_ccc_ble_encrypt_fail_test(void);

/** @brief Inject CCC illegal state transition test */
void dk_fi_ccc_illegal_state_test(void);

/* ========================================================================
 *  ICCOA Protocol Fault Injectors
 * ======================================================================== */

/** @brief Inject ICCOA handshake failure test */
void dk_fi_iccoa_handshake_fail_test(void);

/** @brief Inject ICCOA key derivation error test */
void dk_fi_iccoa_key_derive_error_test(void);

/** @brief Inject ICCOA downgrade attack test */
void dk_fi_iccoa_downgrade_attack_test(void);

/** @brief Inject ICCOA permission bypass test */
void dk_fi_iccoa_permission_bypass_test(void);

/** @brief Inject ICCOA frame HMAC tamper test */
void dk_fi_iccoa_hmac_tamper_test(void);

/* ========================================================================
 *  Run all fault injection tests
 * ======================================================================== */

/** @brief Run all DK protocol fault injection tests */
void dk_fi_run_all_tests(void);

#else /* DK_FAULT_INJECT_ENABLE == 0 */

/* ── All stubs — compile to nothing in production ── */
static inline void dk_fi_init(void) {}
static inline void dk_fi_deinit(void) {}
static inline void dk_fi_enable(uint32_t id, bool en) { (void)id; (void)en; }
static inline bool dk_fi_is_active(uint32_t id) { (void)id; return false; }
static inline void dk_fi_clear_all(void) {}
static inline void dk_fi_record_result(const dk_fi_result_t *r) { (void)r; }
static inline uint32_t dk_fi_get_result_count(void) { return 0; }
static inline const dk_fi_result_t* dk_fi_get_result(uint32_t idx) { (void)idx; return NULL; }
static inline void dk_fi_print_results(void) {}

static inline void dk_fi_icce_sign_forgery_test(void) {}
static inline void dk_fi_icce_cert_expired_test(void) {}
static inline void dk_fi_icce_illegal_transition_test(void) {}
static inline void dk_fi_icce_comm_timeout_test(void) {}
static inline void dk_fi_icce_distance_spoof_test(void) {}

static inline void dk_fi_ccc_secure_channel_fail_test(void) {}
static inline void dk_fi_ccc_cert_verify_anomaly_test(void) {}
static inline void dk_fi_ccc_nfc_oob_corrupt_test(void) {}
static inline void dk_fi_ccc_ble_encrypt_fail_test(void) {}
static inline void dk_fi_ccc_illegal_state_test(void) {}

static inline void dk_fi_iccoa_handshake_fail_test(void) {}
static inline void dk_fi_iccoa_key_derive_error_test(void) {}
static inline void dk_fi_iccoa_downgrade_attack_test(void) {}
static inline void dk_fi_iccoa_permission_bypass_test(void) {}
static inline void dk_fi_iccoa_hmac_tamper_test(void) {}

static inline void dk_fi_run_all_tests(void) {}

#endif /* DK_FAULT_INJECT_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* DK_FAULT_INJECT_H */
