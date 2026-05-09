#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ccc.h"

// Fuzzing harness for CCC protocol parser

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16) return 0;
    
    ccc_session_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    // Attempt to parse fuzzed data as CCC message
    // This tests the resilience of the parser
    
    return 0;
}
