# Go 安全漏洞修复报告

**日期**: 2026-07-18
**扫描**: govulncheck

## 修复项

| 漏洞 | 影响 | 修复前 | 修复后 |
|------|------|:------:|:------:|
| GO-2026-5856 crypto/tls 高危 | TLS 握手拒绝服务 | go1.26.3 | **go1.26.5** ✅ |
| GO-2026-5039 net/textproto 中危 | 未转义输入泄露 | go1.26.3 | **go1.26.5** (>=1.26.4) ✅ |
| GO-2026-5037 crypto/x509 | 证书校验效率 | go1.26.3 | **go1.26.5** (>=1.26.4) ✅ |
| GO-2025-3540 go-redis 中危 | CLIENT SETINFO 超时乱序 | v9.5.1 | **v9.21.0** ✅ |

## 改动文件
- `backend/dkcs/go.mod` → toolchain + redis升级
- `backend/cloud/hub/go.mod` → toolchain升级
- `go.work` → toolchain升级
- `.github/workflows/ci.yml` → GO_VERSION 1.25→1.26

## 验证
- `go test ./...` — 全部通过 ✅
- `go vet` — 零警告 ✅
- 所有覆盖率保持 ✅
