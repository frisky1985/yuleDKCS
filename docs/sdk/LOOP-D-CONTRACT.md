# Sprint Contract: Loop Batch D — ICCOA 状态模型 / CVE 扫描 / 审计更新

> 老板指令: "直接 loop"。来源: iccoa-compliance-audit P1 + TASK_STATUS 依赖扫描项。
> 裁决日期: 2026-08-01

---

## 工作流 1: ICCOA 钥匙状态模型补齐 + 审计更新（W1）

### 现状
- 审计 (iccoa-compliance-audit.md): ICCOA 状态模型缺 SUSPENDED/TERMINATED（pb.KeyStatus 无对应值）
- **已确认过时**: S2S 生产接入已由 main.go 环境变量完成（ICCOA_{VENDOR}_BASE_URL）, 审计"未接入"结论需更新

### 任务
1. proto: pb.KeyStatus 增加 SUSPENDED/TERMINATED 枚举值（注意现有枚举值编号, 保持向后兼容）→ 重新生成 pb.go
2. service/store: 支持 suspend/resume 设置 SUSPENDED, revoke 设置 TERMINATED（若现有逻辑只认 active/revoked 需扩展）
3. 单测: 状态流转（active→suspended→active, active→terminated）
4. 更新过时文档: iccoa-compliance-audit.md（S2S 接入状态 ✅）+ sdk-certification-checklist.md（阻塞项移除）

### 完成标准（AC-ST）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| ST-1 | proto 枚举 + 重新生成 | go build 通过 | W1 |
| ST-2 | 状态流转支持 + 单测 | go test 绿 | W1 |
| ST-3 | 审计文档更新 | 无过时结论 | W1 |

## 工作流 2: govulncheck CVE 扫描（W2）

### 现状
- TASK_STATUS P1: "依赖安全漏洞清零 ✅ go mod tidy + go vet 通过 | 需安装 govulncheck 做 CVE 深度扫描"

### 任务
1. 安装 govulncheck（go install golang.org/x/vuln/cmd/govulncheck@latest; 中国网络受限时用 GOPROXY=goproxy.cn 或 GONOSUMDB）
2. 扫描 backend/cloud/hub + backend/dkcs + backend/relay（如有独立 module）
3. 修复发现的高/危 CVE（依赖升级或 overrides, 最小改动）
4. 报告扫描结果

### 完成标准（AC-CVE）
| ID | Criterion | Pass | Owner |
|:--:|:----------|:-----|:------|
| CVE-1 | govulncheck 安装 | 可执行 | W2 |
| CVE-2 | 扫描结果记录 | 报告列出 | W2 |
| CVE-3 | 高/危 CVE 修复 | 修复后扫描干净（或记录不可修+理由） | W2 |

---

## Negotiation Log

| Round | Party | Action | Notes |
|:------|:------|:-------|:------|
| R1 | Generator | PROPOSE | 两线; W1 状态模型 + 审计更新; W2 安全扫描 |
| R2 | architect-lead | APPROVE | 枚举加值向后兼容; CVE 修复最小化 |
| R3 | Evaluator | APPROVE | 可验证 |
| R4 | 老板 | 确认 | 直接 loop |
| R5 | Evaluator | APPROVE | W1 状态模型: proto TERMINATED + 字符串常量统一 + 流转 (suspend→suspended/resume→active/revoke→terminated) + 10 断言 (含非 active 拒控车) + 1391 passed; 审计文档全面更新 (S2S 已接入 ✅ 移除 P0); W2 CVE: 依赖升级 (grpc/x-net/x-crypto 修复版) + govulncheck 三模块 0 漏洞 |
| R6 | 主导 | CLOSE | TASK_STATUS 依赖扫描 ✅; 剩余外部依赖: TLS 证书/Apple 证书/真机联调 |
