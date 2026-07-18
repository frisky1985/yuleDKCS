# OpenSpec: yuleDKCS KNI 生产 Bug 修复

> **版本**: 1.0.0  
> **基于**: `reports/fix-review.md` 发现的 KNI-001~003  
> **日期**: 2026-07-18

---

## 概述

修复小克测试发现、小马审查确认的 3 个生产 bug。

### KNI-001: ICCOACodec.Encode nil pointer dereference（P0 🔴）→ SWR-HUB-001, SWR-HUB-002

**Reason:** ForwardToVendor 中 registry.Get 传入大写 proto.String() 但适配器以小写注册，导致 Get 失败，fallthrough 到适配器的 Encode 方法时 RemoteControl 为 nil 引发 panic

**SHALL:**
<!-- REQ-ID: SWR-HUB-001.1 -->
- The system SHALL lowercase vendor and protocol strings before registry lookup
<!-- REQ-ID: SWR-HUB-002.1 -->
- The system SHALL add nil safety check before accessing RemoteControl field

**Status:** PROPOSED

### KNI-002: strings.ToLower 定义了但未调用（P1 🟡）→ SWR-HUB-002

**Reason:** 变量 `vendorLower` 定义了但 strings.ToLower() 未实际调用，proto 枚举大写值不匹配 switch 小写

**SHALL:**
<!-- REQ-ID: SWR-HUB-002.2 -->
- The system SHALL actually call strings.ToLower() on the vendor variable
<!-- REQ-ID: SWR-HUB-002.3 -->
- The system SHALL normalize vendor/protocol to lowercase at lookup points

**Status:** PROPOSED

### KNI-003: Registry 大小写不敏感匹配缺失（P1 🟡）→ SWR-HUB-001

**Reason:** Registry.Register() 以原始字符串存储，Registry.Get() 以原始字符串匹配。适配器注册时用小写，但 proto.String() 返回大写

**SHALL:**
<!-- REQ-ID: SWR-HUB-001.2 -->
- The system SHALL normalize keys to lowercase in Registry.Register()
<!-- REQ-ID: SWR-HUB-001.3 -->
- The system SHALL normalize keys to lowercase in Registry.Get()
<!-- REQ-ID: SWR-HUB-001.4 -->
- The system SHALL NOT break existing matching behavior for lowercase keys

**Status:** PROPOSED

---

## 场景

### Scenario: 正向流程
- GIVEN adapter registered as "xiaomi:iccoa_dk40"
- WHEN ForwardToVendor calls Get("XIAOMI", "ICCOA_DK40")
- THEN Registry SHALL find the adapter
- AND no nil pointer dereference SHALL occur

### Scenario: 反向兼容
- GIVEN adapter registered as "xiaomi:iccoa_dk40"
- WHEN Get("xiaomi", "iccoa_dk40") is called
- THEN Registry SHALL still find the adapter

---

## 验收矩阵

| Bug ID | 验收条件 | 验证方法 |
|--------|----------|----------|
| KNI-001 | Registry.Get 大写输入也能匹配小写注册 | `go test ./backend/cloud/hub/internal/adapter/...` |
| KNI-001 | 无 nil panic 路径 | go test -race |
| KNI-002 | ToLower 实际被调用 | 代码审查 |
| KNI-003 | 注册和查找统一 lowercase | `go test ./backend/cloud/hub/internal/adapter/...` |
| 全部 | go vet 零错误 | `go vet ./backend/cloud/hub/...` |
| 全部 | 所有测试通过 | `go test -count=1 ./backend/cloud/hub/...` |
