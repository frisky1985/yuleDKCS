# 文档缺陷修复报告

> **日期**: 2026-07-07
> **文件**: `spec-contract.md`, `safety-concept.md`
> **修复类型**: P1 文档缺陷

---

## DOC-P1-01: ASIL/EAL 等级冲突

### 问题描述
- `spec-contract.md` 序言业务目标表和 §1.9 KS-SHALL-04 标注 SE050 安全等级为 **EAL5+**
- `safety-concept.md` §4 FSC 和 §5 假设均正确标注 **EAL6+**
- SE050 真实安全规格为 EAL6+（安全芯片认证等级）

### 修复内容
| 文件 | 位置 | 修改前 | 修改后 |
|:-----|:-----|:-------|:-------|
| spec-contract.md | 序言·业务目标表 | 满足 EAL5+，端到端加密... | 满足 EAL6+（SE050 认证等级），端到端加密... |
| spec-contract.md | §1.9 KSS-SHALL-04 | SE050 SHALL 满足 EAL5+（认证等级）及以上 | SE050 SHALL 满足 EAL6+（安全芯片认证等级）及以上 |

### 状态
- `safety-concept.md`: ✅ 无需修改，已正确定义 EAL6+
- `spec-contract.md`: ✅ 已统一为 EAL6+

---

## DOC-P1-02: FTTI 冲突

### 问题描述
- SG-01 (非预期解锁防护) 定义 FTTI **<500ms**
- 验收矩阵第4节"被动解锁—解锁响应时间"标注 **≤ 1s**
- 两个值时序约束不一致，导致验收标准与安全目标脱节

### 修复内容
| 文件 | 位置 | 修改前 | 修改后 |
|:-----|:-----|:-------|:-------|
| spec-contract.md | §4 验收矩阵·被动解锁 | ≤ 1s（靠近→解锁） | <500ms（靠近→解锁） |

### 说明
验收矩阵的解锁响应时间与 Safety Goal SG-01 的 FTTI 统一为 <500ms，确保验收标准与安全目标一致。

---

## DOC-P1-03: Spec ID 前缀冲突

### 问题描述
- §1.6 钥匙分享 使用 `KS-` 前缀（`KS-SHALL-01` ~ `KS-SHALL-07`）
- §1.9 密钥存储与安全 同样使用 `KS-` 前缀（`KS-SHALL-01` ~ `KS-SHALL-08`）
- 两套 `KS-SHALL-01` ~ `KS-SHALL-07` ID 完全重复，无法唯一标识

### 修复内容
§1.9 密钥存储与安全的所有 ID 前缀统一改为 **`KSS-`**（Key Storage Security），共涉及 10 个 SHALL/SHALL NOT 条目。

| 旧 ID | 新 ID |
|:------|:------|
| KS-SHALL-01 | KSS-SHALL-01 |
| KS-SHALL-02 | KSS-SHALL-02 |
| KS-SHALL-03 | KSS-SHALL-03 |
| KS-SHALL-04 | KSS-SHALL-04 |
| KS-SHALL-05 | KSS-SHALL-05 |
| KS-SHALL-06 | KSS-SHALL-06 |
| KS-SHALL-07 | KSS-SHALL-07 |
| KS-SHALL-08 | KSS-SHALL-08 |
| KS-SHALL-NOT-01 | KSS-SHALL-NOT-01 |
| KS-SHALL-NOT-02 | KSS-SHALL-NOT-02 |

§1.6 钥匙分享的 `KS-` 前缀保持不变。

---

## 验证结果

```bash
# EAL 等级一致 ✅
spec-contract.md:  EAL6+（SE050 认证等级）
spec-contract.md:  EAL6+（安全芯片认证等级）
safety-concept.md: EAL6+ 认证
safety-concept.md: EAL6+ (信赖芯片厂商)

# FTTI 一致 ✅
验收矩阵被动解锁: <500ms（靠近→解锁）— 对齐 SG-01

# 前缀冲突已修复 ✅
§1.6 KS-SHALL: 9 条
§1.9 KSS-SHALL: 10 条（旧 KS- 前缀全部更替）
无重复 KS- ID
```

---

## 影响范围

| 文件 | 修改行数 | 影响章节 |
|:-----|:---------|:---------|
| `spec-contract.md` | 12 行 (1+1+10) | 序言业务目标、§1.9 KSS-SHALL-01~08/NOT-01~02、§4 验收矩阵 |
| `safety-concept.md` | 0 行 | 无修改（原有 EAL6+ 正确） |

## 约束遵守情况
- ✅ 未改变 spec 语义 — 仅修正冲突值，不做功能变更
- ✅ OpenSpec 格式完整 — 所有表格、ID 结构保持
- ✅ ASIL 等级不变 — 仅修正 EAL 认证等级
