# Makefile for yuleDKCS AFL++ Fuzzing Suite
# 
# Usage:
#   make              - Build all fuzzing targets
#   make fuzz_dkcs    - Build only DKCS fuzzer
#   make fuzz_ccc     - Build only CCC fuzzer
#   make fuzz_scp03   - Build only SCP03 fuzzer
#   make run          - Run fuzzing with AFL++
#   make clean        - Clean build artifacts
#   make test         - Test fuzzers with seed corpus
#
# Requirements:
#   - AFL++ (https://github.com/AFLplusplus/AFLplusplus)
#   - clang/llvm with fuzzer support

#==============================================================================
# Configuration
#==============================================================================
CC ?= afl-clang-fast
CFLAGS = -g -O2 -Wall -Wextra -I../../include -I.
LDFLAGS =

# AFL++ specific flags
AFL_FLAGS = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer

# Source files
DKCS_SRC = ../../src/common/dkcs.c
CCC_SRC = ../../src/ccc/protocol/ccc_core.c
ICCE_SRC = ../../src/icce/protocol/icce_core.c
ICCOA_SRC = ../../src/iccoa/protocol/iccoa_core.c
SCP03_SRC = ../../src/security/scp03/scp03_session.c \
            ../../src/security/scp03/scp03_crypto.c

ALL_SRC = $(DKCS_SRC) $(CCC_SRC) $(ICCE_SRC) $(ICCOA_SRC) $(SCP03_SRC)

# Fuzzing targets
TARGETS = fuzz_dkcs fuzz_ccc fuzz_scp03

# Directories
INPUT_DIR = inputs
OUTPUT_DIR = outputs
CRASH_DIR = crashes

#==============================================================================
# Default Target
#==============================================================================
.PHONY: all clean run test help

all: $(TARGETS)

#==============================================================================
# Individual Fuzzing Targets
#==============================================================================

fuzz_dkcs: fuzz_dkcs.c $(ALL_SRC)
	@echo "Building DKCS fuzzer..."
	$(CC) $(CFLAGS) $(AFL_FLAGS) -DAFL_COMPILER \
		-o $@ fuzz_dkcs.c $(ALL_SRC) $(LDFLAGS)

fuzz_ccc: fuzz_ccc.c $(CCC_SRC) $(DKCS_SRC)
	@echo "Building CCC fuzzer..."
	$(CC) $(CFLAGS) $(AFL_FLAGS) -DAFL_COMPILER \
		-o $@ fuzz_ccc.c $(CCC_SRC) $(DKCS_SRC) $(LDFLAGS)

fuzz_scp03: fuzz_scp03.c $(SCP03_SRC)
	@echo "Building SCP03 fuzzer..."
	$(CC) $(CFLAGS) $(AFL_FLAGS) -DAFL_COMPILER \
		-o $@ fuzz_scp03.c $(SCP03_SRC) $(LDFLAGS)

#==============================================================================
# Non-AFL Build (for testing without AFL)
#==============================================================================

no-afl: CC = clang
no-afl: CFLAGS += -fsanitize=fuzzer
no-afl: $(TARGETS)

#==============================================================================
# Fuzzing Execution
#==============================================================================

run: all
	@echo "Starting AFL++ fuzzing..."
	@mkdir -p $(OUTPUT_DIR)/dkcs $(OUTPUT_DIR)/ccc $(OUTPUT_DIR)/scp03
	@mkdir -p $(CRASH_DIR)
	@echo "Run the following commands in separate terminals:"
	@echo ""
	@echo "Terminal 1 (DKCS master):"
	@echo "  afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/dkcs -M dkcs_master -- ./fuzz_dkcs"
	@echo ""
	@echo "Terminal 2 (CCC slave):"
	@echo "  afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/ccc -S ccc_slave -- ./fuzz_ccc"
	@echo ""
	@echo "Terminal 3 (SCP03 slave):"
	@echo "  afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/scp03 -S scp03_slave -- ./fuzz_scp03"

# Run single fuzzer
run-dkcs: fuzz_dkcs
	@mkdir -p $(OUTPUT_DIR)/dkcs
	afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/dkcs -M dkcs_master -- ./fuzz_dkcs

run-ccc: fuzz_ccc
	@mkdir -p $(OUTPUT_DIR)/ccc
	afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/ccc -M ccc_master -- ./fuzz_ccc

run-scp03: fuzz_scp03
	@mkdir -p $(OUTPUT_DIR)/scp03
	afl-fuzz -i $(INPUT_DIR) -o $(OUTPUT_DIR)/scp03 -M scp03_master -- ./fuzz_scp03

#==============================================================================
# Testing
#==============================================================================

test: all
	@echo "Testing fuzz targets with seed corpus..."
	@for seed in $(INPUT_DIR)/*; do \
		echo "Testing with $$seed..."; \
		./fuzz_dkcs $$seed 2>/dev/null || true; \
		./fuzz_ccc $$seed 2>/dev/null || true; \
		./fuzz_scp03 $$seed 2>/dev/null || true; \
	done
	@echo "Seed corpus testing complete."

# Test individual targets
test-dkcs: fuzz_dkcs
	./fuzz_dkcs $(INPUT_DIR)/seed_pairing_request.bin || true

test-ccc: fuzz_ccc
	./fuzz_ccc $(INPUT_DIR)/seed_unlock_command.bin || true

test-scp03: fuzz_scp03
	./fuzz_scp03 $(INPUT_DIR)/seed_scp03_session.bin || true

#==============================================================================
# Corpus Minimization
#==============================================================================

minimize: all
	@echo "Minimizing corpus..."
	@mkdir -p $(INPUT_DIR)_min
	afl-cmin -i $(INPUT_DIR) -o $(INPUT_DIR)_min -- ./fuzz_dkcs
	afl-cmin -i $(INPUT_DIR) -o $(INPUT_DIR)_min -- ./fuzz_ccc
	afl-cmin -i $(INPUT_DIR) -o $(INPUT_DIR)_min -- ./fuzz_scp03
	@echo "Minimized corpus saved to $(INPUT_DIR)_min"

#==============================================================================
# Tmin (Test case minimization)
#==============================================================================

tmin:	@if [ -z "$(CRASH)" ]; then \
		echo "Usage: make tmin CRASH=path/to/crash/file"; \
		exit 1; \
	fi
	@echo "Minimizing crash test case: $(CRASH)"
	afl-tmin -i $(CRASH) -o $(CRASH).min -- ./fuzz_dkcs

#==============================================================================
# Coverage Analysis
#==============================================================================

coverage: CC = clang
coverage: CFLAGS += -fprofile-instr-generate -fcoverage-mapping
coverage: clean $(TARGETS)
	@echo "Coverage-enabled binaries built."
	@echo "Run: LLVM_PROFILE_FILE=fuzz.profraw ./fuzz_dkcs <input>"
	@echo "Then: llvm-profdata merge -sparse fuzz.profraw -o fuzz.profdata"
	@echo "Then: llvm-cov show ./fuzz_dkcs -instr-profile=fuzz.profdata"

#==============================================================================
# Static Analysis
#==============================================================================

analyze:
	@echo "Running static analysis..."
	@if command -v clang-static-analyzer >/dev/null 2>&1; then \
		scan-build make CC=clang CFLAGS="$(CFLAGS) --analyze"; \
	else \
		echo "clang-static-analyzer not found, skipping..."; \
	fi

#==============================================================================
# Cleaning
#==============================================================================

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGETS)
	rm -f *.o *.gcda *.gcno
	rm -rf $(OUTPUT_DIR)
	rm -f *.profraw *.profdata
	rm -f crash-* oom-* timeout-*

clean-all: clean
	@echo "Cleaning all artifacts including minimized corpus..."
	rm -rf $(INPUT_DIR)_min
	rm -rf $(CRASH_DIR)

#==============================================================================
# Help
#==============================================================================

help:
	@echo "yuleDKCS Fuzzing Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build all fuzzing targets (default)"
	@echo "  fuzz_dkcs   - Build DKCS unified API fuzzer"
	@echo "  fuzz_ccc    - Build CCC protocol fuzzer"
	@echo "  fuzz_scp03  - Build SCP03 APDU fuzzer"
	@echo "  no-afl      - Build with clang/libFuzzer (no AFL required)"
	@echo ""
	@echo "Running:"
	@echo "  run         - Show AFL++ run commands"
	@echo "  run-dkcs    - Run DKCS fuzzer"
	@echo "  run-ccc     - Run CCC fuzzer"
	@echo "  run-scp03   - Run SCP03 fuzzer"
	@echo ""
	@echo "Testing:"
	@echo "  test        - Test all fuzzers with seed corpus"
	@echo "  test-dkcs   - Test DKCS fuzzer"
	@echo "  test-ccc    - Test CCC fuzzer"
	@echo "  test-scp03  - Test SCP03 fuzzer"
	@echo ""
	@echo "Analysis:"
	@echo "  minimize    - Minimize seed corpus"
	@echo "  tmin        - Minimize a crash (CRASH=path)"
	@echo "  coverage    - Build with coverage instrumentation"
	@echo "  analyze     - Run static analysis"
	@echo ""
	@echo "Cleaning:"
	@echo "  clean       - Remove build artifacts"
	@echo "  clean-all   - Remove all artifacts"
