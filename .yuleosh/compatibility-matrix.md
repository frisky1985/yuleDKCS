# 版本兼容性矩阵

> yuleDKCS 数字钥匙系统 — 各端版本对应关系、协议支持、平台要求
> **最后更新**: 2026-07-07

---

## 1. 各端版本对应

| yuleDKCS 版本 | 嵌入式固件 | Android SDK | iOS SDK | Hub (Go) | DKCS (Go) | Java Adapters |
|:-------------:|:----------:|:-----------:|:-------:|:--------:|:---------:|:-------------:|
| **1.0.0**     | v1.0.0     | v1.0.0      | v1.0.0  | v1.0.0   | v1.0.0    | v1.0.0        |
| **1.0.0-rc.1**| v1.0.0-rc  | v1.0.0-rc   | v1.0.0-rc| v1.0.0-rc| v1.0.0-rc | v1.0.0-rc    |

> 各端版本必须匹配主版本号。不匹配的组合可能导致协议不兼容。

## 2. 协议版本支持

| 协议 | 版本 | 嵌入式 | Android SDK | iOS SDK | 云端 | 状态 |
|:----|:----|:------:|:-----------:|:-------:|:----:|:----:|
| **ICCE** | T/CA 110-2020 | ✅ | ✅ | ✅ | ✅ | BLE 协议层双端就绪；SM 算法库待完整集成 |
| **CCC** | DK 3.0 Release 1 | ✅ | ✅ | ✅ | ✅ | 全部通过 |
| **ICCOA** | DK 3.0 | ✅ | ✅ | ✅ | ✅ | Android/iOS SDK BLE 协议层均就绪 |
| **ICCOA** | DK 4.0 | ✅ | ✅ | ✅ | ✅ | Android/iOS SDK BLE 协议层均就绪 |

> ⚠️ ICCE 国密算法（SM2/SM3/SM4）当前为部分实现，已识别为 P0 缺陷（已修复占位逻辑，但完整 SM 算法库需额外集成）。

## 3. 操作系统 / 平台要求

### 3.1 手机端

| 平台 | 最低版本 | BLE | UWB | NFC (Reader) | NFC (HCE) |
|:----|:--------:|:---:|:---:|:------------:|:---------:|
| **iOS** | 15.0 | ✅ (CoreBluetooth) | ✅ (iPhone 11+, NearbyInteraction) | ✅ (CoreNFC) | ❌ (仅 SE) |
| **Android** | 10 (API 29) | ✅ | ✅ (API 31+) | ✅ | ✅ (HCE) |

### 3.2 车端（嵌入式）

| 组件 | 型号 | 接口 |
|:----|:----|:----|
| **MCU** | NXP S32G2 / S32G3 | - |
| **BLE** | NXP KW47A (KW38 兼容) | BLE 5.0+, GATT Profile |
| **UWB** | NXP NCJ29D6 | IEEE 802.15.4z HRP |
| **NFC** | ST ST25R501 | ISO 14443 Type A/B, FeliCa |
| **安全芯片** | NXP SE050 | EAL5+, SM2/ECC/AES |
| **可信固件** | TFM (Trusted Firmware-M) | 安全启动链 |

### 3.3 云端

| 组件 | 版本要求 | 说明 |
|:----|:---------|:-----|
| Go | 1.22+ | Hub + DKCS |
| Java | 17+ | Spring Boot 3.2 Adapters |
| PostgreSQL | 15+ | 主数据库 |
| Redis | 7+ | 缓存 + 分布式锁 |
| Kafka | 3.6+ | 消息队列 |
| Kubernetes | 1.28+ | 容器编排 |
| Docker | 24.0+ | 容器运行时 |

### 3.4 开发环境

| 组件 | 版本要求 |
|:----|:---------|
| Xcode | 15.0+ (iOS 开发) |
| Android Studio | 2023.2+ |
| CocoaPods | 1.14+ |
| CMake | 3.20+ (嵌入式) |
| GCC/Clang | 支持 C11 (嵌入式) |
| Docker Compose | 2.20+ (本地开发) |

## 4. 密钥兼容性

| 密钥类型 | ICCE | CCC | 说明 |
|:--------|:----:|:---:|:-----|
| 主钥匙 (Owner) | ✅ | ✅ | 全部权限，可分享 |
| 副钥匙 (Friend) | ✅ | ✅ | 限制权限，不可分享 |
| 临时钥匙 (Temporary) | ✅ | ✅ | 时间/次数/地理限制 |
| NFC 离线钥匙 | ✅ | ✅ | 不依赖手机电量 |

## 5. 通信加密兼容

| 通道 | 协议 | 加密 | 认证 |
|:----|:----|:----|:----|
| App ↔ Hub | HTTPS | TLS 1.3 | JWT Bearer |
| Hub ↔ DKCS | gRPC | TLS (双向) | mTLS |
| DKCS ↔ TCU | MQTT | TLS | 证书 |
| 手机 ↔ 车端 (BLE) | BLE GATT | AES-256-GCM / SM4 | ECDSA / SM2 |
| 手机 ↔ 车端 (UWB) | IEEE 802.15.4z | AES-128 | Secure Ranging |
| 手机 ↔ 车端 (NFC) | ISO 7816-4 | AES-256-GCM | 会话密钥 |

## 6. 已知不兼容

| 场景 | 说明 | 状态 |
|:----|:-----|:----:|
| UWB — Android < API 31 | 必须 Android 12+ 才有原生 UWB API | 无法绕过 |
| UWB — iPhone < 11 | iPhone XS 及更早型号不支持 UWB (NearbyInteraction) | 无法绕过 |
| NFC HCE — iOS | iOS 不支持卡模拟模式，CCC 使用 iOS SE 完成 NFC 交互 | 架构限制 |
| ICCE 国密 — go 标准库 | Go 标准库不支持 SM2/SM3/SM4，需额外引入国密库 | 待集成 |
| ⚠️ ICCE SM 库 — iOS/Android | ICCE BLE 协议层已就绪，SM 算法库待集成 | 需引入 tjfoc/gmsm 后确认 |
