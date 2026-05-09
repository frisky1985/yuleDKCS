# yuleDKCS Fuzzing Suite

This directory contains AFL++ fuzzing harnesses for the yuleDKCS Digital Key Connectivity Stack.

## Overview

The fuzzing suite targets the following components:

| Fuzzer | Target | Description |
|--------|--------|-------------|
| `fuzz_dkcs` | Unified API | Tests DKCS high-level API functions |
| `fuzz_ccc` | CCC Protocol | Tests CCC Digital Key R3 protocol parsing |
| `fuzz_scp03` | SCP03 APDU | Tests SCP03 secure channel operations |

## Requirements

- **AFL++** (AFLplusplus): https://github.com/AFLplusplus/AFLplusplus
- **clang/llvm** with fuzzer support

### Installing AFL++

```bash
# Ubuntu/Debian
sudo apt-get install afl++

# Or build from source
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make
sudo make install
```

## Building

### Using Makefile (Recommended)

```bash
cd tests/fuzzing
make
```

### Using CMake

```bash
cd /home/admin/yuleDKCS
mkdir build && cd build
cmake .. -DENABLE_FUZZING=ON
make fuzz_dkcs fuzz_ccc fuzz_scp03
```

### Build without AFL (for testing)

```bash
cd tests/fuzzing
make no-afl
```

## Running Fuzzing

### Single Fuzzer

```bash
# DKCS fuzzer
cd tests/fuzzing
mkdir -p outputs/dkcs
afl-fuzz -i inputs -o outputs/dkcs -M dkcs_master -- ./fuzz_dkcs
```

### Parallel Fuzzing

Run in separate terminals:

```bash
# Terminal 1 - Master
afl-fuzz -i inputs -o outputs/dkcs -M dkcs_master -- ./fuzz_dkcs

# Terminal 2 - Slave
afl-fuzz -i inputs -o outputs/dkcs -S dkcs_slave1 -- ./fuzz_dkcs

# Terminal 3 - Slave
afl-fuzz -i inputs -o outputs/dkcs -S dkcs_slave2 -- ./fuzz_dkcs
```

## Fuzzing Targets

### 1. fuzz_dkcs

Tests the unified DKCS API:
- `dkcs_key_import()` - Import with fuzzed key data
- `dkcs_key_export()` - Export with fuzzed key IDs
- `dkcs_key_share()` - Share with arbitrary permissions
- `dkcs_pairing_start()` - Pairing with fuzzed VIN
- `dkcs_vehicle_unlock()` - Unlock with fuzzed credentials

### 2. fuzz_ccc

Tests CCC protocol implementation:
- Pairing message parsing
- Certificate chain validation
- Session establishment
- Message encryption/decryption
- Vehicle command processing

### 3. fuzz_scp03

Tests SCP03 secure channel:
- Session initialization with fuzzed keys
- APDU wrap/unwrap operations
- MAC calculation/verification
- Key derivation

## Seed Corpus

Seed inputs are in `inputs/` directory:
- `seed_pairing_request.bin` - Valid pairing request
- `seed_unlock_command.bin` - Valid unlock command
- `seed_random.bin` - Random bytes for general testing
- `seed_scp03_session.bin` - SCP03 session data

## Detected Issues

The fuzzers are configured to detect:

1. **Crashes** (SIGSEGV, SIGILL, SIGABRT)
2. **Memory leaks** (via ASan)
3. **Buffer overflows** (via ASan)
4. **Use-after-free** (via ASan)
5. **Undefined behavior** (via UBSan)
6. **Integer overflows**
7. **Infinite loops** (via timeout detection)

## Minimizing Crashes

If you find a crash, minimize it:

```bash
# Minimize crash test case
afl-tmin -i crash_file -o crash_min -- ./fuzz_dkcs

# Minimize entire corpus
make minimize
```

## Coverage Analysis

```bash
# Build with coverage
make coverage

# Run and generate coverage report
LLVM_PROFILE_FILE=fuzz.profraw ./fuzz_dkcs inputs/seed_random.bin
llvm-profdata merge -sparse fuzz.profraw -o fuzz.profdata
llvm-cov show ./fuzz_dkcs -instr-profile=fuzz.profdata
```

## Integration with CI/CD

Run a quick fuzzing test in CI:

```bash
# Test with seed corpus for a short time
make test

# Or run limited fuzzing
afl-fuzz -i inputs -o outputs -V 60 -- ./fuzz_dkcs
```

## Security Notes

- The fuzzers use **mock SE interfaces** - no real secure element is needed
- All cryptographic operations use placeholder implementations
- Fuzzing does not test actual security properties, only robustness
- Found crashes should be analyzed for security impact

## Troubleshooting

### "afl-clang-fast not found"

```bash
# Set path to AFL compiler
export PATH=$PATH:/path/to/AFLplusplus
# Or use clang directly
make CC=clang CFLAGS="-g -O2 -fsanitize=fuzzer"
```

### "No instrumentation detected"

Make sure to compile with AFL++ compiler, not system clang/gcc.

### Core dumps not working

```bash
echo core | sudo tee /proc/sys/kernel/core_pattern
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```

## References

- [AFL++ Documentation](https://aflplus.plus/docs/)
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [CCC Digital Key R3 Specification](https://carconnectivity.org/)
- [GlobalPlatform SCP03 Specification](https://globalplatform.org/)
