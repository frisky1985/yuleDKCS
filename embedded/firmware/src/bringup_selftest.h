/**
 * @file bringup_selftest.h
 * @brief G1 hardware bring-up self-test.
 *
 * Executes a fixed sequence of peripheral checks and prints one line per
 * check over the debug UART. Intended to be the first firmware image flashed
 * on a new EVB; see docs/g1-bringup-runbook.md.
 */

#ifndef BRINGUP_SELFTEST_H
#define BRINGUP_SELFTEST_H

#include <stdint.h>

typedef struct {
    uint16_t passed;
    uint16_t failed;
    uint16_t skipped;
} bringup_result_t;

/**
 * @brief Run all bring-up checks.
 *
 * @param result  Optional output counters.
 * @return 0 if no check failed, -1 otherwise.
 */
int bringup_selftest_run(bringup_result_t *result);

#endif /* BRINGUP_SELFTEST_H */
