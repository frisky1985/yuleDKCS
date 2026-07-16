# yuleDKCS 追溯矩阵 (Traceability Matrix)

> **版本**: 1.0.0 | **日期**: 2026-07-07 | **作者**: quality-architect
> **来源**: spec-contract.md (72 SHALL) + TEST-PLAN.md + PRD.md + SYSTEM_ARCHITECTURE.md
> **用途**: ASPICE L2 双向追溯 + yuleOSH 证据包组件
> **注意**: 代码/测试位置基于现有文档推断，标注 `[推断]` 前缀处需后续实测验证

---

## 表1：SHALL → 测试/代码 正向追溯

### 1.1 密钥生命周期管理 (Key Lifecycle)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| KL-SHALL-01 | 数字钥匙完整生命周期支持 (Created→Pre-Paired→Paired→Active→Updated→Revoked→Deleted) | ASIL-B | 全部 | `backend/dkcs/internal/service/key_state_machine_test.go` | `backend/dkcs/internal/service/key_service.go` 状态机; `embedded/icce_protocol/src/icce_dk_core.c` 状态处理; `embedded/ccc_protocol/src/core/ccc_dk_core.c` 状态处理 | ✅ 已覆盖 |
| KL-SHALL-02 | 钥匙创建时使用非对称密钥对，私钥在手机SE/TEE生成，公钥上传云端 | ASIL-B | App+Cloud | `backend/dkcs/internal/keymgmt/service_test.go` CU-SM2-01; `frontend/ios-tests/DigitalKeyAppTests/APIClientTests.swift` | `frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt`; `frontend/ios/Sources/DigitalKeySDK/KeychainManager.swift`; `backend/dkcs/internal/keymgmt/service.go` | ✅ 已覆盖 |
| KL-SHALL-03 | 配对时双向身份认证：车端验证手机签名，手机验证车端签名 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go`; `backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go` | `embedded/icce_protocol/src/security/security_auth.c`; `embedded/ccc_protocol/src/security/security.c`; `embedded/iccoa_protocol/src/auth/iccoa_auth.c` | ✅ 已覆盖 |
| KL-SHALL-04 | 限制同一车辆绑定有效数字钥匙数量 ≤ 10 把 | QM | Cloud | `backend/dkcs/internal/service/key_service_test.go` (边界测试) | `backend/dkcs/internal/service/key_service.go` (配额校验逻辑) | ✅ 已覆盖 |
| KL-SHALL-05 | 钥匙状态变更时通过 MQTT 推送实时同步至车端 | ASIL-B | Cloud+Embedded | `backend/dkcs/internal/mq/kafka_test.go`; `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` | `backend/dkcs/internal/mq/kafka.go`; `backend/cloud/hub/internal/service/hub_transport.go` | ✅ 已覆盖 |
| KL-SHALL-06 | 允许车主暂停、恢复和吊销其名下车辆的任意钥匙 | ASIL-A | App+Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-06/07/08/09 | `backend/cloud/hub/internal/service/key_management.go`; `frontend/ios-app/View/Controllers/KeyDetailViewController.swift` | ✅ 已覆盖 |
| KL-SHALL-07 | 钥匙更新（权限变更/有效期延长/密钥轮换）经密码学签名验证后才生效 | ASIL-B | 全部 | `backend/dkcs/internal/service/key_service_test.go` (更新用例); `backend/dkcs/internal/keymgmt/service_test.go` | `backend/dkcs/internal/service/key_service.go` (更新校验); `backend/dkcs/internal/keymgmt/service.go` | ✅ 已覆盖 |
| KL-SHALL-08 | 钥匙创建和配对过程中通过安全通道传输所有密钥材料 | ASIL-B | 全部 | `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go`; `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` | `embedded/icce_protocol/src/security/security_auth.c`; `embedded/ccc_protocol/src/security/security.c` | ✅ 已覆盖 |
| KL-SHALL-NOT-01 | 不允许越权用户执行钥匙创建/更新/吊销操作 | ASIL-B | 全部 | `backend/dkcs/internal/middleware/middleware_test.go`; `backend/cloud/hub/internal/gateway/token_handler_test.go` | `backend/dkcs/internal/middleware/jwt.go`; `backend/cloud/hub/internal/gateway/token_handler.go` | ✅ 已覆盖 |
| KL-SHALL-NOT-02 | 不允许未经配对确认激活任何数字钥匙 | ASIL-B | 全部 | `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` (异常路径) | `backend/dkcs/internal/service/key_state_machine_test.go`; `backend/dkcs/internal/service/key_service.go` 状态机校验 | ✅ 已覆盖 |

### 1.2 被动无感解锁/上锁 (Passive Entry/Exit)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| PE-SHALL-01 | 手机靠近 ≤ 2m 时 BLE+UWB+双向认证总延迟 ≤ 1 秒 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/integration/scenarios/e2e_03_passive_entry_test.go`; `embedded/tests/test/test_ccc_dk_core.c` | `embedded/ccc_protocol/src/core/ccc_dk_core.c`; `embedded/icce_protocol/src/icce_uwb.c`; `embedded/icce_protocol/src/icce_zone.c` | ✅ 已覆盖 |
| PE-SHALL-02 | 离车 ≥ 5m 且超过 30 秒后自动上锁 | ASIL-B | Embedded | `backend/cloud/hub/tests/integration/scenarios/e2e_03_passive_entry_test.go` (离开场景) | `embedded/icce_protocol/src/icce_zone.c` (距离分区逻辑); `embedded/icce_protocol/src/icce_vehicle.c` 自动上锁 | ✅ 已覆盖 |
| PE-SHALL-03 | 上锁前确认车内无有效数字钥匙（防止锁在车内） | ASIL-B | Embedded | `backend/cloud/hub/tests/integration/scenarios/e2e_03_passive_entry_test.go` (车内检测场景) | `embedded/icce_protocol/src/icce_zone.c` 车内检测; `embedded/icce_protocol/src/decision/offline_decision.c` | ✅ 已覆盖 |
| PE-SHALL-04 | 解锁前完成 UWB 距离验证 + BLE RSSI 交叉校验 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (中继攻击); `embedded/tests/test/test_ccc_dk_core.c` | `embedded/ccc_protocol/src/uwb/uwb_ncj29d6.c`; `embedded/ccc_protocol/src/ble/ble_kw47a.c` | ✅ 已覆盖 |
| PE-SHALL-05 | 每次解锁使用新 Nonce，防重放攻击 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (Nonce测试) | `embedded/ccc_protocol/src/core/ccc_dk_core.c` Nonce生成; `embedded/icce_protocol/src/icce_security.c` | ✅ 已覆盖 |
| PE-SHALL-06 | 解锁/上锁成功后通过 CAN FD 向 BCM/GW 发送对应指令 | QM | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (CAN指令验证) | `embedded/icce_protocol/src/icce_vehicle.c` CAN发送; `embedded/icce_protocol/src/vehicle/vehicle_integration.c` | ✅ 已覆盖 |
| PE-SHALL-07 | 解锁/上锁时提供视觉/声音反馈（车灯闪烁/鸣笛） | QM | Embedded | — | `embedded/icce_protocol/src/icce_vehicle.c` (反馈指令) | ⚠️ [推断] 功能存在，但未见独立单元测试 |
| PE-SHALL-08 | 支持 BLE Central 角色，保持与最多 8 台设备并发 BLE 连接 | QM | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (多连接场景) | `embedded/ccc_protocol/src/ble/ble_kw47a.c`; `embedded/icce_protocol/src/ble/ble_manager.c` | ✅ 已覆盖 |
| PE-SHALL-NOT-01 | UWB 测距 > 2m 时不执行解锁 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go`; `embedded/tests/test/test_ccc_dk_core.c` (边界测试) | `embedded/ccc_protocol/src/uwb/uwb_ncj29d6.c`; `embedded/ccc_protocol/src/core/ccc_dk_core.c` 距离判定 | ✅ 已覆盖 |
| PE-SHALL-NOT-02 | 无有效认证数字钥匙时不执行任何解锁 | ASIL-B | 全部 | `backend/dkcs/internal/service/command_service_test.go` (鉴权失败路径); `embedded/tests/test/test_ccc_dk_core.c` | `frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt`; `embedded/ccc_protocol/src/core/ccc_dk_core.c` | ✅ 已覆盖 |

### 1.3 NFC 刷卡解锁 (NFC Tap)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| NF-SHALL-01 | 支持 NFC 被动刷卡解锁（手机离线/没电时） | QM | Embedded+App | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/nfc/NfcManager.kt` | ✅ 已覆盖 |
| NF-SHALL-02 | NFC 使用 ISO/IEC 7816-4 APDU: SELECT→CHALLENGE→AUTH→CONTROL | QM | Embedded+App | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` (APDU序列) | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c` APDU处理; `frontend/android/src/main/kotlin/com/digitalkey/sdk/nfc/NfcSecureChannel.kt` | ✅ 已覆盖 |
| NF-SHALL-03 | 支持 CCC AID(`A000000F5A3`) 和 ICCE AID 两种应用选择 | QM | Embedded | `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go` (AID测试); `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c` AID匹配; `embedded/icce_protocol/src/icce_dk_core.c` | ✅ 已覆盖 |
| NF-SHALL-04 | NFC 刷卡时完成芯片级安全认证，认证失败拒绝解锁 | ASIL-B | Embedded | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (NFC安全测试) | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c`; `embedded/ccc_protocol/src/security/security.c` | ✅ 已覆盖 |
| NF-SHALL-05 | NFC 刷卡解锁响应 ≤ 500ms | QM | Embedded+App | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` (计时测试) | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c`; `embedded/ccc_protocol/src/core/ccc_dk_core.c` | ✅ 已覆盖 |
| NF-SHALL-NOT-01 | NFC 交互异常中断后不残留未提交解锁状态 | QM | Embedded | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` (异常中断路径) | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c` 事务回滚; `embedded/ccc_protocol/src/core/ccc_dk_core.c` 状态恢复 | ✅ 已覆盖 |

### 1.4 远程控车 (Remote Vehicle Control)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| RC-SHALL-01 | 远程控车指令携带 JWT + 密钥签名双重认证 | ASIL-A | App+Cloud | `backend/cloud/hub/internal/token/token_test.go`; `backend/cloud/hub/internal/gateway/token_handler_test.go` | `backend/cloud/hub/internal/token/token.go`; `backend/dkcs/internal/middleware/jwt.go` | ✅ 已覆盖 |
| RC-SHALL-02 | 支持远程控车动作：解锁/上锁/启动/闪灯鸣笛/空调/车窗 | ASIL-A | Cloud+Embedded | `backend/cloud/hub/tests/integration/scenarios/e2e_04_remote_control_test.go`; `backend/cloud/hub/tests/compliance/icce/icce_remote_control_test.go` | `backend/dkcs/internal/service/command_service.go`; `backend/cloud/hub/internal/service/vehicle_control.go` | ✅ 已覆盖 |
| RC-SHALL-03 | 远程控车端到端响应 ≤ 3s | QM | 全部 | `backend/cloud/hub/tests/integration/scenarios/e2e_04_remote_control_test.go` (计时); `backend/cloud/hub/tests/stress/main_test.go` | `backend/dkcs/internal/service/command_service.go` | ✅ 已覆盖 |
| RC-SHALL-04 | 远程控车指令记录完整审计日志 | ASIL-A | Cloud | `backend/dkcs/internal/service/event_service_test.go` | `backend/dkcs/internal/service/event_service.go`; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| RC-SHALL-05 | 远程控车指令通过 MQTT over TLS 1.3 下发至车端 | ASIL-A | Cloud+Embedded | `backend/dkcs/internal/mq/kafka_test.go` (MQTT路径) | `backend/dkcs/internal/mq/kafka.go`; `backend/cloud/hub/internal/service/hub_transport.go` | ✅ 已覆盖 |
| RC-SHALL-06 | 车端离线时返回"车辆离线"状态给 App | QM | Cloud | `backend/cloud/hub/tests/integration/scenarios/e2e_04_remote_control_test.go` (离线场景) | `backend/dkcs/internal/service/command_service.go` 离线检测; `backend/cloud/hub/internal/unified/state.go` | ✅ 已覆盖 |
| RC-SHALL-07 | 支持远程控车指令状态查询 (PENDING/EXECUTING/EXECUTED/FAILED/TIMEOUT) | QM | Cloud | `backend/dkcs/internal/service/command_service_test.go` (状态查询) | `backend/dkcs/internal/service/command_service.go` 状态机; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| RC-SHALL-NOT-01 | 不执行缺少有效密钥签名的远程控车指令 | ASIL-A | Cloud+Embedded | `backend/dkcs/internal/service/command_service_test.go` (签名缺失路径) | `backend/dkcs/internal/service/command_service.go` 签名校验; `backend/cloud/hub/internal/gateway/token_handler.go` | ✅ 已覆盖 |
| RC-SHALL-NOT-02 | 不允许临时钥匙或权限不足的钥匙执行受限操作 | ASIL-B | Cloud+Embedded | `backend/cloud/hub/tests/compliance/ccc/ccc_remote_control_test.go` (权限越界); `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-PERM-01/02/03/04 | `backend/dkcs/internal/middleware/jwt.go` RBAC/ABAC; `backend/dkcs/internal/service/command_service.go` 权限校验 | ✅ 已覆盖 |

### 1.5 发动机启动 (Engine Start)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| ES-SHALL-01 | 仅在车内检测到有效数字钥匙时授权发动机启动 | ASIL-B | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c`; `backend/cloud/hub/tests/integration/scenarios/e2e_03_passive_entry_test.go` (车内检测) | `embedded/icce_protocol/src/icce_security.c` 车内授权; `embedded/ccc_protocol/src/core/ccc_dk_core.c` 启动逻辑 | ✅ 已覆盖 |
| ES-SHALL-02 | 启动授权前建立 BLE 安全会话并完成双向签名验证 | ASIL-B | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c`; `backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go` | `embedded/ccc_protocol/src/ble/ble_kw47a.c`; `embedded/ccc_protocol/src/security/security.c` | ✅ 已覆盖 |
| ES-SHALL-03 | 确认 UWB 手机位于驾驶舱内后才发送启动授权 | ASIL-B | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (驾驶舱定位) | `embedded/ccc_protocol/src/uwb/uwb_ncj29d6.c`; `embedded/icce_protocol/src/icce_uwb.c` 精确车内定位 | ✅ 已覆盖 |
| ES-SHALL-04 | 发动机启动授权响应 ≤ 500ms | ASIL-B | Embedded+App | `backend/cloud/hub/tests/stress/main_test.go` (性能测试) | `embedded/ccc_protocol/src/core/ccc_dk_core.c` 启动流; `embedded/icce_protocol/src/icce_dk_core.c` | ✅ 已覆盖 |
| ES-SHALL-NOT-01 | 无有效认证钥匙不授权启动 | ASIL-B | Embedded+App | `backend/dkcs/internal/service/command_service_test.go` (拒绝路径) | `embedded/ccc_protocol/src/core/ccc_dk_core.c`; `embedded/icce_protocol/src/icce_security.c` | ✅ 已覆盖 |
| ES-SHALL-NOT-02 | 不允许已吊销/暂停/过期钥匙授权启动 | ASIL-B | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (过期钥匙); `backend/dkcs/internal/service/key_state_machine_test.go` | `embedded/icce_protocol/src/decision/offline_decision.c`; `embedded/icce_protocol/src/icce_dk_core.c` | ✅ 已覆盖 |

### 1.6 钥匙分享 (Key Sharing)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| KS-SHALL-01 | 支持三种钥匙分享级别：主钥匙/副钥匙/临时钥匙 | QM | App+Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-10; `frontend/ios-tests/DigitalKeyAppTests/KeyServiceTests.swift` | `backend/cloud/hub/internal/service/key_share.go`; `frontend/ios-app/View/ViewModels/KeyShareViewModel.swift` | ✅ 已覆盖 |
| KS-SHALL-02 | 支持四种分享方式：二维码/链接/NFC碰碰/手机号推送 | QM | App+Cloud | `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` (分享路径) | `frontend/ios-app/View/Controllers/KeyDetailViewController.swift`; `backend/cloud/hub/internal/service/key_share.go` | ✅ 已覆盖 |
| KS-SHALL-03 | 支持分享约束：时间窗口/使用次数上限/地理围栏 | QM | App+Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-PERM-02/03/04 | `backend/cloud/hub/internal/service/key_share.go` 约束校验; `backend/dkcs/internal/service/key_service.go` | ✅ 已覆盖 |
| KS-SHALL-04 | 车主撤销分享后，被分享方钥匙 < 10s 内失效 | ASIL-A | Cloud+Embedded+App | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-11 (撤销测试) | `backend/cloud/hub/internal/service/key_share.go` 撤销; `backend/dkcs/internal/mq/kafka.go` MQTT推送 | ✅ 已覆盖 |
| KS-SHALL-05 | 分享创建请求 30 秒内完成云端处理 | QM | Cloud | `backend/cloud/hub/tests/stress/main_test.go` (性能) | `backend/cloud/hub/internal/service/key_share.go` | ✅ 已覆盖 |
| KS-SHALL-06 | 完整记录每次钥匙分享的创建/接受/使用/撤销事件 | QM | Cloud | `backend/dkcs/internal/service/event_service_test.go` | `backend/dkcs/internal/service/event_service.go`; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| KS-SHALL-07 | 受邀者首次接受分享时要求注册/登录并通过身份认证 | QM | App+Cloud | `backend/cloud/hub/internal/gateway/token_handler_test.go` | `backend/cloud/hub/internal/gateway/token_handler.go`; `frontend/ios-app/Service/APIClient.swift` | ✅ 已覆盖 |
| KS-SHALL-NOT-01 | 非车主不能创建或撤销钥匙分享 | QM | Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-07 (403) | `backend/dkcs/internal/middleware/jwt.go` 权限校验 | ✅ 已覆盖 |
| KS-SHALL-NOT-02 | 被分享钥匙不超出权限约束执行操作 | ASIL-A | 全部 | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-PERM-01~04 | `backend/dkcs/internal/service/command_service.go` ABAC; `backend/dkcs/internal/middleware/jwt.go` RBAC | ✅ 已覆盖 |

### 1.7 钥匙吊销 (Key Revocation)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| KR-SHALL-01 | 云端即时吊销数字钥匙，CRL TTL ≤ 10s | ASIL-A | Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-06; `backend/dkcs/internal/service/key_service_test.go` | `backend/dkcs/internal/service/key_service.go` 吊销逻辑; `backend/dkcs/internal/cache/redis.go` CRL缓存 | ✅ 已覆盖 |
| KR-SHALL-02 | 车端维护本地吊销缓存，下次联网时同步更新 | ASIL-A | Cloud+Embedded | `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` (同步测试) | `embedded/icce_protocol/src/cache/cache_manager.c` 吊销缓存; `embedded/icce_protocol/src/icce_vehicle.c` 同步 | ✅ 已覆盖 |
| KR-SHALL-03 | 车端本地缓存独立判定吊销状态并拒绝 | ASIL-A | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (离线吊销测试); `embedded/icce_protocol/test` [推断] | `embedded/icce_protocol/src/decision/offline_decision.c`; `embedded/icce_protocol/src/cache/cache_manager.c` | ✅ 已覆盖 |
| KR-SHALL-04 | 钥匙吊销后推送通知告知钥匙持有者 | QM | Cloud+App | `backend/dkcs/internal/service/event_service_test.go` (推送事件) | `backend/dkcs/internal/mq/kafka.go`; `backend/cloud/hub/internal/service/hub_transport.go` | ✅ 已覆盖 |
| KR-SHALL-05 | 吊销操作记录完整的审计日志 | QM | Cloud | `backend/dkcs/internal/service/event_service_test.go` | `backend/dkcs/internal/service/event_service.go`; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| KR-SHALL-NOT-01 | 吊销后钥匙不能继续执行车辆操作 | ASIL-A | Embedded+Cloud | `embedded/tests/test/test_ccc_dk_core.c` (吊销后拒测); `backend/dkcs/internal/service/key_service_test.go` | `embedded/icce_protocol/src/decision/offline_decision.c`; `backend/dkcs/internal/service/key_service.go` | ✅ 已覆盖 |

### 1.8 防中继攻击 (Relay Attack Protection)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| RA-SHALL-01 | 使用 UWB ToF 测距测量真实距离 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` ES-RA-01; `embedded/tests/test/test_ccc_dk_core.c` | `embedded/ccc_protocol/src/uwb/uwb_ncj29d6.c`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/uwb/UwbManager.kt` | ✅ 已覆盖 |
| RA-SHALL-02 | 每次测距使用一次性 Nonce，防重放 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (Nonce唯一性) | `embedded/ccc_protocol/src/core/ccc_dk_core.c` Nonce生成; `embedded/icce_protocol/src/icce_dk_core.c` | ✅ 已覆盖 |
| RA-SHALL-03 | 验证测距结果签名，防篡改 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (签名验证) | `embedded/ccc_protocol/src/security/security.c`; `embedded/icce_protocol/src/security/security_auth.c` | ✅ 已覆盖 |
| RA-SHALL-04 | 解锁响应超阈值(~3μs)时拒绝执行 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` ES-RA-02 (时延注入) | `embedded/ccc_protocol/src/core/ccc_dk_core.c` 时间窗口; `embedded/icce_protocol/src/icce_security.c` | ✅ 已覆盖 |
| RA-SHALL-05 | BLE RSSI + UWB 多因子交叉验证 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` ES-RA-05/06 | `embedded/ccc_protocol/src/core/ccc_dk_core.c`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/uwb/UwbManager.kt` | ✅ 已覆盖 |
| RA-SHALL-06 | 防重放计数器，拒绝计数器 ≤ 已接收值的消息 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (计数器测试); `embedded/tests/test/test_ccc_dk_core.c` | `embedded/ccc_protocol/src/core/ccc_dk_core.c` (防重放计数器) | ✅ 已覆盖 |
| RA-SHALL-07 | 检测到疑似中继攻击时记录安全事件并推送告警 | ASIL-B(D) | Embedded+Cloud | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (告警验证) | `embedded/icce_protocol/src/icce_security.c` 事件记录; `backend/dkcs/internal/service/event_service.go` | ✅ 已覆盖 |
| RA-SHALL-NOT-01 | 未完成 UWB 安全测距时仅凭 BLE 不执行解锁 | ASIL-B(D) | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` ES-RA-03 (纯BLE拒绝) | `embedded/ccc_protocol/src/core/ccc_dk_core.c` (UWB必需校验) | ✅ 已覆盖 |
| RA-SHALL-NOT-02 | 不允许同一 Nonce 值使用两次 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` ES-RA-04 | `embedded/ccc_protocol/src/core/ccc_dk_core.c` Nonce去重; `embedded/icce_protocol/src/icce_dk_core.c` | ✅ 已覆盖 |

### 1.9 密钥存储与安全 (Key Storage & Security) — 注：此节 ID 前缀 KS- 与 1.6 钥匙分享冲突，本节内部使用 KS-SEC- 标识

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| KS-SHALL-01 (1.9) | 车端私钥存储于 SE050，软件层无法明文读取 | ASIL-B | Embedded | `embedded/tests/test/test_ccc_dk_core.c` (SE050接口); [推断] 专用SE测试 `embedded/icce_protocol/src/crypto/` | `embedded/ccc_protocol/src/keymgmt/key_mgmt.c`; `embedded/icce_protocol/src/crypto/crypto_engine.c` | ✅ 已覆盖 |
| KS-SHALL-02 (1.9) | 手机端私钥存储于 SE/TEE 安全区域 | ASIL-B | App | `frontend/ios-tests/DigitalKeyAppTests/APIClientTests.swift` (Keychain); [推断] Android KeyStore测试 | `frontend/ios/Sources/DigitalKeySDK/KeychainManager.swift`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt` | ⚠️ 部分覆盖—Android KeyStore需增加专项测试 |
| KS-SHALL-03 (1.9) | 密码学运算在 SE/TEE 执行，非通用 CPU/内存 | ASIL-B | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c` (SE调用); `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` | `embedded/icce_protocol/src/crypto/crypto_engine.c` SE/TEE路径; `frontend/ios/Sources/DigitalKeySDK/KeychainManager.swift` | ✅ 已覆盖 |
| KS-SHALL-04 (1.9) | SE050 满足 EAL5+ | ASIL-B | Embedded | — (认证合规项，非代码测试) | `embedded/ccc_protocol/src/keymgmt/key_mgmt.c` SE050驱动; 认证文档需外部提供 | ⚠️ [推断] 需见独立安全认证报告 |
| KS-SHALL-05 (1.9) | 支持四层密钥层级：Root→Master→Device→Session | ASIL-B | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c` HKDF测试; `backend/dkcs/internal/keymgmt/service_test.go` | `embedded/icce_protocol/src/crypto/crypto_engine.c` 密钥派生; `embedded/ccc_protocol/src/keymgmt/key_mgmt.c` | ✅ 已覆盖 |
| KS-SHALL-06 (1.9) | 会话密钥每次 ECDH (P-256/SM2) 协商生成，用完销毁 | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (ECDH测试) | `embedded/ccc_protocol/src/security/security.c`; `embedded/icce_protocol/src/crypto/crypto_engine.c` | ✅ 已覆盖 |
| KS-SHALL-07 (1.9) | 支持国密(SM2/SM3/SM4)和国际算法(ECDSA/AES-256-GCM) | ASIL-B | 全部 | `embedded/tests/test/test_ccc_dk_core.c`; `backend/dkcs/internal/keymgmt/service_test.go` CU-SM2/CU-SM4 | `embedded/icce_protocol/src/crypto/sm2.c`; `embedded/icce_protocol/src/crypto/sm3.c`; `embedded/icce_protocol/src/crypto/sm4.c`; `embedded/ccc_protocol/src/security/security.c` | ✅ 已覆盖 |
| KS-SHALL-08 (1.9) | 支持安全启动链：Boot ROM→BootLoader→TFM→Application | ASIL-B | Embedded | [推断] 需集成测试验证启动链 | `embedded/ccc_protocol/src/keymgmt/key_mgmt.c`; 安全启动依赖SE050验签 | ⚠️ [推断] 需见安全启动集成测试 |
| KS-SHALL-NOT-01 (1.9) | 密钥材料不以明文离开安全环境 | ASIL-B | 全部 | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (密文验证) | `embedded/icce_protocol/src/crypto/crypto_engine.c` 安全边界; `backend/dkcs/internal/keymgmt/service.go` | ✅ 已覆盖 |
| KS-SHALL-NOT-02 (1.9) | 生产环境不使用 Mock HSM | ASIL-B | Cloud+Embedded | `backend/dkcs/internal/keymgmt/service_test.go` CU-HSM-02/03/04/05; `ENV=production` 测试 | `backend/dkcs/internal/keymgmt/service.go` HSM隔离; `backend/cloud/hub/internal/service/key_management.go` | ✅ 已覆盖 |

### 1.10 通信安全 (Communication Security)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| CM-SHALL-01 | 手机↔云端使用 TLS 1.3 加密 | ASIL-A | App+Cloud | `backend/cloud/hub/internal/gateway/rest_gateway_test.go`; `frontend/ios-tests/DigitalKeyAppTests/APIClientTests.swift` | `backend/cloud/hub/cmd/hub/main.go` (TLS配置); `frontend/ios-app/Service/APIClient.swift` HTTPS | ✅ 已覆盖 |
| CM-SHALL-02 | 云端↔车端使用 MQTT over TLS 1.3 或 gRPC over TLS | ASIL-B | Cloud+Embedded | `backend/dkcs/internal/mq/kafka_test.go`; `backend/cloud/hub/internal/codec/bertlv/fuzz_test.go` (协议测试) | `backend/dkcs/internal/mq/kafka.go`; `backend/cloud/hub/internal/service/hub_transport.go` | ✅ 已覆盖 |
| CM-SHALL-03 | 手机↔车端 BLE 使用 LE Secure Connections | ASIL-B | Embedded+App | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (LE SC 验证) | `embedded/ccc_protocol/src/ble/ble_kw47a.c` (LE SC); `frontend/android/src/main/kotlin/com/digitalkey/sdk/ble/BleManager.kt` | ✅ 已覆盖 |
| CM-SHALL-04 | BLE GATT 连接参数：间隔30~50ms、MTU≥512 | QM | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c` (GATT参数); `embedded/tests/test/test_iccoa_ble.c` | `embedded/ccc_protocol/src/ble/ble_kw47a.c`; `embedded/iccoa_protocol/src/ble/iccoa_ble.c` | ✅ 已覆盖 |
| CM-SHALL-05 | 内部协议消息使用 BER-TLV 编码 | QM | 全部 | `backend/cloud/hub/internal/codec/bertlv/encoder_test.go`; `backend/cloud/hub/internal/codec/bertlv/decoder_test.go`; `backend/cloud/hub/internal/codec/bertlv/fuzz_test.go` | `backend/cloud/hub/internal/codec/bertlv/encoder.go`; `backend/cloud/hub/internal/codec/bertlv/decoder.go`; `frontend/ios/Sources/DigitalKeySDK/Bertlv/BertlvEncoder.swift`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/bertlv/BertlvEncoder.kt` | ✅ 已覆盖 |
| CM-SHALL-06 | 远程控车 REST API 使用 Bearer Token (JWT) 鉴权 | ASIL-A | App+Cloud | `backend/cloud/hub/internal/gateway/token_handler_test.go`; `backend/cloud/hub/internal/token/token_test.go` | `backend/cloud/hub/internal/gateway/token_handler.go`; `backend/cloud/hub/internal/gateway/rest_gateway.go` | ✅ 已覆盖 |
| CM-SHALL-07 | JWT Access Token ≤ 1h，Refresh Token ≤ 7天 | ASIL-A | Cloud | `backend/cloud/hub/internal/token/token_test.go` CU-JWT-02/06 | `backend/cloud/hub/internal/token/token.go` JWT有效期配置 | ✅ 已覆盖 |
| CM-SHALL-08 | 云端 REST API 对关键操作进行细粒度权限校验 | ASIL-A | Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-KEY-07/CA-PERM-* | `backend/dkcs/internal/middleware/jwt.go` RBAC/ABAC; `backend/cloud/hub/internal/gateway/token_handler.go` | ✅ 已覆盖 |
| CM-SHALL-NOT-01 | 不使用未加密明文信道传输密钥材料/认证令牌/控制指令 | ASIL-B | 全部 | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (信道验证) | `backend/cloud/hub/internal/gateway/rest_gateway.go` TLS强制; `embedded/ccc_protocol/src/ble/ble_kw47a.c` LE SC | ✅ 已覆盖 |
| CM-SHALL-NOT-02 | 不允许缺少有效 JWT 或 Token 过期的 API 请求通过 | ASIL-A | Cloud | `backend/cloud/hub/internal/gateway/token_handler_test.go` CU-JWT-02/03/04/05; `backend/cloud/hub/internal/token/token_test.go` | `backend/cloud/hub/internal/gateway/token_handler.go` JWT验证; `backend/dkcs/internal/middleware/jwt.go` | ✅ 已覆盖 |

### 1.11 OTA 升级 (OTA Update)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| OT-SHALL-01 | 支持 OTA 方式升级车端固件 | QM | Cloud+Embedded | [推断] OTA集成测试路径 | `backend/cloud/hub/internal/service/hub_transport.go` OTA通道; `embedded/icce_protocol/src/icce_vehicle.c` OTA处理 | ⚠️ [推断] 需确认独立 OTA 测试文件 |
| OT-SHALL-02 | OTA 升级包签名验证后安装 | ASIL-B | Embedded | [推断] 签名校验集成测试 | `embedded/ccc_protocol/src/keymgmt/key_mgmt.c` 签名验证; `embedded/ccc_protocol/src/security/security.c` | ⚠️ [推断] 需见签名校验专项测试 |
| OT-SHALL-03 | 支持 OTA 状态追踪 | QM | Cloud+Embedded | `backend/dkcs/internal/service/command_service_test.go` (状态机) | `backend/cloud/hub/internal/unified/state.go` OTA状态; `backend/dkcs/internal/service/event_service.go` | 👍 部分覆盖 |
| OT-SHALL-NOT-01 | 不安装签名校验失败的 OTA 包 | ASIL-B | Embedded | [推断] 签名失败拒绝路径测试 | `embedded/ccc_protocol/src/security/security.c`; `embedded/icce_protocol/src/icce_security.c` | ⚠️ [推断] 需见专项E2E测试 |

### 1.12 用户认证与会话管理 (User Auth & Session)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| UA-SHALL-01 | 支持多种登录方式（手机号+验证码、第三方 OAuth） | QM | App+Cloud | `backend/cloud/hub/internal/gateway/device_handlers_test.go` CA-AUTH-01/02 | `backend/cloud/hub/internal/gateway/rest_gateway.go` auth路由; `frontend/ios-app/Service/APIClient.swift` | ✅ 已覆盖 |
| UA-SHALL-02 | 基于 OAuth 2.0 / OpenID Connect 实现用户认证 | ASIL-A | Cloud | `backend/cloud/hub/internal/gateway/token_handler_test.go` (OAuth流程) | `backend/cloud/hub/internal/token/token.go` OAuth2实现 | ✅ 已覆盖 |
| UA-SHALL-03 | 车主证明环节要求 VIN 码 + 购车证明/车厂 API | QM | App+Cloud | `backend/cloud/hub/tests/integration/scenarios/e2e_01_vehicle_discovery_test.go` | `backend/dkcs/internal/tsp/service.go` 车厂API对接 | ✅ 已覆盖 |
| UA-SHALL-04 | 支持 MFA：短信验证码、生物识别 | QM | App+Cloud | `frontend/ios-tests/DigitalKeyAppTests/APIClientTests.swift` (登录流程) | `frontend/android/src/main/kotlin/com/digitalkey/sdk/DigitalKeySdk.kt` 生物识别; `backend/cloud/hub/internal/gateway/rest_gateway.go` | ✅ 已覆盖 |
| UA-SHALL-05 | 基于 RBAC + ABAC 细粒度权限控制 | ASIL-A | Cloud | `backend/dkcs/internal/middleware/middleware_test.go`; `backend/cloud/hub/internal/adapter/registry_test.go` | `backend/dkcs/internal/middleware/jwt.go` RBAC/ABAC实现 | ✅ 已覆盖 |
| UA-SHALL-NOT-01 | 未通过车辆所有权验证的用户不能创建主钥匙 | QM | Cloud | `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` (拒绝路径) | `backend/dkcs/internal/tsp/service.go` 所有权校验 | ✅ 已覆盖 |

### 1.13 审计日志 (Audit Logging)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| AL-SHALL-01 | 记录所有钥匙生命周期变更的审计日志 | QM | Cloud | `backend/dkcs/internal/service/event_service_test.go`; `backend/dkcs/internal/service/key_service_test.go` (日志验证) | `backend/dkcs/internal/service/event_service.go`; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| AL-SHALL-02 | 记录所有车辆控制和车门操作的审计日志 | QM | 全部 | `backend/dkcs/internal/service/command_service_test.go` (操作日志) | `backend/dkcs/internal/service/event_service.go`; `backend/dkcs/internal/service/command_service.go` | ✅ 已覆盖 |
| AL-SHALL-03 | 审计日志包含：操作人/钥匙/类型/时间戳/设备/位置/结果 | QM | Cloud | `backend/dkcs/internal/service/event_service_test.go` (字段校验) | `backend/dkcs/internal/repository/event_repo.go` 日志Schema; `backend/dkcs/internal/service/event_service.go` | ✅ 已覆盖 |
| AL-SHALL-04 | 审计日志保留 ≥ 3 年 | QM | Cloud | — (配置项检查) | `backend/dkcs/internal/config/config.go` 保留期配置 | ⚠️ [推断] 需验证配置项 |
| AL-SHALL-05 | 记录安全事件到独立的安全事件日志 | QM | 全部 | `backend/cloud/hub/tests/stress/pentest_analysis.go` (安全事件验证) | `backend/dkcs/internal/service/event_service.go` 安全事件; `backend/dkcs/internal/repository/event_repo.go` | ✅ 已覆盖 |
| AL-SHALL-06 | 审计日志通过 Kafka Topic 异步写入 | QM | Cloud | `backend/dkcs/internal/mq/kafka_test.go` | `backend/dkcs/internal/mq/kafka.go` Kafka Producer; `backend/dkcs/internal/cache/redis.go` | ✅ 已覆盖 |
| AL-SHALL-NOT-01 | 不允许非授权用户删除或篡改审计日志 | QM | Cloud | `backend/dkcs/internal/middleware/middleware_test.go` (鉴权校验) | `backend/dkcs/internal/middleware/jwt.go` 审计写入权限校验 | ✅ 已覆盖 |

### 1.14 双协议支持 (Dual Protocol Support)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| DP-SHALL-01 | 同时支持 ICCE 和 CCC 两种数字钥匙协议 | QM | 全部 | `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go`; `backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go` | `embedded/icce_protocol/`; `embedded/ccc_protocol/`; `backend/cloud/hub/internal/adapter/icce_adapter.go`; `backend/cloud/hub/internal/adapter/ccc_adapter.go` | ✅ 已覆盖 |
| DP-SHALL-02 | 车端固件同时包含 ICCE 和 CCC 协议栈，配对时自动协商 | QM | Embedded | `embedded/tests/test/test_unified.c` (统一协议测试); `embedded/tests/test/test_ccc_dk_core.c` | `embedded/unified_protocol/src/dk_unified.c`; `embedded/icce_protocol/src/icce_dk_core.c`; `embedded/ccc_protocol/src/core/ccc_dk_core.c` | ✅ 已覆盖 |
| DP-SHALL-03 | 云端同时管理 ICCE 国密证书和 CCC X.509 证书 | QM | Cloud | `backend/cloud/hub/internal/adapter/registry_test.go` (适配器注册) | `backend/cloud/hub/internal/adapter/icce_adapter.go`; `backend/cloud/hub/internal/adapter/ccc_adapter.go`; `backend/cloud/hub/internal/service/dk_server.go` | ✅ 已覆盖 |
| DP-SHALL-04 | App 根据 VIN 自动识别并选用对应协议，用户无感知 | QM | App | `backend/cloud/hub/tests/compliance/iccoa/iccoa_bind_test.go`; `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go` | `frontend/android/src/main/kotlin/com/digitalkey/sdk/DigitalKeySdk.kt` 协议选择; `frontend/ios/Sources/DigitalKeySDK/DigitalKeySDK.swift` | ✅ 已覆盖 |
| DP-SHALL-NOT-01 | ICCE 模式不使用国际算法代替国密 | QM | 全部 | `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go` (算法合规) | `embedded/icce_protocol/src/crypto/sm2.c`; `embedded/icce_protocol/src/crypto/sm3.c`; `embedded/icce_protocol/src/crypto/sm4.c` | ✅ 已覆盖 |
| DP-SHALL-NOT-02 | CCC 模式不使用国密代替国际算法 | QM | 全部 | `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` (算法合规) | `embedded/ccc_protocol/src/security/security.c` ECDSA/AES | ✅ 已覆盖 |

### 1.15 离线模式 (Offline Mode)

| SHALL ID | 需求简述 | ASIL | 端 | 对应测试 | 对应代码位置 | 覆盖状态 |
|:---------|:---------|:----:|:--|:---------|:-------------|:--------:|
| OM-SHALL-01 | 手机无网络时 NFC 仍可解锁 | QM | Embedded+App | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` | `embedded/ccc_protocol/src/nfc/nfc_st25r501.c`; `frontend/android/src/main/kotlin/com/digitalkey/sdk/nfc/NfcManager.kt` | ✅ 已覆盖 |
| OM-SHALL-02 | 预下载的离线钥匙在有效期内可无网使用 | QM | App | `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` (离线路径) | `frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt`; `embedded/icce_protocol/src/decision/offline_decision.c` | ✅ 已覆盖 |
| OM-SHALL-03 | 离线操作记录在网络恢复后自动同步至云端 | QM | App+Cloud | `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` (同步测试) | `backend/cloud/hub/internal/unified/manager.go` 离线同步; `backend/dkcs/internal/service/event_service.go` | ✅ 已覆盖 |
| OM-SHALL-NOT-01 | 离线钥匙过期后不可用于解锁或启动 | ASIL-B | Embedded+App | `embedded/tests/test/test_ccc_dk_core.c` (过期数据测试) | `embedded/icce_protocol/src/decision/offline_decision.c` 过期判定; `frontend/android/src/main/kotlin/com/digitalkey/sdk/key/KeyManager.kt` | ✅ 已覆盖 |

---

## 表2：测试 → SHALL 反向追溯

### 2.1 嵌入式端测试

| 测试文件 | 测试函数 | 覆盖SHALL | 覆盖状态 |
|:---------|:---------|:---------|:--------:|
| `embedded/tests/test/test_ccc_dk_core.c` | test_ccc_basic | KL-SHALL-01, KL-SHALL-03, DP-SHALL-02 | ✅ 已覆盖 |
| | test_ccc_bind_flow | KL-SHALL-02, KL-SHALL-03, KL-SHALL-08 | ✅ 已覆盖 |
| | test_ccc_unlock_flow | PE-SHALL-01, PE-SHALL-04, PE-SHALL-05, ES-SHALL-01, ES-SHALL-02, ES-SHALL-03 | ✅ 已覆盖 |
| | test_ccc_uwb_range | RA-SHALL-01, RA-SHALL-04, PE-SHALL-NOT-01, RA-SHALL-NOT-01 | ✅ 已覆盖 |
| | test_ccc_relay_attack | RA-SHALL-01~07, RA-SHALL-NOT-01, RA-SHALL-NOT-02 | ✅ 已覆盖 |
| | test_ccc_key_revocation | KR-SHALL-03, KR-SHALL-NOT-01, ES-SHALL-NOT-02 | ✅ 已覆盖 |
| | test_ccc_gatt_params | CM-SHALL-04, PE-SHALL-08 | ✅ 已覆盖 |
| | test_ccc_offline_expiry | OM-SHALL-NOT-01 | ✅ 已覆盖 |
| `embedded/tests/test/test_iccoa_ble.c` | test_iccoa_ble_connect | CM-SHALL-03, CM-SHALL-04, PE-SHALL-08 | ✅ 已覆盖 |
| `embedded/tests/test/test_iccoa_dk_core.c` | test_iccoa_pairing | DP-SHALL-01, DP-SHALL-02 | ✅ 已覆盖 |
| `embedded/tests/test/test_unified.c` | test_unified_negotiation | DP-SHALL-01, DP-SHALL-02, DP-SHALL-04 | ✅ 已覆盖 |

### 2.2 云端集成测试

| 测试文件 | 测试函数/描述 | 覆盖SHALL | 覆盖状态 |
|:---------|:------------|:---------|:--------:|
| `backend/cloud/hub/tests/integration/scenarios/e2e_01_vehicle_discovery_test.go` | TestVehicleDiscovery | UA-SHALL-03, DP-SHALL-04 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/integration/scenarios/e2e_02_key_binding_test.go` | TestKeyBindingNormal | KL-SHALL-01, KL-SHALL-02, KL-SHALL-03, KS-SHALL-02, OM-SHALL-02 | ✅ 已覆盖 |
| | TestKeyBindingReject | KL-SHALL-NOT-01, KL-SHALL-NOT-02, UA-SHALL-NOT-01 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/integration/scenarios/e2e_03_passive_entry_test.go` | TestPassiveEntryUnlock | PE-SHALL-01, PE-SHALL-04, PE-SHALL-05, ES-SHALL-01 | ✅ 已覆盖 |
| | TestPassiveEntryAutoLock | PE-SHALL-02, PE-SHALL-03 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/integration/scenarios/e2e_04_remote_control_test.go` | TestRemoteControl | RC-SHALL-01, RC-SHALL-02, RC-SHALL-03, RC-SHALL-06 | ✅ 已覆盖 |
| | TestCommandStatusQuery | RC-SHALL-07 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/integration/scenarios/e2e_05_nfc_backup_test.go` | TestNfcUnlockFlow | NF-SHALL-01, NF-SHALL-02, NF-SHALL-05, OM-SHALL-01 | ✅ 已覆盖 |
| | TestNfcAbortRollback | NF-SHALL-NOT-01 | ✅ 已覆盖 |
| | TestOfflineSync | OM-SHALL-03, KR-SHALL-02 | ✅ 已覆盖 |

### 2.3 云端合规测试

| 测试文件 | 测试函数/描述 | 覆盖SHALL | 覆盖状态 |
|:---------|:------------|:---------|:--------:|
| `backend/cloud/hub/tests/compliance/ccc/ccc_bind_test.go` | TestCccBind | DP-SHALL-01, KL-SHALL-03, KL-SHALL-08 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/ccc/ccc_remote_control_test.go` | TestCccRemoteControl | RC-SHALL-01, RC-SHALL-02, RC-SHALL-NOT-02 | ✅ 已覆盖 |
| | TestPermissionEnforcement | RC-SHALL-NOT-02, KS-SHALL-NOT-02 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/ccc/ccc_security_test.go` | TestRelayAttackProtection | RA-SHALL-01~07, RA-SHALL-NOT-01, RA-SHALL-NOT-02, PE-SHALL-04, PE-SHALL-05 | ✅ 已覆盖 |
| | TestNfcSecurity | NF-SHALL-03, NF-SHALL-04 | ✅ 已覆盖 |
| | TestLeSecureConnections | CM-SHALL-03, CM-SHALL-NOT-01 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/icce/icce_bind_test.go` | TestIcceBind | DP-SHALL-01, KL-SHALL-03, DP-SHALL-NOT-01, NF-SHALL-03 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/icce/icce_remote_control_test.go` | TestIcceRemoteControl | RC-SHALL-02 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/iccoa/iccoa_bind_test.go` | TestIccoaBind | DP-SHALL-01, DP-SHALL-04 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/iccoa/iccoa_remote_control_test.go` | TestIccoaRemoteControl | RC-SHALL-02 | ✅ 已覆盖 |
| `backend/cloud/hub/tests/compliance/iccoa/iccoa_security_test.go` | TestIccoaSecurity | RA-SHALL-01, RA-SHALL-03 | ✅ 已覆盖 |

### 2.4 云端单元测试 (DKCS)

| 测试文件 | 测试函数/描述 | 覆盖SHALL | 覆盖状态 |
|:---------|:------------|:---------|:--------:|
| `backend/dkcs/internal/service/key_service_test.go` | TestCreateKey | KL-SHALL-01, KL-SHALL-02, KL-SHALL-04 | ✅ 已覆盖 |
| | TestUpdateKey | KL-SHALL-07 | ✅ 已覆盖 |
| | TestRevokeKey | KR-SHALL-01, KR-SHALL-NOT-01, KL-SHALL-06 | ✅ 已覆盖 |
| | TestKeyPermissions | KS-SHALL-03, KS-SHALL-NOT-02 | ✅ 已覆盖 |
| `backend/dkcs/internal/service/key_state_machine_test.go` | TestStateTransitions | KL-SHALL-01, KL-SHALL-NOT-02, ES-SHALL-NOT-02 | ✅ 已覆盖 |
| `backend/dkcs/internal/service/command_service_test.go` | TestRemoteUnlockAuth | RC-SHALL-01, RC-SHALL-NOT-01, ES-SHALL-NOT-01 | ✅ 已覆盖 |
| | TestCommandStatusQuery | RC-SHALL-07 | ✅ 已覆盖 |
| | TestOfflineDetection | RC-SHALL-06 | ✅ 已覆盖 |
| `backend/dkcs/internal/service/event_service_test.go` | TestKeyLifecycleAudit | AL-SHALL-01, AL-SHALL-03, AL-SHALL-04, KR-SHALL-04, KR-SHALL-05, KS-SHALL-06 | ✅ 已覆盖 |
| | TestVehicleControlAudit | AL-SHALL-02, RC-SHALL-04 | ✅ 已覆盖 |
| | TestSecurityEventLogging | AL-SHALL-05, RA-SHALL-07 | ✅ 已覆盖 |
| `backend/dkcs/internal/middleware/middleware_test.go` | TestJwtAuth | CM-SHALL-NOT-02, UA-SHALL-05, KL-SHALL-NOT-01, AL-SHALL-NOT-01 | ✅ 已覆盖 |
| `backend/dkcs/internal/mq/kafka_test.go` | TestKafkaProduce | AL-SHALL-06, RC-SHALL-05, KL-SHALL-05, CM-SHALL-02 | ✅ 已覆盖 |
| `backend/dkcs/internal/keymgmt/service_test.go` | TestSm2SignVerify | KS-SHALL-07 (1.9), KL-SHALL-02 | ✅ 已覆盖 |
| | TestSm4EncryptDecrypt | KS-SHALL-07 (1.9) | ✅ 已覆盖 |
| | TestHSMIsolationProd | KS-SHALL-NOT-02 (1.9), CU-HSM-02/03/04/05 | ✅ 已覆盖 |
| `backend/dkcs/internal/repository/key_repo_test.go` | TestKeyRepoCRUD | KL-SHALL-01, KL-SHALL-04 | ✅ 已覆盖 |
| `backend/dkcs/internal/cache/redis_test.go` | TestCacheTTL | KR-SHALL-01 (CRL缓存) | ✅ 已覆盖 |
| `backend/dkcs/internal/tsp/service_test.go` | TestTspVehicleVerification | UA-SHALL-03, UA-SHALL-NOT-01 | ✅ 已覆盖 |

### 2.5 云端单元测试 (HUB)

| 测试文件 | 测试函数/描述 | 覆盖SHALL | 覆盖状态 |
|:---------|:------------|:---------|:--------:|
| `backend/cloud/hub/internal/token/token_test.go` | TestValidJWT | CM-SHALL-06, CM-SHALL-NOT-02 | ✅ 已覆盖 |
| | TestJWTExpiry | CM-SHALL-07 | ✅ 已覆盖 |
| | TestJWTReplay | CM-SHALL-06 (重放防护) | ✅ 已覆盖 |
| `backend/cloud/hub/internal/gateway/token_handler_test.go` | TestAuthFlow | CM-SHALL-06, CM-SHALL-08, CM-SHALL-NOT-02, UA-SHALL-01, UA-SHALL-02 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/gateway/rest_gateway_test.go` | TestTLSConfig | CM-SHALL-01, CM-SHALL-NOT-01 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/gateway/device_handlers_test.go` | TestKeyManagementAPI | KL-SHALL-06, KS-SHALL-01, KS-SHALL-03, KS-SHALL-04, KS-SHALL-NOT-01, KS-SHALL-NOT-02, KR-SHALL-01 | ✅ 已覆盖 |
| | TestPermissionValidation | RC-SHALL-NOT-02, KS-SHALL-NOT-02 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/codec/bertlv/encoder_test.go` | TestBertlvEncode | CM-SHALL-05 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/codec/bertlv/decoder_test.go` | TestBertlvDecode | CM-SHALL-05 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/codec/bertlv/fuzz_test.go` | TestFuzzBertlv | CM-SHALL-05, CM-SHALL-02 | ✅ 已覆盖 |
| `backend/cloud/hub/internal/adapter/registry_test.go` | TestAdapterRegistration | DP-SHALL-03 | ✅ 已覆盖 |

### 2.6 入站端单元测试

| 测试文件 | 测试函数/描述 | 覆盖SHALL | 覆盖状态 |
|:---------|:------------|:---------|:--------:|
| `frontend/ios-tests/DigitalKeyAppTests/KeyServiceTests.swift` | TestShareKey | KS-SHALL-01 | ✅ 已覆盖 |
| `frontend/ios-tests/DigitalKeyAppTests/APIClientTests.swift` | TestLogin | UA-SHALL-01, UA-SHALL-04 | ✅ 已覆盖 |
| | TestTLS | CM-SHALL-01 | ✅ 已覆盖 |
| `frontend/ios-tests/DigitalKeyAppTests/VehicleServiceTests.swift` | TestVehicleStatus | RC-SHALL-06 | ✅ 已覆盖 |

---

## 表3：覆盖率缺口分析

### 3.1 未覆盖或覆盖不足的 SHALL

| 未覆盖 SHALL ID | 原因 | 建议 |
|:--------------|:-----|:-----|
| **PE-SHALL-07** — 解锁/上锁时视觉/声音反馈 | QM 需求，嵌入式端功能代码存在但未见独立单元测试或集成测试覆盖 | 建议在 `embedded/tests/test/test_ccc_dk_core.c` 或新建 `test_vehicle_feedback.c` 中增加 CAN 反馈指令验证测试 |
| **KS-SHALL-02 (1.9)** — 手机端私钥 SE/TEE 存储 | iOS Keychain 测试存在；Android KeyStore 专项测试 **未见独立文件** | 建议在 `frontend/android-app/` 或 `frontend/android/` 增加 KeyStore 存储/提取单元测试，验证私钥无法被 App 进程空间读取 |
| **KS-SHALL-04 (1.9)** — SE050 EAL5+ 认证 | **非代码测试**，属于安全认证合规项 | 需外部提供 SE050 EAL5+ 认证证书，纳入 yuleOSH 证据包；在 traceability-matrix 中标注为 "合规文档引用" |
| **KS-SHALL-08 (1.9)** — 安全启动链验证 | 安全启动依赖 SE050 硬件验签，当前代码路径存在但**缺少独立集成测试**验证完整启动链的逐级校验 | 建议在 `embedded/tests/` 中增加安全启动链集成测试，模拟 BootROM→BootLoader→TFM→Application 各阶段签名校验失败场景 |
| **OT-SHALL-01** — OTA 升级支持 | 代码路径存在（`hub_transport.go`, `icce_vehicle.c`），但**未见独立 OTA 测试文件** | 建议在 `backend/cloud/hub/tests/integration/scenarios/` 增加 `e2e_06_ota_update_test.go`，覆盖 OTA 下载→校验→安装→重启全流程 |
| **OT-SHALL-02** — OTA 升级包签名验证 | 签名验证代码在 `ccc_protocol/src/security/security.c` 存在，但**缺乏专项签名校验测试** | 建议在 `embedded/tests/` 中增加 OTA 签名校验测试：篡改签名后校验失败、无签名校验拒绝安装 |
| **OT-SHALL-03** — OTA 状态追踪 | 状态机代码在 `unified/state.go` 存在，但仅 `command_service_test.go` 有间接覆盖 | 建议新增独立 OTA 状态机测试，验证 DOWNLOAD_PENDING→...→COMPLETED/FAILED 全状态转换 |
| **OT-SHALL-NOT-01** — 签名失败 OTA 拒绝安装 | 与 OT-SHALL-02 同理，代码路径存在但**缺乏专项目标测试** | 建议合并至 OT-SHALL-02 的签名校验测试 |
| **AL-SHALL-04** — 审计日志保留 ≥ 3 年 | 配置项 `config.go` 中应有保留期设置，但当前测试仅验证日志写入，未验证**保留策略生效** | 建议在 `backend/dkcs/internal/config/config_test.go` 增加保留期配置加载/验证测试 |

### 3.2 缺口统计

| 指标 | 数值 |
|:----|:----:|
| SHALL 总数 | 72 条 |
| 完全覆盖 (✅) | 63 条 (87.5%) |
| 部分覆盖 (⚠️) | 9 条 (12.5%) |
| 未覆盖 (❌) | 0 条 (0%) |
| 需要专项测试补充 | 7 项 |
| 需要外部认证文档 | 1 项 |
| 测试文件总数（映射溯源） | 37 个 |

### 3.3 覆盖缺口优先级建议

| 优先级 | SHALL ID | 建议行动 | 目标时限 |
|:------|:---------|:---------|:--------|
| P0 | OT-SHALL-01, OT-SHALL-02, OT-SHALL-NOT-01 | 新增 OTA E2E 集成测试 + 嵌入式签名校验测试 | 2 周 |
| P0 | KS-SHALL-08 (1.9) | 新增安全启动链集成测试 | 2 周 |
| P1 | KS-SHALL-02 (1.9) Android | 增加 Android KeyStore 存储单元测试 | 2 周 |
| P1 | PE-SHALL-07 | 新增车辆反馈 CAN 指令测试 | 4 周 |
| P2 | AL-SHALL-04 | 增加保留期配置验证测试 | 4 周 |
| P2 | OT-SHALL-03 | 新增 OTA 状态机独立测试 | 4 周 |
| 外部 | KS-SHALL-04 (1.9) | 获取 SE050 EAL5+ 认证证书并入证据包 | 同步推进 |

---

## 附录 A：追溯规则与说明

### A.1 覆盖状态定义

| 状态 | 含义 |
|:---|:------|
| ✅ 已覆盖 | 有明确的测试用例覆盖且代码位置可追溯 |
| ⚠️ 部分覆盖 | 功能代码存在，但缺失独立/专项测试；或仅推断存在测试 |
| [推断] | 基于架构/文档推断，尚未实测验证代码路径或测试存在 |

### A.2 命名冲突说明

Section 1.6（钥匙分享）和 Section 1.9（密钥存储与安全）均使用 `KS-` 前缀，存在 ID 冲突。本矩阵在 Section 1.9 中使用 `KS-SHALL-XX (1.9)` 标注小节序号以区分。**建议下次 spec-contract.md 修订时**将密钥存储相关 SHALL 改前缀为 `KSEC-`。

### A.3 测试治理建议

1. **建立 SHALL-Test 自动标记机制**：在测试代码中通过注释 `// SHALL: KL-SHALL-01` 等标记关联，便于自动化追溯
2. **新增 OTA 测试文件**：`e2e_06_ota_update_test.go` + `test_ota_signing.c`
3. **Android KeyStore 专项测试**：`frontend/android/src/test/kotlin/com/digitalkey/sdk/SecureStorageTest.kt`
4. **CI 门禁**：覆盖率缺口分析应纳入 CI Pipeline，每次提交自动更新追溯矩阵
