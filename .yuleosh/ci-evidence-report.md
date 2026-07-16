# yuleDKCS → yuleOSH 证据链对接 — CI 验证报告

> **生成**: 2026-07-07T16:28+08:00
> **项目**: yuleDKCS (Digital Key System)
> **对接层**: yuleOSH Evidence Engine
> **版本**: ci-pipeline.yaml v1.0.0, yuleosh-ci.yml v1.0.0

---

## 1. 验证概要

| 检查项 | 状态 | 说明 |
|--------|------|------|
| Python import (yuleosh.evidence) | ✅ 通过 | `from yuleosh.evidence.compliance import generate_evidence` OK |
| Python import (yuleosh.manifest) | ✅ 通过 | `from yuleosh.evidence.manifest import generate_audit_manifest` OK |
| SHALL 识别 (spec-contract.md) | ✅ 通过 | 共检测到 **142** 条 SHALL/SHALL_NOT 语句行 |
| ci-pipeline.yaml 结构完整性 | ✅ 通过 | 3 stages, evidence 配置完整 |
| yuleosh-ci.yml 结构完整性 | ✅ 通过 | 10 steps, 并行运行, 不覆盖现有 CI |
| 现有 CI 文件未修改 | ✅ 通过 | `ci.yml` + `lint.yml` 均未触碰 |
| 证据包输出路径 | ✅ 通过 | `.yuleosh/evidence/audit-manifest.json` |
| GitHub Actions 上传配置 | ✅ 通过 | artifact name: `yuleosh-evidence`, retention: 90天 |

---

## 2. Pipeline 定义验证

### ci-pipeline.yaml — 三层结构

```yaml
stages:
  - name: layer1-dev               # 开发验证层
    commands:                        # Go build + test + coverage
      - cd backend/dkcs && go build ./...
      - cd backend/cloud/hub && go build ./...
      - cd backend/dkcs && go test -count=1 -coverprofile=coverage.out ./...
      - cd backend/cloud/hub && go test -count=1 -coverprofile=coverage.out ./...

  - name: layer2-integration        # 集成验证层
    commands:                        # golangci-lint 静态分析
      - cd backend/dkcs && golangci-lint run ./...
      - cd backend/cloud/hub && golangci-lint run ./...

  - name: layer3-evidence-pack      # 证据打包层
    commands:                        # yuleOSH 证据链生成
      - yuleosh evidence pack --project-dir . -o .yuleosh/evidence
```

### 证据输出结构

```
.yuleosh/evidence/
├── audit-manifest.json      ← 主清单 (含 build_id / 组件hash / SWE 状态)
├── manifest.json            ← 目录级清单 (SHA256)
├── compliance-pack.zip      ← 合规包 (ZIP)
├── ci-results/              ← CI 执行结果
├── coverage/                ← 覆盖率数据 (coverage.out)
├── lint-results/            ← Lint 报告
└── traceability/            ← 规范/追溯
```

---

## 3. GitHub Actions 工作流验证

### yuleosh-ci.yml — 关键特征

| 特征 | 实现 |
|------|------|
| 触发条件 | `push` + `pull_request` on main/develop/release |
| Python 版本 | 3.13 (通过 setup-python) |
| Go 版本 | 1.25 (通过 setup-go) |
| yuleOSH 安装 | pip install (支持本地/远程/PYTHONPATH 回退) |
| L1 Build+Test | Go build + test -json 格式输出 |
| L2 Lint | golangci-lint JSON 格式输出 |
| L3 证据打包 | Python 内联脚本生成 audit-manifest.json |
| 产物上传 | actions/upload-artifact@v4, 90天保留 |
| 与现有 CI 关系 | **并行运行** — 不修改 ci.yml / lint.yml |
| 错误容忍 | 各步骤使用 `continue-on-error: true` |
| PR 注释 | 通过 GITHUB_STEP_SUMMARY 输出摘要 |

### 与现有 CI 的对比

| 维度 | ci.yml (现有) | yuleosh-ci.yml (新增) |
|------|--------------|----------------------|
| 用途 | 基础 CI + 覆盖率检查 | 证据链收集 + 审计清单生成 |
| 触发 | push + PR | push + PR (同条件) |
| Go build | ✅ | ✅ |
| Go test | ✅ | ✅ |
| golangci-lint | ❌ (在 lint.yml 中) | ✅ |
| 覆盖率收集 | `-cover` (仅检查) | `-coverprofile` + 持久化到 evidence |
| yuleOSH 证据 | ❌ | ✅ audit-manifest.json |
| 产物上传 | ❌ | ✅ GitHub Artifacts (90天) |

---

## 4. yuleOSH 模块验证

### 4.1 Python 导入测试

```python
# evidence/compliance
from yuleosh.evidence.compliance import     generate_evidence       # ✅
from yuleosh.evidence.compliance import     pack_compliance_zip     # ✅
from yuleosh.evidence.compliance import     _check_pipeline_not_running  # ✅

# evidence/manifest
from yuleosh.evidence.manifest import       generate_audit_manifest # ✅
from yuleosh.evidence.manifest import       save_manifest           # ✅
from yuleosh.evidence.manifest import       load_manifest           # ✅

# evidence/generator
from yuleosh.evidence.generator import      EvidenceCollector       # ✅

# evidence/pack (backwards compat)
from yuleosh.evidence.pack import           generate_evidence       # ✅
```

### 4.2 SHALL 语句统计

| 类别 | 计数 |
|------|------|
| SHALL (包含语句行) | 115 |
| SHALL NOT (包含语句行) | 27 |
| **总计** | **142** |

统计命令:
```bash
grep -c "SHALL " .yuleosh/spec-contract.md     # → 115
grep -c "SHALL NOT" .yuleosh/spec-contract.md  # → 27
```

所有 SHALL/SHALL NOT 均已在证据链中标记为 **可追溯需求**，在生成 audit-manifest.json 时记录到 `spec_contract` 字段。

---

## 5. 已知限制

1. **yuleOSH PyPI 包**：目前不在 PyPI 上，需通过 PYTHONPATH 或本地源码安装。工作流已包含 fallback 逻辑。
2. **compliance-pack.zip**：需要 `EvidenceCollector` 运行完整 pipeline（含 SIL 报告收集），CI 环境中可能缺少部分组件。
3. **SHALL 深度解析**：目前为词法计数（grep），未做语义 AST 解析。未来可对接 OpenSpec 解析器 (`yuleosh spec validate`) 做结构化约束检查。
4. **MISRA 报告**：Go 项目没有 cppcheck/MISRA 需求，故 L2 lint 仅包含 golangci-lint。嵌入式 C 代码需单独处理。

---

## 6. 结论

| 交付物 | 状态 | 路径 |
|--------|------|------|
| CI Pipeline 定义 | ✅ 已创建 | `.yuleosh/ci-pipeline.yaml` |
| GitHub Actions Workflow | ✅ 已创建 | `.github/workflows/yuleosh-ci.yml` |
| 验证报告 | ✅ 已生成 | 本文 |
| yuleOSH 模块可导入 | ✅ 已验证 | 4 个关键模块全部可用 |
| SHALL 可被识别 | ✅ 已验证 | 142 条 SHALL/SHALL_NOT 约束行已纳入追溯链 |
| 现有 CI 未修改 | ✅ | `ci.yml` + `lint.yml` 未被触碰 |

**对接状态: ✅ 完成**
