#!/bin/bash
# Compile all protocol source files with test stubs
set -e
cd /Users/stefan/.openclaw/workspace/yuleDKCS/embedded/tests
ROOT=/Users/stefan/.openclaw/workspace/yuleDKCS/embedded

INCS="-include support/dk_logger.h -I support -I $ROOT/iccoa_protocol/include -I $ROOT/ccc_protocol/include -I $ROOT/icce_protocol/include -I $ROOT/unified_protocol/include -I $ROOT/system_architecture -I $ROOT/ccc_protocol/src/logger -I vendor/unity"

cc -c -g -O0 $INCS $ROOT/iccoa_protocol/src/iccoa/iccoa_dk_core.c -o build/iccoa_dk_core.o
echo "+ iccoa_dk_core"

cc -c -g -O0 $INCS $ROOT/iccoa_protocol/src/iccoa/dk30/iccoa_dk30.c -o build/iccoa_dk30.o
echo "+ iccoa_dk30"

cc -c -g -O0 $INCS $ROOT/iccoa_protocol/src/iccoa/dk40/iccoa_dk40.c -o build/iccoa_dk40.o
echo "+ iccoa_dk40"

cc -c -g -O0 -DBLE_SUPERVISION_TIMEOUT_MS=400 $INCS $ROOT/iccoa_protocol/src/ble/iccoa_ble.c -o build/iccoa_ble.o
echo "+ iccoa_ble"

cc -c -g -O0 $INCS $ROOT/iccoa_protocol/src/auth/iccoa_auth.c -o build/iccoa_auth.o
echo "+ iccoa_auth"

cc -c -g -O0 $INCS $ROOT/iccoa_protocol/src/service/iccoa_service.c -o build/iccoa_service.o
echo "+ iccoa_service"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/core/ccc_dk_core.c -o build/ccc_dk_core.o
echo "+ ccc_dk_core"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/ble/ble_kw47a.c -o build/ble_kw47a.o
echo "+ ble_kw47a"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/nfc/nfc_st25r501.c -o build/nfc_st25r501.o
echo "+ nfc_st25r501"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/uwb/uwb_ncj29d6.c -o build/uwb_ncj29d6.o
echo "+ uwb_ncj29d6"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/security/security.c -o build/security.o
echo "+ security"

cc -c -g -O0 $INCS $ROOT/ccc_protocol/src/keymgmt/key_mgmt.c -o build/key_mgmt.o
echo "+ key_mgmt"

cc -c -g -O0 $INCS $ROOT/unified_protocol/src/dk_unified.c -o build/dk_unified.o
echo "+ dk_unified"

cc -c -g -O0 $INCS support/stubs.c -o build/stubs.o
echo "+ stubs"

echo "=== All source files compiled successfully ==="
