# P0-03 修复记录: 安全启动链 KSS-SHALL-08 独立测试

## 问题
KSS-SHALL-08（安全启动链）无独立测试，追溯矩阵中标记为 ⚠️ 需见安全启动集成测试。

## 修复
创建 `backend/dkcs/internal/service/secure_boot_test.go`，包含 6 个测试函数：

| 测试函数 | 验证内容 |
|:---------|:---------|
| TestKSSShall08_SecureBootChain | 完整安全启动链：Boot ROM → BootLoader(SE050验签) → TFM → Application |
| TestKSSShall08_BootLoaderSignatureFail | BootLoader 签名篡改导致启动终止（场景 S-11） |
| TestKSSShall08_TFMSignatureFail | TFM 签名校验失败，启动终止 |
| TestKSSShall08_AppSignatureFail | Application 无签名，启动终止 |
| TestKSSShall08_ChainIntegrity | 逐级校验 — 任何一级签名失败都阻止启动（表驱动测试） |
| TestKSSShall08_Se050Verification | SE050 验签函数边界条件（nil image/nil key/short signature） |

### 安全启动链模型
```
Boot ROM (OEM Root Key burned in OTP)
  └→ BootLoader (signed by OEM Root, verified by Boot ROM using OEM Root Pub)
      └→ TFM (signed by BootLoader, verified by BootLoader using its Pub)
          └→ Application (signed by TFM, verified by TFM using its Pub)
```

## 验证
- ✅ go build ./... 全部通过
- ✅ go test -run TestKSSShall ./... 全部 PASS
- ✅ 覆盖正常路径 + 3 种异常路径 + 边界条件
