/**
 * Unity - Minimalist Test Framework Implementation
 */
#include "unity.h"

Unity UnityFixture = {0};

/* setUp/tearDown are provided by each test file — not here */
__attribute__((weak)) void setUp(void) {}
__attribute__((weak)) void tearDown(void) {}

void _unity_fail(const char *msg, int line)
{
    UnityFixture.num_failures++;
    printf("  ✗ FAIL: %s (line %d) in test '%s'\n",
           msg, line, UnityFixture.current_test ? UnityFixture.current_test : "?");
    if (UnityFixture.abort_active) {
        longjmp(UnityFixture.abort_jmp, 1);
    }
}
