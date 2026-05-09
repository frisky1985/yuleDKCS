#include <stdint.h>
#include <stddef.h>
#include "scp03.h"

// Fuzzing harness for SCP03 APDU processing

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;
    
    scp03_apdu_t apdu;
    scp03_session_t session;
    
    // Initialize minimal session
    memset(&session, 0, sizeof(session));
    session.security_level = SCP03_I_0C;
    
    // Try to unwrap fuzzed APDU
    // scp03_unwrap_apdu(&session, data, size, &apdu);
    
    return 0;
}
