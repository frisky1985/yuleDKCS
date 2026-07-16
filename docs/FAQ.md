# yuleDKCS 常见问题 (FAQ)

> **版本**: 1.0.0 | **面向**: 集成商 / 终端用户 / 运维人员
> **最后更新**: 2026-07-08

---

## 一、协议兼容

### Q1: yuleDKCS 支持哪些数字钥匙协议？

ICCE（T/CA 110-2020，国密）、CCC Digital Key 3.0、ICCOA DK 3.0 & 4.0。系统可在车端自动协商协议，App 根据车辆 VIN 自动选用对应协议。

### Q2: 手机厂商（Apple、小米、OPPO）如何接入？

- **Apple Wallet (CCC)**: 通过云端 CCC 适配器（`adapter-ccc`）对接 Apple 数字钥匙服务
- **小米/OPPO (ICCOA)**: 通过云端 ICCOA 适配器（`adapter-iccoa`）对接
- **其他厂商 (ICCE)**: 通过云端 ICCE 适配器（`adapter-icce`）对接

> 新增厂商仅需新增一个 Java Adapter 模块，云端架构支持热插拔。

### Q3: ICCE 国密算法（SM2/SM3/SM4）当前实现状态？

⚠️ **部分实现**。ICCE 协议栈架构已完成，`security_auth/bind` 签名验证已从 TODO 占位符升级为真实验证循环。但国密算法（SM2/SM3/SM4）完整库集成待完成：
- **Go 端**: 当前使用 P-256/SHA-256 模拟 SM2/SM3，`github.com/tjfoc/gmsm` 库已识别但未引入（`backend/cloud/hub/tests/compliance/iccoa/iccoa_bind_test.go` 中有注释待启用代码）
- **Android SDK**: ICCE BLE 协议层已实现，SM 算法回调接口预留待集成
- **iOS SDK**: ICCE BLE 协议层已实现，SM 算法回调接口预留待集成
- **Java Adapter**: 完整架构就绪（`backend/adapters/adapter-icce/`），但 SM 库依赖未加入 pom.xml

完整 SM 库集成已列为独立 P1 待办。详见 [RELEASE_NOTES.md](RELEASE_NOTES.md) 已知问题第 2 条。

### Q4: 同一辆车可以同时支持多个协议吗？

可以。车端固件同时包含 ICCE、CCC、ICCOA 三个协议栈，配对时自动协商使用哪个协议。云端也同时部署三个协议的适配器。

---

## 二、安全

### Q5: 数字钥匙安全等级如何？

- 车端 SE050 安全芯片，EAL6+ 认证等级（文档已统一对齐）
- 手机端密钥存储在 Secure Enclave (iOS) / StrongBox KeyStore (Android)
- 通信加密：HTTPS TLS 1.3、gRPC 双向 TLS、MQTT TLS
- 防中继攻击：UWB Secure Ranging + 一次性 Nonce + 时间窗口校验
- 解锁指令仅在测距 < 2m 且信号往返时间在协议阈值内时执行

### Q6: 密钥数据存在哪里？云端能看到我的私钥吗？

**不。** DK Hub 只做授权决策，不持有密钥材料。密钥对在车端 SE050 和手机安全硬件中生成并存储，云端仅管理钥匙的权限和状态元数据。

### Q7: 手机丢了怎么办？

可立即通过另一台设备登录数字钥匙 App，远程吊销丢失设备上的数字钥匙。吊销指令即使车端离线，也会在车端下次联网时生效（静默吊销）。NFC 离线钥匙可在吊销后设置黑名单。紧急情况下可通过 OEM 客服热线挂失。

### Q8: 是否支持防中继/重放攻击？

是。UWB 安全测距（IEEE 802.15.4z）物理层检测真实距离，结合一次性随机数 (Nonce) 防重放。解锁指令仅在测距 < 2m 且信号往返时间在协议阈值内（约 3μs）时执行。ICCE 协议栈已实现 Challenge-Response 超时窗口（P1 已修复）。

### Q9: 是否满足 ISO 26262 功能安全？

关键功能（车门解锁、发动机启动、UWB 测距、密钥管理）目标 ASIL-B。当前 ASIL-B 安全机制在代码层尚未完全落地（识别为 P1 待办），安全概念 (safety-concept.md) 已定义 HARA 分析和 FTTI < 500ms。

---

## 三、部署

### Q10: 生产环境推荐部署方式？

生产环境建议 **分离部署** —— Hub 和 DKCS 独立运行（`--mode=hub-only` + `--mode=server-only`），部署于 Kubernetes 集群，各服务 3 副本 + HPA 自动扩缩容。Java Adapters 作为独立 Deployment 运行。详细部署参考 [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)。

### Q11: 依赖哪些基础设施？

| 组件 | 用途 | 建议副本/规模 |
|:----|:-----|:--------------|
| PostgreSQL 15+ | 持久化存储 | 1 Primary + N Replica |
| Redis 7+ | 缓存 + 分布式锁 | Cluster (3+) |
| Kafka 3.6+ | 消息队列 | 3-broker |
| Prometheus + Grafana | 监控 | 各 1 |
| Nginx Ingress | API 网关 + TLS | 2+ |

### Q12: 是否支持单体部署？

支持 `--mode=all-in-one` 模式，适合开发/测试。生产环境请务必使用分离部署模式。

### Q13: 云端服务的最低硬件要求？

- 开发环境: 2 vCPU, 4GB RAM 可运行全部服务
- 生产环境（单 Hub/DKCS Pod）: 建议 2 vCPU, 2GB RAM, 500m CPU request
- Java Adapter: 1 vCPU, 1GB RAM

---

## 四、故障排查

### Q14: 解锁响应时间超出 1 秒，可能原因？

1. UWB 测距异常：检查手机是否支持 UWB，以及是否已授予 UWB 权限
2. BLE 连接质量差：检查车端与手机之间的信号强度（RSSI > -70dBm 为佳）
3. 云端网络延迟：检查手机网络连接
4. 防中继检查失败：UWB 测距置信度低于阈值或距离超标
5. 车端 CAN 总线延时：检查车端 MCU 负载

### Q15: App 扫描不到车辆？

1. 确认手机已打开蓝牙
2. 确认手机与车辆距离在 BLE 覆盖范围内（通常 10m 以内）
3. 确认车辆 TCU 已上电、BLE 模块工作正常
4. 检查 App 位置权限是否已授予（Android BLE 扫描需要定位权限）
5. 检查车端 BLE 广播是否正常（可用 nRF Connect 或 LightBlue 工具扫描）

### Q16: 钥匙创建失败，提示"配对超时"？

1. 确保车机和手机网络正常（配对过程需要云端参与）
2. 确认配对码输入正确
3. 检查 OTA 通道是否通畅
4. 检查 SE050 剩余存储空间
5. 重试前建议等待 30 秒

### Q17: MQTT 指令下发无响应？

遵循以下排查顺序：
```
指令下发 → DKCS 日志输出 → MQTT Broker → TCU 在线状态 → 车端协议栈处理
```
1. 检查 DKCS 日志，确认指令是否已发出
2. 检查 Kafka/MQTT Broker 状态
3. 检查 TCU 网络连接（4G/5G 信号）
4. 检查 MQTT Broker（EMQX `backend/cloud/deploy/k8s/emqx.yaml`）Topic ACL 配置 — 默认配置使用客户端证书认证，无显式 Topic ACL。如需精细控制，参照 [operations-manual.md](operations-manual.md) 第 4 章配置 EMQX ACL 规则
5. 检查 TCU 证书是否过期 — 车端和设备证书使用 let's encrypt cluster-issuer（`backend/cloud/deploy/k8s/ingress.yaml`），TCU 证书过期检查需在 TCU OTA 管理系统中配置 CRL/OCSP 检查

### Q18: 如何处理钥匙分享后对方无法使用？

1. 确认分享的钥匙未过期
2. 确认接收方已成功领取钥匙（App → 钥匙列表）
3. 确认接收方手机已安装对应厂商 OEM App
4. 确认接收方手机是否支持所需的 BLE/UWB/NFC 能力
5. 检查分享权限设置（临时钥匙可能有时间/次数限制）

### Q19: 车端 OTA 升级失败怎么办？

1. 确认车辆处于安全状态（驻车、非行驶中）
2. 检查车端 4G/5G 网络信号
3. 检查 OTA 服务器端固件包完整性 (SHA256)
4. 如果升级中断，车端应自动回滚到上一版本
5. 联系 OEM 技术支持获取 OTA 日志

### Q20: 如何确认 CI 门禁是否通过？

查看 GitHub Actions 页面:
- `.github/workflows/ci.yml` — Go 端 build + test + coverage
- `.github/workflows/android-ci.yml` — Android lint + test + coverage + build
- `.github/workflows/ios-ci.yml` — iOS lint + test + build
- `.github/workflows/ci-java.yml` — Java checkstyle + test + coverage
- `.github/workflows/yuleosh-ci.yml` — yuleOSH 证据链收集

---

## 五、版本与兼容

### Q21: 各端版本必须严格匹配吗？

**是。** 各端版本号必须匹配主版本号（v1.x.x）。不匹配的组合可能导致协议不兼容。详见 [compatibility-matrix.md](compatibility-matrix.md)。

### Q22: 如何升级到新版本？

参考 [RELEASE_NOTES.md](RELEASE_NOTES.md) 中的升级注意事项。一般步骤：
1. 先升级云端服务（Hub → DKCS → Java Adapters）
2. 再升级手机 App（通过应用商店）
3. 最后通过 OTA 升级车端固件

### Q23: 版本更新是否向后兼容？

v1.0.0 对 v1.0.0-rc.1 完全向后兼容（已验证）。未来版本会保持 API 兼容性，Breaking Change 会在大版本升级时明确标注。

---

## 六、开发与贡献

### Q24: 如何搭建本地开发环境？

```bash
# 克隆仓库
git clone https://github.com/frisky1985/yuleDKCS.git
cd yuleDKCS

# 启动依赖服务
docker compose up -d

# Go 后端
cd backend/dkcs && go build ./...
cd backend/cloud/hub && go build ./...

# Android
cd frontend/android && ./gradlew build

# iOS
cd frontend/ios && xcodegen generate && xcodebuild

# Java Adapters
cd backend/adapters && mvn package
```

### Q25: 运行测试？

```bash
# Go
cd backend/dkcs && go test -count=1 -cover ./...

# Android
cd frontend/android && ./gradlew testDebugUnitTest

# iOS
cd frontend/ios && xcodebuild test -scheme DigitalKeySDK -sdk iphonesimulator

# Java
cd backend/adapters && mvn test

# 集成测试（需要 Docker 服务运行中）
cd backend/dkcs && go test -tags=integration -v -count=1 ./...
```

### Q26: 代码审查标准？

参考 `docs/design/CODE-REVIEW-V2.md` 和 `docs/reviews/` 下的相关规范。

---

## 七、其他

### Q27: yuleDKCS 和 yuleOSH 的关系？

**yuleDKCS** 是数字钥匙系统产品代码。**yuleOSH** 是质量编排引擎，通过 `ci-pipeline.yaml` + 证据链自动生成审计清单，实现 CI/CD 到审计跟踪的闭环。

### Q28: 是否存在开源协议限制？

当前为私有项目（Private — All rights reserved）。如需商业合作请联系项目组。

### Q29: 如何报告问题或提交改进建议？

参考 `CODE_OF_CONDUCT.md` 中的指引。安全相关问题请直接联系项目安全团队。

### Q30: 从哪里获取所有文档？

| 文档 | 位置 |
|:----|:-----|
| 系统架构 | `docs/SYSTEM_ARCHITECTURE.md` |
| API 参考 | `docs/API_REFERENCE.md` |
| 部署指南 | `docs/DEPLOYMENT_GUIDE.md` |
| 安全指南 | `docs/SECURITY_GUIDE.md` |
| 集成指南 | `docs/integration-guide.md` |
| 运维手册 | `docs/operations-manual.md` |
| 版本发布 | `docs/RELEASE_NOTES.md` |
| 变更日志 | `docs/CHANGELOG.md` |
| 兼容性矩阵 | `docs/compatibility-matrix.md` |
| 三端开发指南 | `docs/design/` |
| 代码审查 | `docs/reviews/` |
