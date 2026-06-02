#!/bin/bash
# Build vehicle C-code test binaries
set -e
ROOT=/Users/stefan/.openclaw/workspace/yuleDKCS/embedded
CDIR=/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/tests
INCS="-include support/dk_logger.h -I support -I $ROOT/iccoa_protocol/include -I $ROOT/ccc_protocol/include -I $ROOT/icce_protocol/include -I $ROOT/unified_protocol/include -I $ROOT/system_architecture -I $ROOT/ccc_protocol/src/logger -I vendor/unity"
CFLAGS="-g -O0 -DBLE_SUPERVISION_TIMEOUT_MS=400"

cd "$CDIR"
mkdir -p build

echo "=== Compiling ICCOA real source files ==="
for f in \
  "$ROOT/iccoa_protocol/src/iccoa/iccoa_dk_core.c" \
  "$ROOT/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c" \
  "$ROOT/iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c" \
  "$ROOT/iccoa_protocol/src/ble/iccoa_ble.c" \
  "$ROOT/iccoa_protocol/src/auth/iccoa_auth.c" \
  "$ROOT/iccoa_protocol/src/service/iccoa_service.c"; do
  name=$(basename "$f" .c)
  cc -c $CFLAGS $INCS "$f" -o "build/${name}.o" && echo "  + ${name}.o"
done

echo ""
echo "=== Compiling Unity framework ==="
cc -c $CFLAGS $INCS vendor/unity/unity.c -o build/unity.o && echo "  + unity.o"

echo ""
echo "=== Compiling stubs ==="
cc -c $CFLAGS $INCS support/stubs.c -o build/stubs.o && echo "  + stubs.o"

echo ""
echo "=== Compiling test files ==="
for f in \
  test/test_iccoa_dk_core.c \
  test/test_iccoa_ble.c \
  test/test_ccc_dk_core.c \
  test/test_unified.c; do
  name=$(basename "$f" .c)
  cc -c $CFLAGS $INCS "$f" -o "build/${name}.o" && echo "  + ${name}.o"
done

echo ""
echo "=== Linking test binaries ==="
ICCOA_OBJS="build/iccoa_dk_core.o build/iccoa_dk30.o build/iccoa_dk40.o build/iccoa_ble.o build/iccoa_auth.o build/iccoa_service.o"
BASE_OBJS="build/stubs.o build/unity.o"

echo "  Linking test_iccoa_dk_core..."
cc -g -O0 build/test_iccoa_dk_core.o $ICCOA_OBJS $BASE_OBJS -o build/test_iccoa_dk_core -lm

echo "  Linking test_iccoa_ble..."
cc -g -O0 build/test_iccoa_ble.o $ICCOA_OBJS $BASE_OBJS -o build/test_iccoa_ble -lm

echo "  Linking test_ccc_dk_core..."
cc -g -O0 build/test_ccc_dk_core.o $BASE_OBJS -o build/test_ccc_dk_core -lm

echo "  Linking test_unified..."
cc -g -O0 build/test_unified.o $BASE_OBJS -o build/test_unified -lm

echo ""
echo "=== Build complete ==="
ls -lh build/test_iccoa_dk_core build/test_iccoa_ble build/test_ccc_dk_core build/test_unified 2>&1
