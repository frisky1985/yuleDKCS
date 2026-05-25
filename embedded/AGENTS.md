# Embedded Agent Instructions

## Commands

```bash
# Build
cd build
cmake ..
make -j$(nproc)

# Clean rebuild
rm -rf build && mkdir build && cd build && cmake .. && make

# Testing
make test                           # Run all tests
cmocka-test                       # Run CMocka unit tests

# Static Analysis
cppcheck --enable=all --error-exitcode=1 ../src

# Flash to device (requires J-Link)
JLinkExe -device KW47B42Z83xxxA -if SWD -speed 4000 -autoconnect 1 -CommanderScript flash.jlink
```

## Structure

```
embedded/
├── src/
│   ├── core/           # Core digital key logic
│   ├── ble/            # BLE protocol implementation
│   ├── uwb/            # UWB ranging implementation
│   ├── nfc/            # NFC protocol implementation
│   ├── crypto/         # Cryptographic operations
│   ├── storage/        # Secure storage
│   └── hal/            # Hardware abstraction
├── sdk/                # NXP SDK
├── tests/              # Unit tests (Unity/CMock)
├── include/            # Public headers
└── tools/              # Build/debug tools
```

## Patterns

### Module Interface
```c
// include/digital_key.h
#ifndef DIGITAL_KEY_H
#define DIGITAL_KEY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DK_OK = 0,
    DK_ERROR_INVALID_PARAM,
    DK_ERROR_NOT_INITIALIZED,
    DK_ERROR_CRYPTO_FAILURE,
} dk_error_t;

dk_error_t dk_init(const dk_config_t* config);
dk_error_t dk_pair_start(const uint8_t* phone_pubkey, size_t len);
dk_error_t dk_unlock(void);

#endif
```

### Implementation
```c
// src/core/digital_key.c
#include "digital_key.h"
#include "crypto_engine.h"

static dk_status_t g_status = DK_STATUS_UNINITIALIZED;
static dk_config_t g_config;

dk_error_t dk_init(const dk_config_t* config)
{
    if (config == NULL) {
        return DK_ERROR_INVALID_PARAM;
    }
    
    dk_error_t err = crypto_init(&config->crypto);
    if (err != DK_OK) {
        return err;
    }
    
    g_config = *config;
    g_status = DK_STATUS_INITIALIZED;
    return DK_OK;
}
```

## Testing

- Framework: Unity + CMock
- Mock hardware: CMock generates mocks for HAL
- Test file: `tests/test_module.c`
- Run: `make test`

## Safety & Compliance

- MISRA C:2012 compliance required
- No dynamic memory allocation
- Bounded loops (no infinite loops without exit)
- All paths must have explicit return/error handling
- Use `static` for internal linkage

## Memory Management

- Static allocation only
- Stack usage minimized
- No recursion
- Memory pools for objects

## Security

- Secrets in secure flash only
- Keys never leave secure boundary
- Anti-tamper detection
- Secure boot verification
