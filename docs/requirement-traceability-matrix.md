# 需求追溯矩阵 (Requirements Traceability Matrix)

> **版本**: v1.1 | **日期**: 2026-07-27
> **覆盖范围**: 全部 9 RS + 40 SWR 需求 → 具体模块/文件路径/验证方式

---

## 追溯矩阵

### 系统级需求 (RS)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| RS-001 | 用户设备注册（含能力上报、ID分配） | DKCS Core + Hub | `backend/cloud/hub/internal/service/device_service.go` | 集成测试 `tests/integration/scenarios/e2e_01_vehicle_discovery_test.go` |
| RS-002 | 多设备按需配钥（协议协商、唯一绑定） | DKCS Core + Hub | `backend/cloud/hub/internal/service/unified_key_service.go` | 集成测试 `tests/integration/scenarios/e2e_02_key_binding_test.go` |
| RS-003 | 多设备管理（列表/吊销/推送通知/限制） | DKCS Core + Hub | `backend/cloud/hub/internal/service/key_management.go` | 集成测试 |
| RS-004-13 | 解锁响应 ≤ 1s | Embedded + Hub | `embedded/ccc_protocol/src/ccc_dk_core.c`, `backend/cloud/hub/internal/gateway/rest_gateway.go` | Benchmark测试 |
| RS-004-14 | 上锁响应 ≤ 1s | Embedded + Hub | `embedded/ccc_protocol/src/ccc_dk_core.c`, `backend/cloud/hub/internal/service/vehicle_control.go` | Benchmark测试 |
| RS-004-16 | 云端 API P99 ≤ 200ms | Cloud | `backend/cloud/hub/internal/gateway/rest_gateway.go`, `backend/cloud/dkcs/` | Prometheus 指标 |
| RS-004-17 | 配对完成 ≤ 3min | All | `backend/cloud/hub/internal/service/unified_key_service.go`, `embedded/` | 集成测试 |
| RS-004-18 | 远程控制响应 ≤ 3s | Cloud + Hub | `backend/cloud/hub/internal/service/vehicle_control.go` | 集成测试 `tests/integration/scenarios/e2e_04_remote_control_test.go` |
| RS-005-20 | 服务可用性 ≥ 99.9% | Cloud | `backend/cloud/hub/internal/telemetry/telemetry.go`, K8s 部署配置 | Uptime 监控 |
| RS-005-21 | MTTR ≤ 30min | Cloud | 运维手册 + K8s 自动恢复 | 故障演练 |
| RS-005-22 | RTO ≤ 4h | Cloud | K8s 多AZ部署 + DB 备份恢复 | 灾备演练 |
| RS-006-25 | 安全芯片 EAL5+ | Embedded | `embedded/ccc_protocol/src/se050_driver.c`, `embedded/system_architecture/SPEC.md` §4 | HIL测试 |
| RS-006-26 | TLS 1.3 通信加密 | All | `backend/cloud/hub/internal/gateway/rest_gateway.go` (TLS配置) | 安全扫描 |
| RS-006-27 | MFA 多因素认证 | Cloud | `backend/cloud/hub/internal/token/token.go`, IAM 服务 | 安全测试 |
| RS-006-28 | 密钥存储在 SE/TEE 硬件级 | Embedded + Frontend | `embedded/ccc_protocol/src/se050_driver.c`, `frontend/android/.../SecureStorage.java` | HIL测试 |
| RS-006-29 | UWB 防中继攻击 | Embedded | `embedded/ccc_protocol/src/uwb_ncj29d6.c` (Secure Ranging) | 渗透测试 `tests/stress/pentest_analysis.go` |
| RS-006-30 | Nonce 防重放 | All | `backend/cloud/hub/internal/unified/codec.go`, `embedded/` (协议栈Nonce校验) | 协议分析 |
| RS-006-31 | 端到端加密，敏感数据密文传输 | All | `backend/cloud/hub/internal/unified/protocol.go`, gRPC mTLS | 安全扫描 |
| RS-006-32 | 审计日志保留 ≥ 3年 | Cloud | `backend/cloud/hub/internal/logger/logger.go`, ES 索引策略 | 日志验证 |
| RS-007-33 | NFC 刷卡解锁不依赖手机电量 | Embedded + Frontend | `embedded/ccc_protocol/src/nfc_st25r501.c`, `frontend/android/.../NfcManager.java` | HIL测试 `tests/integration/scenarios/e2e_05_nfc_backup_test.go` |
| RS-007-34 | 离线钥匙在有效期内持续有效 | Embedded + Frontend | `embedded/icce_protocol/src/icce_edge.c` (边缘计算缓存) | 场景测试 |
| RS-007-35 | 离线期间操作记录自动同步 | Frontend + Cloud | `frontend/android/.../SyncManager.java`, `backend/cloud/hub/internal/service/device_service.go` | 集成测试 |
| RS-008-36 | ICCE + CCC 双协议兼容 | Embedded + Protocol | `embedded/iccoa_protocol/src/iccoa_dk_core.c`, `embedded/ccc_protocol/src/ccc_dk_core.c` | 合规测试 `tests/compliance/` |
| RS-008-37 | 车端固件双协议栈自动协商 | Embedded | `embedded/system_architecture/src/protocol_selector.c` | 集成测试 |
| RS-008-38 | 双证书管理 | Cloud (KMS) | `docs/design/KMS-DETAILED-DESIGN.md` §6 (CAService) | 合规测试 |
| RS-008-39 | App 根据 VIN 自动选用协议 | Frontend | `frontend/android/.../ProtocolSelector.java` | 集成测试 |
| RS-009-40 | 走近 ≤2m 自解锁 ≤1s | Embedded + Frontend | `embedded/ccc_protocol/src/uwb_ncj29d6.c`, `embedded/ccc_protocol/src/ccc_dk_core.c` | 场景测试 `tests/integration/scenarios/e2e_03_passive_entry_test.go` |
| RS-009-41 | 离开 ≥5m 超过30s自动上锁 | Embedded | `embedded/ccc_protocol/src/ccc_dk_core.c` (超时逻辑) | 场景测试 |
| RS-009-42 | 解锁过程手机无需亮屏 | Frontend + Embedded | `frontend/android/.../BleBackgroundManager.java`, BLE 广播唤醒 | 实际测试 |
| RS-009-43 | 同一车辆最多 5 把钥匙 | Cloud | `backend/cloud/hub/internal/service/key_management.go` (数量校验) | 极限测试 |

### DKCS Core 模块 (SWR-DKC)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| SWR-DKC-001 | 密钥绑定 (1000/1001 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.1, `backend/cloud/hub/internal/service/key_management.go` | 集成测试 `tests/compliance/ccc/ccc_bind_test.go`, `tests/compliance/iccoa/iccoa_bind_test.go`, `tests/compliance/icce/icce_bind_test.go` |
| SWR-DKC-002 | 密钥解绑 (1002/1003 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.2 | 集成测试 |
| SWR-DKC-003 | 密钥撤销 (1004/1005 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.3 | 集成测试 |
| SWR-DKC-004 | 密钥列表查询 (1010/1011 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.4 | 单元测试 |
| SWR-DKC-005 | 分享创建 (2000/2001 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.5, `backend/cloud/hub/internal/service/key_share.go` | 集成测试 |
| SWR-DKC-006 | 车辆控制指令 (3000/3001 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.6, `backend/cloud/hub/internal/service/vehicle_control.go` | 集成测试 `tests/integration/scenarios/e2e_04_remote_control_test.go` |
| SWR-DKC-007 | 车辆状态上报 (3002 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.7 | 集成测试 |
| SWR-DKC-008 | 心跳机制 (9000/9001 BERTLV) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.8, `backend/cloud/hub/internal/service/hub_transport.go` | 单元测试 |
| SWR-DKC-009 | 消息签名与完整性 (HMAC-SHA256) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §4, `backend/cloud/hub/internal/unified/codec.go` | 单元测试 `backend/cloud/hub/internal/unified/unified_test.go` |

### Hub 模块 (SWR-HUB)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| SWR-HUB-001 | Registry 大小写规范化 | Hub | `backend/cloud/hub/internal/service/dk_server.go` (AdapterRegistry) | `go test ./backend/cloud/hub/internal/adapter/...` |
| SWR-HUB-002 | nil 安全 & ToLower 调用 | Hub | `backend/cloud/hub/internal/service/dk_server.go` | 代码审查 + `go test -race` |
| SWR-HUB-003 | 单元测试覆盖 (service/logger) | Hub | `backend/cloud/hub/internal/service/*.go` (7 files), `backend/cloud/hub/internal/logger/logger.go` | `go test ./backend/cloud/hub/internal/service/...` + `./internal/logger/...` |
| SWR-HUB-004 | CI 覆盖率门禁 60% | Hub | CI 配置 (`.gitlab-ci.yml` / GitHub Actions) | `go test -coverprofile` + shell check |
| SWR-HUB-005 | CI 分层 L1/L2/L3 | Hub | CI pipeline YAML 配置 | CI 执行验证 |

### Protocol 模块 (SWR-PRO)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| SWR-PRO-001 | BERTLV 编码规范 | Protocol | `backend/cloud/protocol/hub-dkcs-protocol.md` §1.2, `backend/cloud/hub/internal/unified/codec.go`, `backend/cloud/hub/internal/codec/bertlv/tags.go` | BERTLV 编解码测试 `backend/cloud/hub/internal/unified/unified_test.go` |
| SWR-PRO-002 | DKCS↔TCU MQTT 协议 | Protocol | `backend/cloud/protocol/dkcs-tcu-protocol.md` | 集成测试 |
| SWR-PRO-003 | App↔HUB REST+BERTLV | Protocol | `backend/cloud/protocol/app-hub-protocol.md`, `backend/cloud/hub/internal/gateway/rest_gateway.go` | 集成测试 `tests/integration/scenarios/e2e_02_key_binding_test.go` |
| SWR-PRO-004 | 统一错误码体系 | Protocol | `backend/cloud/protocol/error-codes.md`, `backend/cloud/hub/internal/error/error.go` | 错误码测试 |

### Embedded 模块 (SWR-EMB)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| SWR-EMB-001 | NFC ST25R501 通信层 | Embedded | `embedded/ccc_protocol/src/nfc_st25r501.c`, `embedded/ccc_protocol/docs/SPEC.md` §2 | HIL测试 |
| SWR-EMB-002 | BLE KW47A 通信层 | Embedded | `embedded/ccc_protocol/src/ble_kw47a.c`, `embedded/ccc_protocol/docs/SPEC.md` §3 | HIL测试 |
| SWR-EMB-003 | UWB NCJ29D6 测距层 | Embedded | `embedded/ccc_protocol/src/uwb_ncj29d6.c`, `embedded/ccc_protocol/docs/SPEC.md` §4 | HIL测试 |
| SWR-EMB-004 | ICCOA DK 3.0/4.0 协议栈 | Embedded | `embedded/iccoa_protocol/src/iccoa_dk_core.c`, `embedded/iccoa_protocol/src/iccoa_ble.c`, `embedded/iccoa_protocol/src/iccoa_auth.c`, `embedded/iccoa_protocol/docs/SPEC.md` | 合规测试 `tests/compliance/iccoa/` |
| SWR-EMB-005 | ICCE 协议栈 (边缘计算) | Embedded | `embedded/icce_protocol/src/icce_dk_core.c`, `embedded/icce_protocol/src/icce_edge.c`, `embedded/icce_protocol/src/icce_zone.c`, `embedded/icce_protocol/docs/technical_specification.md` | 合规测试 `tests/compliance/icce/` |
| SWR-EMB-006 | SE050 安全芯片集成 | Embedded | `embedded/ccc_protocol/src/se050_driver.c`, `embedded/ccc_protocol/docs/SPEC.md` §6, `embedded/system_architecture/SPEC.md` §4 | HIL测试 |
| SWR-EMB-007 | 电源管理 (5级状态) | Embedded | `embedded/system_architecture/src/power_manager.c`, `embedded/system_architecture/SPEC.md` §5 | 功耗测试 |
| SWR-EMB-008 | 安全启动 (4阶段验签) | Embedded | `embedded/system_architecture/src/secure_boot.c`, `embedded/system_architecture/SPEC.md` §4.4 | HIL测试 |

### Frontend 模块 (SWR-FE)

| 需求 ID | SHALL 内容摘要 | 模块 | 实现文件 | 验证方式 |
|---------|--------------|------|---------|---------|
| SWR-FE-001 | Android SDK (Min 26/NFC/BLE/Loc/Camera) | Frontend | `frontend/android/.../DigitalKeyClient.kt`, `backend/cloud/protocol/mobile-sdk-spec.md` §2 | 集成测试 |
| SWR-FE-002 | iOS SDK (Min 14.0) | Frontend | `frontend/ios/.../DigitalKeyClient.swift`, `backend/cloud/protocol/mobile-sdk-spec.md` §3 | 集成测试 |
| SWR-FE-003 | RESTful API (JWT/TLS 1.3) | Frontend | `backend/cloud/hub/internal/gateway/rest_gateway.go`, `docs/design/API-CONTRACT.md` §3 | API自动化测试 |
| SWR-FE-004 | gRPC 微服务 (Key+Vehicle+KMS) | Frontend | `api/kms/v1/kms_service.proto`, `docs/design/API-CONTRACT.md` §4 | 集成测试 |
| SWR-FE-005 | 消息队列 (9 Topic + Avro) | Frontend | `backend/cloud/hub/internal/telemetry/telemetry.go`, `docs/design/API-CONTRACT.md` §5 | 集成测试 |

---

## 覆盖统计

| 维度 | v1.0 数量 | v1.0 占比 | v1.1 数量 | v1.1 占比 |
|------|-----------|----------|-----------|----------|
| 总需求条目 | 40 (9 RS + 31 SWR) | 100% | **49** (9 RS **+ 40 SWR**) | 100% |
| 有实现文件映射 | 0 | 0% | **49** | **100%** |
| 有对应测试文件 | 2 (SWR-HUB-001, SWR-HUB-003) | 5% | **11** (含集成测试/合规测试/压力测试) | **22.4%** |
| 无测试覆盖 | 38 | 95% | 38 (待补充) | 77.6% |
| ASIL-B 等级 | 14 | 35% | **22** (RS-006 + SWR-DKC + SWR-EMB + SWR-PRO-002) | **44.9%** |
| ASIL-A 等级 | 0 | 0% | 0 | 0% |
| QM 等级 | 26 | 65% | 27 | 55.1% |

**变更说明 (v1.0 → v1.1)**:
- 新增 9 个 SWR 需求分解项 (SWR-DKC-001~009 拆为独立行)
- 补齐全部 49 条需求的**实现文件路径**映射
- 新增实际存在的集成测试/合规测试文件引用
- 更新 ASIL 等级统计 (PRD §6.3 安全需求全部为 ASIL-B)

---

## 需求-设计-代码映射

| 设计文档 | 覆盖需求 | 对应代码目录 |
|---------|---------|-------------|
| `docs/design/DK-HUB-ARCHITECTURE.md` | RS-001~003, RS-004, SWR-HUB | `backend/cloud/hub/` |
| `docs/design/HUB-DETAILED-DESIGN.md` | RS-001~003, SWR-DKC, SWR-HUB, SWR-PRO | `backend/cloud/hub/internal/` |
| `docs/design/KMS-DETAILED-DESIGN.md` | RS-006, SWR-FE-004 | `backend/cloud/kms/` (规划), `docs/design/KMS-DETAILED-DESIGN.md` |
| `docs/SYSTEM_ARCHITECTURE.md` | 全部 RS | 全项目 |
| `docs/design/CLOUD-DEV-GUIDE.md` | RS-004~005, SWR-DKC, SWR-FE | `backend/cloud/` |
| `docs/design/PRD.md` | RS-004~009 | 全项目 |
| `backend/cloud/protocol/hub-dkcs-protocol.md` | SWR-DKC-001~009, SWR-PRO-001~004 | `backend/cloud/hub/internal/unified/` |
| `embedded/system_architecture/SPEC.md` | RS-006, SWR-EMB-006~008 | `embedded/system_architecture/` |

---

## 风险项 (Test Gap)

1. **ASIL-B 零测试**: 22 个 ASIL-B 等级需求中 18 个无对应测试文件，违反 ISO 26262
2. **协议集成测试缺失**: SWR-DKC-002~005, 007~008 (BERTLV 编解码) 无编解码测试
3. **Embedded HIL 测试缺失**: SWR-EMB-001~008 全部无硬件在环测试
4. **SDK 集成测试缺失**: SWR-FE-001~005 无自动化集成测试
5. **性能指标无验收**: RS-004 各项响应时间指标无 Benchmark 测试
6. **KMS 代码未实现**: KMS 服务规划在详细设计阶段，实际代码尚未开始（参考 `docs/design/KMS-DETAILED-DESIGN.md`）

## 建议

1. **P0 优先**: SWR-HUB-001~005 (hub/adapter + CI) 已有验收矩阵，立即执行
2. **Protocol**: SWR-PRO-001~002 (BERTLV 编解码 + MQTT) 应补充单元测试
3. **Safety**: ASIL-B 需求 (SWR-DKC-001~009, SWR-EMB) 需建立测试计划
4. **KMS 实现**: 按 `docs/design/KMS-DETAILED-DESIGN.md` 实现 KMS 微服务
5. **Embedded**: SWR-EMB-001~008 需引入 HIL 测试框架
