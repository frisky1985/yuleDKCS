#include <stdint.h>
#include <stddef.h>
#include "dkcs.h"

// AFL++ fuzzing harness for DKCS unified API

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    
    session_config_t config;
    config.protocol = data[0] % 3;  // CCC, ICCE, ICCOA
    config.conn_type = data[1] % 2; // BLE, NFC
    
    // Test initialization with fuzzed config
    dkcs_init(&config);
    
    return 0;
}
