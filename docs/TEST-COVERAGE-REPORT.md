# yuleDKCS 测试覆盖率报告

> 版本: v1.0 | 日期: 2026-07-27

## 当前状态

因网络代理不可用，`go test -cover ./...` 无法下载依赖，覆盖率数据待网络恢复后补充。

## 现有测试资产

| 项目 | 数量 |
|------|------|
| Go 测试文件 | 44 文件 (`*_test.go`) |
| 嵌入式测试 | 6 文件 |
| CI 测试 Workflow | ✅ ci.yml 已有 pytest/go test 步骤 |

## 建议

网络恢复后执行：
```bash
cd backend/dkcs && go mod tidy && go test -cover ./...
```

历史覆盖率参考：此前 Go 测试全部通过，44 个测试文件覆盖核心 DKCS/Hub 路径。
