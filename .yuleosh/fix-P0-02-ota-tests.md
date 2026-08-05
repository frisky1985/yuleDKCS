# P0-02 修复记录: OTA 测试完全缺失

## 问题
OT-SHALL-01/02/03/OT-SHALL-NOT-01 共 4 项需求无专项测试。

## 修复
创建了以下测试文件：

### 1. E2E 集成测试
- `backend/cloud/hub/tests/integration/scenarios/e2e_06_ota_update_test.go`
  - E2E-06-01: OTA正常升级流程 (OT-SHALL-01)
  - E2E-06-02: 签名校验失败拒绝 (OT-SHALL-NOT-01)
  - E2E-06-03: 无签名包拒绝 (OT-SHALL-02)
  - E2E-06-04: OTA状态查询 (OT-SHALL-03)
  - E2E-06-05: ECDSA P-256签名验证 (OT-SHALL-02)
  - E2E-06-06: 数据篡改检测 (OT-SHALL-NOT-01)

### 2. DKCS 单元测试
- `backend/dkcs/internal/service/ota_service_test.go`
  - TestOTAShall01_Support: OTA升级支持验证
  - TestOTAShall02_SignatureVerify: 签名验证 (valid/wrong/tampered)
  - TestOTAShallNot01_RejectBadSignature: 表驱动拒绝测试
  - TestOTAShall03_StateMachine: 状态机覆盖

### 3. Mock 扩展
- 在 `testSuite/mock_tcu.go` 中增加了 OTA 模拟方法:
  - OTAPackage 结构体 (Version/Hash/Signature/Data/Size)
  - VerifyOTAPackage() — 模拟 SE050 签名验证
  - StartOTADownload() / InstallOTAPackage() / GetOTAPackageStatus()

## 验证
- ✅ go build ./... 全部通过
- ✅ 所有 OT-SHALL 需求均有对应测试
- ✅ Mock 模式，无外部依赖
