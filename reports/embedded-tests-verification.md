# 嵌入式 C 测试验证报告

**日期**: 2026-07-16 02:22:01
**编译器**: gcc (/usr/bin/gcc)
**运行模式**: native

## ✅ 所有测试通过

| 测试套件 | 用例数 | 结果 |
|----------|--------|------|
| iccoa_core | 12 | ✅ 通过 |
| iccoa_ble | 5 | ✅ 通过 |
| ccc | 18 | ✅ 通过 |
| unified | 12 | ✅ 通过 |

## 测试用例清单

### ICCOA 数字钥匙核心 (12 用例)
```
  - test_iccoa_init
  - test_iccoa_auth_request
  - test_iccoa_auth_verify
  - test_iccoa_ble_adv
  - test_iccoa_ble_send_data
  - test_iccoa_ble_callback
  - test_iccoa_dk30_checksum
  - test_iccoa_dk30_response
  - test_iccoa_vehicle_control
  - test_iccoa_vehicle_status
  - test_iccoa_service_dk_init
  - test_iccoa_dk40_init
```

### ICCOA BLE (5 用例)
```
  - test_ble_init_adv
  - test_dk_init_ble
  - test_ble_send
  - test_ble_callback
  - test_ble_lifecycle
```

### CCC 数字钥匙核心 (18 用例)
```
  - test_ccc_init
  - test_ccc_initial_state
  - test_ccc_run_cycle
  - test_uwb_init
  - test_uwb_create_session
  - test_uwb_zone_classification
  - test_nfc_init
  - test_nfc_field_detect
  - test_nfc_listen
  - test_security_init
  - test_se_key_store_load
  - test_ble_init_and_gatt
  - test_ble_connect_disconnect
  - test_key_management
  - test_key_sharing
  - test_uwb_threshold_and_callback
  - test_ccc_full_lifecycle
  - test_security_sign_verify
```

### 统一接口 (12 用例)
```
  - test_unified_init_smartphone
  - test_unified_init_vehicle_ce
  - test_unified_init_icce
  - test_unified_callback_registration
  - test_unified_ble_adv
  - test_unified_nfc_listen
  - test_unified_key_lifecycle
  - test_unified_vehicle_control
  - test_unified_location
  - test_unified_protocol_raw
  - test_unified_auth_flow
  - test_unified_run_tick
```

## 工具链要求

### Native (x86_64 / arm64)
- GCC 或 Clang (已安装: Apple clang version 21.0.0 (clang-2100.1.1.101))
- Unity 测试框架 (自动下载到 `tests/vendor/unity/`)
- ICCOA 协议源文件 + stubs.c 提供 CCC/Unified API (MCU 相关代码由 Stub 替代)

### ARM 交叉编译
- `arm-none-eabi-gcc` 工具链
- MCU SDK 头文件 (如 S32K312, KW47A, NCJ29D6 等)
- 需要启用在 Makefile 中注释的 CCC/ICCE/Unified 源文件编译规则

## 注意事项

1. **测试文件不自带 return value**: `run_*_tests()` 调用 `UNITY_END()` 但未返回其值。
   CI 脚本通过解析 Unity stdout 中的 "0 Failures" 字符串来判断通过/失败。
2. **CCC BLE/UWB/NFC/Security 源文件**: MCU 专用寄存器定义 (如 `KW47A_CS_PORT`) 在
   x86_64/arm64 上不可用。这些模块由 `tests/support/stubs.c` 中的 mock 实现替代。
3. **统一接口源文件**: 依赖 ICCE 头文件中非标准类型 (如 `sint32`)。
   Stub 实现 (`stubs.c`) 提供完整替代 API。
4. **编译日志**: 构建产物在 `embedded/tests/build/` 目录。
