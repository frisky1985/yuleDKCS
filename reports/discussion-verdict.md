# 🔥 内部讨论裁决 — ICCOACodec nil 安全检查

> **主持**: 小明 🔥（项目经理/终审）
> **参与**: 小克 👨‍💻（编码架构师） + 小马 🐴（质量架构师）
> **日期**: 2026-07-18

---

## 讨论摘要

### 发现一：ICCOACodec.encodeRemoteControl nil 安全检查

**小克 👨‍💻 分析**:
- 当前 Manager 所有路径都构造完整字段消息，**当前生产路径走不到**
- 但发现范围比预期大：ICCOACodec **4 个** encode 方法 + ICCECodec **3 个** encode 分支都有同样的 nil 解引用问题
- CCCCodec **已经有** nil 保护，可以做参考模板
- 优先级建议：P2（可以等下个 Sprint）

**小马 🐴 分析**:
- `UnifiedKeyService.ForwardToVendor` 中构造 `&UnifiedMessage{Type: MsgTypeRemoteControl}` **不设置 RemoteControl 字段** → 确定 nil → 确定 panic
- 72% 概率量化（条件链分析：ForwardToVendor 触发 × ICCOA 判定 × nil RemoteControl）
- **两条 SHALL 完全未完成**，验收可拒签
- 优先级建议：P0（现在修，30 分钟工作量）

### 发现二：Register(小写)+Get(大写) 测试

**小克**: 功能覆盖已有，只是命名风格问题

**小马**: 需要 4 个独立测试函数覆盖大小写正向/反向/混合/负向

---

## 🔥 小明裁决

### 发现一：同意小马的风险评估，但优先级下调一级

| 维度 | 裁决 |
|:-----|:------|
| 风险存在？ | ✅ **存在**。`UnifiedKeyService.ForwardToVendor` 是 gRPC handler 生产代码 |
| 修复成本？ | ✅ 极低。~10 行 if guard + ~50 行测试 |
| 范围？ | 扩大。ICCOACodec 4 个 + ICCECodec 3 个 encode 方法一起修 |
| 优先级？ | **P1**。P0 太激烈（需特定外部输入触发），P2 太宽松（风险真实存在且修复成本极低） |
| 时限？ | **今天内修**。30 分钟工作量不值得排队 |

**裁决理由**:
> 小克说的"当前 Manager 路径走不到"是对的，但小马指出的 `ForwardToVendor` 路径也是真实存在的生产代码。两个都对，但小马看到了更完整的调用链。30 分钟能搞定的 bug，不值得等到下个 Sprint。

### 发现二：同意小马，需要显式测试用例

| 维度 | 裁决 |
|:-----|:------|
| 功能覆盖？ | ⚠️ 有但不显式 |
| 需要加？ | ✅ **加**。4 个子测试（小写注册大写查、大写注册小写查、混合大小写、不存在返回 false） |

### 执行方案

| 步骤 | 责任人 | 预计时间 |
|:-----|:------:|:--------:|
| codec.go: 7 处 nil guard | 小克 👨‍💻 | 10 min |
| registry.go: Register/Get ToLower 确认 | 小克 👨‍💻 | 5 min |
| 测试：nil encode + case-insensitive | 小克 👨‍💻 | 15 min |
| 审查验收 | 小马 🐴 | 10 min |
| **总计** | | **~40 min** |

---

*小明 🔥 终审 · 2026-07-18 11:30 GMT+8*
