# P0-01 修复记录: cover-check.yml 接入 CI

## 问题
cover-check.yml 位于 `~/.yuleDKCS/cover-check.yml`（用户 HOME 目录），未接入 CI 流程。

## 修复
1. 将 `~/.yuleDKCS/cover-check.yml` 复制到 `~/yuleDKCS/.github/workflows/cover-check.yml`
2. 配置 cover-check.yml 支持 `workflow_call` 和 `pull_request` 触发器
3. 更新 `ci.yml`，在 pull_request 时调用 cover-check.yml 作为独立 job
4. cover-check.yml 包含 Hub 和 DKCS 两个模块的覆盖率检查和阈值门槛

## 验证
- ✅ cover-check.yml 已存在于正确路径
- ✅ ci.yml 通过 `uses: ./.github/workflows/cover-check.yml` 引用
- ✅ go build ./... 全部通过
