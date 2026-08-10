# P1-4: Git Flow 分支 + CI 适配 + Android Biometric

**日期**: 2026-07-16  
**执行人**: Claude (subagent)  
**状态**: ✅ Complete

---

## 1. Git Flow 分支创建 (修复 1)

### 操作

```bash
git checkout -b develop master
git push origin develop
```

- **新分支**: `develop` 基于 `master` 创建并推送到 origin
- **当前分支**: 停留在 `develop`（后续所有修改在此分支上完成）
- **不破坏**: `master` 不受影响，所有修改仅在 `develop` 上进行

### 影响

- CI 工作流（`android-ci.yml`, `ios-ci.yml`, `ci-java.yml`, `fault-inject-ci.yml`）已内置对 `develop` 和 `release/*` 的支持——无需额外修改分支触发条件。
- `yuleosh-ci.yml` 也已配置 `on: { push: { branches: [main, develop, "release/**"] } }`。

---

## 2. CI 工作流分支策略注释 (修复 1)

### `ci.yml` — 添加分支策略头注释

为工作流文件增加了清晰的 **双分支策略文档注释**，说明：

```
== Branch Strategy ==

  develop  ← feature branches (feat/*, fix/*, docs/*, refactor/*, test/*, chore/*)
    ↓ PR
  main     ← release branch (protected, only squash-merges from develop)

== Triggers ==

  push / PR to develop    → Runs full test suite (this workflow)
  merge to main           → Extra yuleOSH evidence chain (see yuleosh-ci.yml)
  release/*               → Additional Android/iOS/Java adapter CI builds
```

### `yuleosh-ci.yml` — 添加证据链角色注释

明确该工作流是 "extra evidence chain"，在 merge 到 main 时额外运行 yuleOSH 合规审计。

### 所有 CI 工作流确认

| 工作流 | 已支持 develop | 备注 |
|--------|:---:|------|
| `ci.yml` | ✅ | `on: [push, pull_request]` 无条件触发 |
| `yuleosh-ci.yml` | ✅ | `branches: [main, develop, "release/**"]` |
| `android-ci.yml` | ✅ | `branches: [main, develop, release/*]` |
| `ios-ci.yml` | ✅ | `branches: [main, develop, release/*]` |
| `ci-java.yml` | ✅ | `branches: [main, develop, release/*]` |
| `misra-ci.yml` | ✅ | `branches: [main, develop, feat/**]` |
| `fault-inject-ci.yml` | ✅ | `branches: [main, develop, release/*]` |
| `cover-check.yml` | ✅ | PR 触发，不限定分支 |

---

## 3. `docs/CONTRIBUTING.md` (修复 2)

创建了一个 **独立的 Git Flow 贡献指南**，内容覆盖：

- **分支拓扑图**（ASCII 图示）
- **分支命名规范**（feat/fix/docs/refactor/test/chore/release 模式）
- **开发流程**：从 `develop` 切 → 开发 → PR → 审查 → squash-merge
- **CI/CD 触发策略表**
- **快速参考命令**
- 与根目录 `CONTRIBUTING.md` 的关系说明

**文件位置**: `docs/CONTRIBUTING.md` (约 3.2 KB Markdown)

> **注意**: 根目录已有 `CONTRIBUTING.md`（更全面的编码规范/测试要求/社区指南），`docs/CONTRIBUTING.md` 专注分支策略和 PR 流程，两者互补。

---

## 4. Android Biometric 评估与可配置化 (修复 3)

### 评估目标

`KeyStoreMetadataStore.getOrCreateSecretKey()` 中使用了：
```kotlin
.setInvalidatedByBiometricEnrollment(false)
```
该调用控制 Android KeyStore AES-256/GCM 密钥在用户添加/删除生物识别（指纹、面部）时是否自动失效。

### 安全权衡分析

| | `false` (默认) | `true` |
|---|---|---|
| **用户体验** | ✅ 用户换指纹/面部后钥匙元数据依旧可用 | ❌ 需重新加密钥匙元数据 |
| **安全性** | ⚠️ 设备失窃后攻击者可绕开 | ✅ 生物识变更视为安全事件 |
| **适用场景** | 家庭用车、消费级 | 共享汽车、企业车队、高安保 |

> 数字钥匙 **元数据**（钥匙 ID、车辆、有效期）不含私钥材料，私钥独立存储在 Android KeyStore 中。元数据泄漏风险有限，但仍需防护——攻击者可利用元数据进行重放攻击。

### 修改内容

**`KeyStoreMetadataStore.kt`** — 3 处修改：

1. **新增 KDoc 段 (`== Biometric Enrollment Invalidation ==`)** — 详细说明 `setInvalidatedByBiometricEnrollment` 的安全权衡
2. **构造函数新增参数**:
   ```kotlin
   private val invalidateOnBiometricEnrollment: Boolean = false
   ```
   不影响现有调用方（默认 `false`，向后兼容）
3. **`getOrCreateSecretKey()` 使用参数**:
   ```kotlin
   .setInvalidatedByBiometricEnrollment(invalidateOnBiometricEnrollment)
   ```

**`KeyManager.kt`** — 1 处修改：

4. **`generateKeyPair()` 添加注释**标记此处同样硬编码为 `false`，未来可通过构造函数参数注入实现可配置化（与 KeyStoreMetadataStore 模式一致）

### 向后兼容性

- ✅ 默认值 `false`，现有测试无需修改
- ✅ `KeyManager.kt` 中 `metadataStore = KeyStoreMetadataStore(context)` — 使用全部默认参数，工作正常
- ✅ 测试文件 `KeyStoreMetadataStoreInstrumentedTest.kt` 和 `KeyStoreMetadataStoreTest.kt` 不直接依赖此参数

### 使用示例

```kotlin
// 默认——用户体验优先（家庭用车推荐）
val store = KeyStoreMetadataStore(context)

// 更安全——生物识别变更时密钥自动失效（高安保车辆推荐）
val secureStore = KeyStoreMetadataStore(
    context,
    invalidateOnBiometricEnrollment = true
)
```

---

## 5. 产出文件汇总

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `.github/workflows/ci.yml` | ✏️ 修改 | 添加分支策略头注释 |
| `.github/workflows/yuleosh-ci.yml` | ✏️ 修改 | 添加证据链角色注释 |
| `docs/CONTRIBUTING.md` | 🆕 创建 | Git Flow 分支策略 & PR 流程 |
| `frontend/android/.../KeyStoreMetadataStore.kt` | ✏️ 修改 | Biometric 可配置 + 安全注释 |
| `frontend/android/.../KeyManager.kt` | ✏️ 修改 | `generateKeyPair()` Biometric 注释 |
| `reports/p1-branch-ci-biometric-fix.md` | 🆕 创建 | 本报告 |

---

## 6. 未做之事与潜在改进

- **不改业务逻辑**: 所有修改保持默认行为不变
- **KeyManager.generateKeyPair() 未加参数**: 因 KeyManager 构造函数已固定 `context`，增加参数会破坏公共 API。建议后续通过 `KeyManagerConfig` data class 注入
- **develop 分支未添加 branch protection rules**: 需在 GitHub Settings 中配置
