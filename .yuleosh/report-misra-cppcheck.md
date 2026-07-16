# MISRA C:2023 cppcheck 质量门禁配置报告
> 任务: P2.1 / TD-05
> 日期: 2026-07-08

## 完成内容

### 1. `.cppcheck` 规则文件
**路径**: `embedded/.cppcheck`

配置内容：
- MISRA C:2023 addon (`--addon=misra`)
- 启用 style/warning/performance/portability 检查
- 现有 MISRA violations 的基线抑制规则（按文件/目录）
- 包含路径和编译定义 for freestanding 编译

#### 基线抑制范围
| 抑制类别 | 文件/目录 | 说明 |
|:---------|:----------|:-----|
| Dir-4.12 | cache_manager.c, offline_decision.c | 允许 bounded pool allocator |
| Rule 8.7 | 全部协议 Include 目录 + crypto | API 函数跨文件调用 |
| 15.6 | CCC 驱动代码 | compact guard 模式 |
| 17.7 | CCC security.c, key_mgmt.c | SE050 回调签名匹配 |
| Rule 10.x | crypto 模块 | 纯计算/位操作例外 |
| Rule 21-21 | cache_manager.c | bounded malloc |

### 2. GitHub Actions CI 工作流
**路径**: `.github/workflows/misra-ci.yml`

工作流功能：
- 在 `push` (main/develop/feat/**) 和 `pull_request` 上触发
- 安装 cppcheck 并验证 MISRA addon
- 分 4 个 job 运行 ICCE / CCC / ICCOA / Unified 协议栈扫描
- 使用 `embedded/.cppcheck` 基线规则
- 结果输出日志并上传 artifact
- PR 中自动报告 violations 数量

## 使用方法
```bash
# 本地验证
cppcheck --addon=misra --suppressions-list=embedded/.cppcheck \
  -I embedded/freestanding_includes -I embedded/icce_protocol/include \
  -D__freestanding__ -DCONFIG_ENABLE_CRYPTO=1 \
  embedded/icce_protocol/src/
```

> **注意**: 该质量门禁仅阻止新增 MISRA 违规，现有违规已通过 `.cppcheck` 基线化。

## 🔧 修复记录 (2026-07-08): P0 — MISRA CI 非阻塞门禁

### 问题
`misra-ci.yml` 中全部 4 个 cppcheck 调用使用 `--error-exitcode=0`，导致无论发现多少 MISRA 违规，步骤永远返回成功。门禁形同虚设。

### 修复内容
1. **`--error-exitcode=1`**: 全部 4 个 cppcheck 调用从 `--error-exitcode=0` 改为 `--error-exitcode=1`
   - ICCE、CCC、ICCOA、Unified 协议各一个

2. **退出码捕获**: 每个 run 步骤使用 `set +e` + `PIPESTATUS` 捕获 cppcheck 实际退出码，通过 `$GITHUB_ENV` 传递到 Check 步骤
   - 避免单个模块失败中断后续模块扫描
   - 所有模块运行完成后统一判定

3. **基线化对比逻辑**: Check 步骤读取每个模块的退出码和日志
   - cppcheck 退出码 = 0 → 无新增违规 ✅
   - cppcheck 退出码 ≠ 0 → 有新增（未基线化）违规 ⚠️ → **构建失败**
   - 退出码已通过 `--suppressions-list=embedded/.cppcheck` 排除已知基线违规
   - 失败时输出全部新违规详细信息到 `$GITHUB_STEP_SUMMARY`

### 变更文件
- `.github/workflows/misra-ci.yml`: 修改 4 处 cppcheck 调用 + 重写 Check 步骤 + 修复 `$(wc -l)` 语法

### 验证方法
```bash
# 无新违规 → 应通过
cppcheck --suppressions-list=embedded/.cppcheck --error-exitcode=1 \
  --addon=misra ... embedded/icce_protocol/src/ && echo "PASS"

# 故意引入违规 → 应失败
cppcheck --error-exitcode=1 --addon=misra \
  embedded/icce_protocol/src/cache/cache_manager.c && echo "FAIL" || echo "PASS (expected fail)"
```
