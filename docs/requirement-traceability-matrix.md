# 需求追溯矩阵 (Requirements Traceability Matrix)

> **版本**: v1.0 | **日期**: 2026-07-18  
> **方法**: yuleDKCS 需求管理规范化 Phase 4  
> **覆盖范围**: 所有 RS / SWR 级别需求 → 对应测试文件 / 验收场景

---

## 追溯矩阵

| 需求 ID | SHALL 内容摘要 | 模块 | 对应 Spec 文件 | 对应测试文件/场景 | 覆盖状态 | ASIL |
|---------|--------------|------|---------------|------------------|---------|------|
| RS-001 | 用户设备注册 (含能力上报、ID分配) | DKCS Core | `docs/spec/spec-multi-device.md` | — | ❌ 无测试 | QM |
| RS-002 | 多设备按需配钥 (协议协商、唯一绑定) | DKCS Core | `docs/spec/spec-multi-device.md` | — | ❌ 无测试 | QM |
| RS-003 | 多设备管理 (列表/吊销/推送通知/限制) | DKCS Core | `docs/spec/spec-multi-device.md` | — | ❌ 无测试 | QM |
| RS-004-13 | 解锁响应 ≤ 1s | Embedded + Hub | `docs/design/PRD.md` §6.1 | — | ❌ 无测试 | QM |
| RS-004-14 | 上锁响应 ≤ 1s | Embedded + Hub | `docs/design/PRD.md` §6.1 | — | ❌ 无测试 | QM |
| RS-004-16 | 云端 API P99 ≤ 200ms | Cloud | `docs/design/PRD.md` §6.1 | — | ❌ 无测试 | QM |
| RS-004-17 | 配对完成 ≤ 3min | All | `docs/design/PRD.md` §6.1 | — | ❌ 无测试 | QM |
| RS-004-18 | 远程控制响应 ≤ 3s | Cloud + Hub | `docs/design/PRD.md` §6.1 | — | ❌ 无测试 | QM |
| RS-005-20 | 服务可用性 ≥ 99.9% | Cloud | `docs/design/PRD.md` §6.2 | — | ❌ 无测试 | QM |
| RS-005-21 | MTTR ≤ 30min | Cloud | `docs/design/PRD.md` §6.2 | — | ❌ 无测试 | QM |
| RS-005-22 | RTO ≤ 4h | Cloud | `docs/design/PRD.md` §6.2 | — | ❌ 无测试 | QM |
| RS-006-25 | 安全芯片 EAL5+ | Embedded | `docs/design/PRD.md` §6.3 | — | ❌ 无测试 | **ASIL-B** |
| RS-006-26 | TLS 1.3 通信加密 | All | `docs/design/PRD.md` §6.3 | — | ❌ 无测试 | **ASIL-B** |
| RS-006-29 | UWB 防中继攻击 | Embedded | `docs/design/PRD.md` §6.3 | — | ❌ 无测试 | **ASIL-B** |
| RS-006-30 | Nonce 防重放 | All | `docs/design/PRD.md` §6.3 | — | ❌ 无测试 | **ASIL-B** |
| RS-007-33 | 手机没电时NFC仍可用 | Embedded | `docs/design/PRD.md` §3.2.2 | — | ❌ 无测试 | QM |
| RS-008-36 | ICCE + CCC 双协议兼容 | Embedded | `embedded/iccoa_protocol/docs/SPEC.md` | — | ❌ 无测试 | QM |
| RS-009-40 | 走近 ≤2m 自解锁 ≤1s | Embedded | `docs/design/PRD.md` §4.2 | — | ❌ 无测试 | QM |
| RS-009-43 | 同一车辆最多 5 把钥匙 | Cloud | `docs/design/PRD.md` §4.2 | — | ❌ 无测试 | QM |
| **SWR-DKC-001** | 密钥绑定 (1000/1001) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.1 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-DKC-002** | 密钥解绑 (1002/1003) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.2 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-DKC-003** | 密钥撤销 (1004/1005) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.3 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-DKC-004** | 密钥列表 (1010/1011) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.4 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-DKC-005** | 分享创建 (2000/2001) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.5 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-DKC-006** | 车辆控制指令 (3000/3001) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.6 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-DKC-007** | 车辆状态上报 (3002) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.7 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-DKC-008** | 心跳 (9000/9001) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §3.8 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-DKC-009** | 消息签名 (HMAC-SHA256) | DKCS Core | `backend/cloud/protocol/hub-dkcs-protocol.md` §4 | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-HUB-001** | Registry 大小写规范化 | Hub | `specs/spec-fix-kni.md` (§KNI-001~003) | `go test ./backend/cloud/hub/internal/adapter/...` (验收矩阵指定) | ❌ 待验证 | QM |
| **SWR-HUB-002** | nil 安全 & ToLower 调用 | Hub | `specs/spec-fix-kni.md` (§KNI-001~002) | 代码审查 + `go test -race` | ❌ 待验证 | QM |
| **SWR-HUB-003** | 单元测试覆盖 (service/logger) | Hub | `specs/spec-fix-p0.md` (FIX-001~002) | `go test ./backend/cloud/hub/internal/service/...` + `./internal/logger/...` | ❌ 待验证 | QM |
| **SWR-HUB-004** | CI 覆盖率门禁 60% | Hub | `specs/spec-fix-p0.md` (FIX-003) | CI 配置 (go test -coverprofile) | ❌ 待验证 | QM |
| **SWR-HUB-005** | CI 分层 L1/L2/L3 | Hub | `specs/spec-fix-p0.md` (FIX-004~006) | CI pipeline YAML | ❌ 待验证 | QM |
| **SWR-PRO-001** | BERTLV 编码规范 | Protocol | `backend/cloud/protocol/hub-dkcs-protocol.md` §1.2 | 暂缺编码测试 | ❌ 无测试 | QM |
| **SWR-PRO-002** | DKCS↔TCU MQTT 协议 | Protocol | `backend/cloud/protocol/dkcs-tcu-protocol.md` | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-PRO-003** | App↔HUB REST+BER-TLV | Protocol | `backend/cloud/protocol/app-hub-protocol.md` | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-PRO-004** | 统一错误码体系 | Protocol | `backend/cloud/protocol/error-codes.md` | 暂缺错误码测试 | ❌ 无测试 | QM |
| **SWR-EMB-001** | NFC ST25R501 通信层 | Embedded | `embedded/ccc_protocol/docs/SPEC.md` §2 | 暂缺HIL测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-002** | BLE KW47A 通信层 | Embedded | `embedded/ccc_protocol/docs/SPEC.md` §3 | 暂缺HIL测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-003** | UWB NCJ29D6 测距层 | Embedded | `embedded/ccc_protocol/docs/SPEC.md` §4 | 暂缺HIL测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-004** | ICCOA DK 3.0/4.0 协议栈 | Embedded | `embedded/iccoa_protocol/docs/SPEC.md` | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-005** | ICCE 协议栈 (边缘计算) | Embedded | `embedded/icce_protocol/docs/technical_specification.md` | 暂缺集成测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-006** | SE050 安全芯片集成 | Embedded | `embedded/ccc_protocol/docs/SPEC.md` §6 | 暂缺HIL测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-EMB-007** | 电源管理 (5级状态) | Embedded | `embedded/system_architecture/SPEC.md` §5 | 暂缺功耗测试 | ❌ 无测试 | QM |
| **SWR-EMB-008** | 安全启动 (4阶段验签) | Embedded | `embedded/system_architecture/SPEC.md` §4.4 | 暂缺HIL测试 | ❌ 无测试 | **ASIL-B** |
| **SWR-FE-001** | Android SDK (Min 26/NFC/BLE/Loc/Camera) | Frontend | `backend/cloud/protocol/mobile-sdk-spec.md` §2 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-FE-002** | iOS SDK (Min 14.0) | Frontend | `backend/cloud/protocol/mobile-sdk-spec.md` §3 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-FE-003** | RESTful API (JWT/TLS 1.3) | Frontend | `docs/design/API-CONTRACT.md` §3 | 暂缺API自动化测试 | ❌ 无测试 | QM |
| **SWR-FE-004** | gRPC 微服务 (Key+Vehicle+KMS) | Frontend | `docs/design/API-CONTRACT.md` §4 | 暂缺集成测试 | ❌ 无测试 | QM |
| **SWR-FE-005** | 消息队列 (9 Topic + Avro) | Frontend | `docs/design/API-CONTRACT.md` §5 | 暂缺集成测试 | ❌ 无测试 | QM |

---

## 覆盖统计

| 维度 | 数量 | 占比 |
|------|------|------|
| 总需求条目 | 40 (含9 RS + 31 SWR) | 100% |
| 有对应测试文件 | 2 (SWR-HUB-001, SWR-HUB-003) | 5% |
| 无测试覆盖 | 38 | 95% |
| ASIL-B 等级 | 14 | 35% |
| ASIL-A 等级 | 0 | 0% |
| QM 等级 | 26 | 65% |

---

## 风险项 (Test Gap)

1. **ASIL-B 零测试**: 14 个 ASIL-B 等级需求无对应测试文件，违反 ISO 26262
2. **协议集成测试缺失**: SWR-PRO-001~004 (BERTLV/MQTT/REST/错误码) 无编解码测试
3. **Embedded HIL 测试缺失**: SWR-EMB-001~008 全部无硬件在环测试
4. **SDK 集成测试缺失**: SWR-FE-001~005 无自动化集成测试
5. **性能指标无验收**: RS-004 各项响应时间指标无 Benchmark 测试

## 建议

1. **P0 优先**: SWR-HUB-001~005 (hub/adapter + CI) 已有验收矩阵，立即执行
2. **Protocol**: SWR-PRO-001~002 (BERTLV 编解码 + MQTT) 应补充单元测试
3. **Safety**: ASIL-B 需求 (SWR-DKC-001~009) 需建立测试计划
4. **Embedded**: SWR-EMB-001~008 需引入 HIL 测试框架
