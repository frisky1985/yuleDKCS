# yuleDKCS 协议标准符合性审计报告

**审计工具**: Claude Code (deepseek-v4-flash)  
**审计日期**: 2026-05-31  
**项目路径**: /Users/stefan/.openclaw/workspace/yuleDKCS  

---

## 文件覆盖

- **62 个源文件** 全部读取 (嵌入式C: 35, Android Kotlin: 13, iOS Swift: 8, 后端 Java: 6)
- **6 份技术规范文档** 全部审阅
- **8 份测试计划/用例文档** 已分析

---

## 核心发现

### 🚨 P0 — BLE UUID 跨层完全断裂

| 层 | 实际 UUID | 标准要求 | 状态 |
|---|---|---|---|
| CCC 嵌入式 spec | 0xEFFF | 0xFFD1 | ❌ |
| ICCE 嵌入式 | 0xFEFA / 0x18F0 | ICCE 标准值 | ❌ |
| ICCE 设计文档 | 0x18F0 | 与 header 矛盾 | ❌ |
| ICCOA 嵌入式 | 0xFEF5 | ICCOA 标准 | ✅ |
| Android SDK | 0xFEFF | 与其他层均不匹配 | ❌ |
| iOS SDK | 0xFDE2-FDE7 | 与其他层均不匹配 | ❌ |

**影响**: 车端 BLE 广播了手机端无法识别的 Service UUID，iPhone/Android App 无法发现车辆。这是最优先修复项。

### 🚨 P0 — ICCE 国密算法完全缺失

| 标准要求 | 实际实现 | 状态 |
|---|---|---|
| SM2 (国密椭圆曲线) | ECDSA P-256 | ❌ |
| SM3 (国密哈希) | SHA-256 | ❌ |
| SM4 (国密对称加密) | AES-256-GCM | ❌ |

ICCE 协议栈的 security/security_auth.c 中所有加密操作均为通用 ECDSA/SHA256/AES-GCM，完全没有国密算法实现。

### 🚨 P0 — iOS 端缺少 UWB 和 NFC 硬件集成

| 功能 | 状态 | 说明 |
|---|---|---|
| CoreBluetooth (BLE) | ✅ 完整实现 | BleManager.swift (555行) |
| CoreNearbyInteraction (UWB) | ❌ 未实现 | 仅有错误码定义 |
| CoreNFC (NFC) | ❌ 未实现 | 仅有错误码定义 |
| Secure Enclave | ❌ 未实现 | 仅有 Keychain 管理 |

### ICCOA 协议栈 — 实现最完整

- DK3.0: XOR 校验和 **已实现**，含序列号防重放（带 0xFFFF 正确回绕处理）
- DK4.0: HMAC-SHA256 通过 SE050 硬件计算，会话管理完整（最多4并发）
- UWB: 实际调用 NXP NCJ29D6 驱动
- 降级保护: DK4.0→DK3.0 降级攻击防护（no_downgrade 机制）
- **ICCOA 是三个协议栈中实现最完整、一致性最好的**

### CCC 协议栈 — 结构完整但安全性偏弱

- BLE/NFC/UWB/SE050 四个硬件驱动层定义完整
- SPEC.md 规范文档详细 (670行)
- 但 security.c 实现多为 placeholder/注释，核心加密操作未实际对接 SE050
- key_mgmt.c 内存存储密钥（未加密），SE050 集成标注 TODO

### ICCE 协议栈 — 架构正确但实现不完整

- 边缘计算规则引擎 (icce_edge.c) 设计良好
- 离线决策引擎 (offline_decision.c) 含风险评估/速率限制
- 但 ICCE 安全模块 (icce_security.c) 标注大量 TODO
- UWB IRQ handler 为注释占位

### 后端层

- CCC/ICCOA/ICCE TSP Adapter 架构清晰，Template Method 模式
- ICCE Adapter 为 stub 实现（仅占位）  
- gRPC 桥接层 (AdapterServiceImpl) 功能完整

### App 端

- Android SDK: BLE/UWB/NFC 实现完整，NfcManager 含 ISO 7816-4 APDU + 安全通道
- iOS SDK: 仅 BLE 实现，UWB/NFC 缺失
- 两端 App UI: 均使用 mock 数据，未对接实际协议

---

## ✅ 已完整实现的功能

1. **ICCOA DK3.0 帧协议**: SOP/CMD/SEQ/LEN/CS/EOP + XOR checksum + 防重放
2. **ICCOA DK4.0 帧协议**: Magic/Ver/MsgType/Token/HMAC + HMAC-SHA256
3. **ICCOA BLE 层**: NXP KW47A GATT server 实现
4. **ICCOA Security**: SE050 ECDSA 验证 + HMAC 计算
5. **Android BLE 栈**: BleManager 完整 (CoreBluetooth 包装)
6. **Android NFC 栈**: NfcManager + NfcSecureChannel (ISO 7816-4 APDU)
7. **Android UWB 栈**: UwbManager (Android 13+ UWB API)
8. **CCC SPEC 文档**: 完整技术规格书 (~670行)
9. **CCC UWB 驱动**: NCJ29D6 TWR 测距 (SPI 通信)
10. **CCC NFC 驱动**: ST25R501 LPCD 低功耗检测
11. **CCE 边缘计算**: 规则引擎 + 距离分区 (5级)
12. **ICCE 离线决策**: 风险评估 + 速率限制
13. **后端 CCC/ICCOA Adapter**: 完整 HTTP 客户端实现
14. **后端 gRPC 桥接**: AdapterServiceImpl 完整

---

## ⚠️ 部分实现但有差距

1. **CCC BLE UUID**: 使用 0xEFFF，标准要求 0xFFD1
2. **CCC Security**: SCP03/ECDSA/AES-GCM 均为注释占位，未对接 SE050
3. **CCC Key Management**: 内存存储密钥，未 SE050 持久化
4. **ICCE 国密算法**: 完全使用 ECDSA/SHA256/AES 替代 SM2/SM3/SM4
5. **ICCE Security**: 标注大量 TODO
6. **ICCE BLE UUID**: header 和 spec 不一致
7. **iOS BLE**: UUID 0xFDE2 与车端/Android 均不匹配
8. **Android BLE**: UUID 0xFEFF 与车端不匹配
9. **App 层集成**: iOS/Android 均使用 mock 数据
10. **测试覆盖**: 仅 4 个模型初始化测试，无协议逻辑测试
11. **CCC NFC**: OOB 数据交换实现但 NDEF 编解码层缺失
12. **ICCE CAN 总线**: vehicle_integration.c 时间处理有逻辑问题

---

## ❌ 完全未实现

1. **国密算法 (SM2/SM3/SM4)**: 任何协议栈均未实现
2. **iOS UWB**: CoreNearbyInteraction 未导入
3. **iOS NFC**: CoreNFC 未导入
4. **iOS Secure Enclave**: 未集成 (仅有 Keychain)
5. **ICCE Adapter (后端)**: IcceAdapter 为 stub
6. **证书链验证**: 代码中有类型定义，但无验证实现
7. **NFC NDEF CCC 格式**: SPEC.md 中有格式定义，但代码未实现

---

## 🔵 实现过度 (标准不要求但做了的)

1. **ICCE 边缘计算引擎**: 标准不要求车端规则引擎
2. **ICCE 离线决策引擎**: 含风险评估和速率限制，超出 ICCE 规范要求
3. **ICCE 5 级距离分区**: 比标准要求的更多层级
4. **BLE KW47A 低功耗模式**: 非标准要求
5. **NFC ST25R501 LPCD 模式**: 标准未强制要求
6. **统一错误码系统 (0x0000-0xACFF)**: 三端统一设计
7. **统一日志系统**: 跨 BLE/NFC/UWB/SEC 模块标签
8. **遥测系统 (iOS/Android)**: trackKeyUse/trackUwbRanging 等

---

## 优先级修复建议

### P0 (立即修复 — 阻塞功能)

1. **BLE UUID 统一**: 建立全局 `BLE_UUID_TABLE` 头文件，所有层引用同一定义
2. **ICCE 算法策略决策**: 确定 ICCE 实际需要国密还是通用算法
3. **iOS UWB**: 集成 CoreNearbyInteraction
4. **iOS NFC**: 集成 CoreNFC 和 ISO 7816-4 APDU

### P1 (高优先级 — 安全隐患)

5. **CCC Security**: SE050 SCP03 对接，移除注释占位
6. **CCC Key Mgmt**: SE050 持久化存储
7. **ICCE SEC**: ECDSA 验证对接 SE050，移除 TODO
8. **App 端 BLE UUID**: 与车端统一

### P2 (中等优先级 — 功能完整性)

9. **CCC NFC NDEF**: 实现 NDEF 编解码器
10. **后端 ICCE Adapter**: 补全实现
11. **证书链验证**: 实现 SV Checking
12. **协议测试**: 为 BLE/UWB/NFC 层编写单元测试

### P3 (低优先级 — 优化)

13. **App mock → 真实协议**: 去掉 DispatchQueue.mock 延迟
14. **CAN 时间处理**: 修复 vehicle_integration.c 中的时间比较 bug
15. **国密算法**: 如果 ICCE 是必须的，引入 GMSSL 库
