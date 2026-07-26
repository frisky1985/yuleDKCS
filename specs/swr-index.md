# yuleDKCS SWR 需求索引

> **版本**: v1.0 | **创建日期**: 2026-07-26
> 基于以下 spec 文件提取：`specs/spec-cmd-test.md`, `specs/spec-embedded-c.md`, `specs/spec-fix-kni.md`, `specs/spec-fix-p0.md`, `specs/spec-frontend-test.md`

| ID | 描述 | 来源文件 | 模块 | 状态 |
|----|------|---------|------|------|
| SWR-001 | dkcs 入口测试（initDatabase、initRedis） | `specs/spec-cmd-test.md` | DKCS Core | DRAFT |
| SWR-002 | hub 入口测试（setupHubGRPCServer、6 适配器注册） | `specs/spec-cmd-test.md` | Hub | DRAFT |
| SWR-003 | yuledkcs 统一入口测试（三种启动模式路由） | `specs/spec-cmd-test.md` | DKCS Core / Hub | DRAFT |
| SWR-001 | C 单元测试框架（Unity 框架引入） | `specs/spec-embedded-c.md` | Embedded | Draft |
| SWR-002 | ICCE 协议栈单元测试（24 用例） | `specs/spec-embedded-c.md` | Embedded | Draft |
| SWR-003 | CCC 协议栈单元测试（核心 API） | `specs/spec-embedded-c.md` | Embedded | Draft |
| SWR-004 | ICCOA 协议栈单元测试 | `specs/spec-embedded-c.md` | Embedded | Draft |
| SWR-005 | CI 中集成 C 单元测试 | `specs/spec-embedded-c.md` | Embedded / CI | Draft |
| SWR-HUB-001 | Registry 大小写规范化 + nil 指针修复 | `specs/spec-fix-kni.md` | Hub | APPROVED |
| SWR-HUB-002 | strings.ToLower 调用修复 | `specs/spec-fix-kni.md` | Hub | APPROVED |
| SWR-HUB-003 | 单元测试覆盖（hub/service + hub/logger） | `specs/spec-fix-p0.md` | Hub | APPROVED |
| SWR-HUB-004 | CI 覆盖率门禁（fail-under=60） | `specs/spec-fix-p0.md` | Hub / CI | APPROVED |
| SWR-HUB-005 | CI 分层机制（L1/L2/L3）+ SAST 安全扫描 | `specs/spec-fix-p0.md` | Hub / CI | APPROVED |
| SWR-001 | Android SDK API 单元测试 | `specs/spec-frontend-test.md` | Frontend (Android) | DRAFT |
| SWR-002 | iOS SDK API 单元测试 | `specs/spec-frontend-test.md` | Frontend (iOS) | DRAFT |
| SWR-003 | 测试编译 CI 集成 | `specs/spec-frontend-test.md` | Frontend / CI | DRAFT |

---

## 按来源文件统计

| 来源文件 | SWR 数量 | SWR ID 列表 |
|---------|---------|-------------|
| `specs/spec-cmd-test.md` | 3 | SWR-001, SWR-002, SWR-003 |
| `specs/spec-embedded-c.md` | 5 | SWR-001~005 |
| `specs/spec-fix-kni.md` | 2 | SWR-HUB-001, SWR-HUB-002 |
| `specs/spec-fix-p0.md` | 3 | SWR-HUB-003, SWR-HUB-004, SWR-HUB-005 |
| `specs/spec-frontend-test.md` | 3 | SWR-001, SWR-002, SWR-003 |
| **合计** | **16** | |

## 按模块统计

| 模块 | SWR 数量 |
|------|---------|
| DKCS Core | 2 |
| Hub | 5 |
| Embedded | 5 |
| Frontend | 2 |
| CI（跨模块） | 3 |
