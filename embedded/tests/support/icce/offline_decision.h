/* offline_decision.h — Test stub for ICCE offline decision engine */
#ifndef OFFLINE_DECISION_H
#define OFFLINE_DECISION_H

#include <stdint.h>
#include <stdbool.h>

#define DECISION_OK                    0
#define DECISION_ERR_PARAM            -1
#define DECISION_ERR_NOT_FOUND        -2
#define DECISION_CHALLENGE_REQUIRED   -3

typedef struct {
    uint8_t action;
    uint8_t param;
    uint32_t expiry_ms;
} decision_result_t;

int32_t decision_init(void);
int32_t decision_evaluate(uint32_t key_id, int32_t distance_mm,
                          uint8_t zone, decision_result_t *out);

#endif /* OFFLINE_DECISION_H */
