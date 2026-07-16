#ifndef OFFLINE_DECISION_H
#define OFFLINE_DECISION_H

#include <stdint.h>
#include <stdbool.h>

/* Decision Results */
#define DECISION_ALLOW              0
#define DECISION_DENY               -1
#define DECISION_REQUIRE_ONLINE     1
#define DECISION_CHALLENGE_REQUIRED 2

/* Decision Reasons */
#define REASON_SUCCESS              0
#define REASON_TIME_INVALID         1
#define REASON_RATE_LIMITED         2
#define REASON_KEY_NOT_FOUND        3
#define REASON_KEY_EXPIRED          4
#define REASON_PERMISSION_DENIED    5
#define REASON_SIGNATURE_INVALID    6
#define REASON_RISK_TOO_HIGH        7

/* Risk Levels */
#define RISK_LOW     0
#define RISK_MEDIUM  1
#define RISK_HIGH    2
#define RISK_CRITICAL 3

/* Key Cache Item */
typedef struct {
    uint32_t key_id;
    uint8_t status;
    uint8_t public_key[64];
    uint32_t expiry_time;
    uint32_t last_sync_time;
} key_cache_item_t;

/* Permission Cache Item */
typedef struct {
    uint32_t user_id;
    uint8_t permissions[32];
    uint32_t valid_from;
    uint32_t valid_until;
} permission_cache_item_t;

/* Risk Score */
typedef struct {
    uint32_t score;
    uint8_t level;
    uint8_t factors[4];
    float confidence;
} risk_score_t;

/* Decision Request */
typedef struct {
    uint32_t user_id;
    uint32_t key_id;
    uint8_t command_type;
    uint32_t timestamp;
    uint8_t nonce[16];
    uint8_t signature[64];
    int16_t rssi;
    uint32_t zone;
} decision_request_t;

/* Decision Output */
typedef struct {
    uint32_t decision_id;
    uint32_t user_id;
    uint32_t key_id;
    uint8_t command_type;
    int32_t result;
    uint8_t reason;
    uint32_t valid_duration;
    risk_score_t risk_score;
} decision_output_t;

/* Decision Rule */
typedef struct {
    uint32_t rule_id;
    uint8_t rule_type;
    int32_t action;
    uint8_t priority;
    uint8_t enabled;
} decision_rule_t;

/* API */
int32_t decision_init(void);
int32_t decision_evaluate(const decision_request_t *request, decision_output_t *output);
int32_t decision_add_rule(const decision_rule_t *rule);
int32_t decision_remove_rule(uint32_t rule_id);
int32_t decision_calculate_risk(const decision_request_t *request, risk_score_t *score);
int32_t decision_log(const decision_output_t *decision);
int32_t decision_get_history(uint32_t user_id, decision_output_t *history, uint32_t *count);

#endif /* OFFLINE_DECISION_H */
