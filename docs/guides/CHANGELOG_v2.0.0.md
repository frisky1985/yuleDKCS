# yuleDKCS v2.0.0 - 完整数字钥匙生态系统

## 🎉 重大更新

本次更新将 yuleDKCS 从嵌入式 SDK 扩展为完整的数字钥匙生态系统，包含云端服务、管理界面和移动端 SDK。

## 📦 新增功能

### Backend (Go 微服务)
- ✅ JWT/OAuth2 认证服务 (登录/注册/刷新 Token)
- ✅ 钥匙管理服务 (发行/分享/撤销/CCC/ICCE/ICCOA 协议)
- ✅ 车辆管理服务 (注册/状态/控制/远程命令)
- ✅ OTA 服务 (固件检查/下载/状态更新)
- ✅ WebSocket 实时通信 (车辆状态/命令结果回调)
- ✅ PostgreSQL + Redis 数据库
- ✅ Docker + Docker Compose 部署

### Frontend (React 管理界面)
- ✅ 认证页面 (登录/注册/忘记密码/第三方登录)
- ✅ Dashboard 仪表盘 (车辆概览/快捷操作/活动日志)
- ✅ 钥匙管理页面 (列表/分享/撤销/二维码)
- ✅ 车辆详情页面 (状态/控制/位置)
- ✅ Zustand 状态管理 + React Query 数据缓存

### Mobile SDK
#### iOS (Swift)
- ✅ Swift Package Manager 包管理
- ✅ KeyManager (钥匙发行/分享/撤销)
- ✅ BLEManager (CoreBluetooth 通信)
- ✅ CryptoWrapper (加密/解密)
- ✅ FFIBridge (C 库绑定)

#### Android (Kotlin)
- ✅ Gradle + JitPack 发布
- ✅ KeyManager (钥匙操作)
- ✅ BLEManager (Nordic BLE 库)
- ✅ CryptoWrapper (Android Keystore)
- ✅ JNI Bridge (原生库调用)

#### Flutter
- ✅ Dart FFI 绑定
- ✅ 跨平台统一 API
- ✅ 示例 App

## 📈 代码统计

| 组件 | 语言 | 代码行数 |
|------|------|---------|
| Backend | Go | 5,040 |
| Frontend | TypeScript/React | 2,376 |
| iOS SDK | Swift | 1,929 |
| Android SDK | Kotlin | 1,441 |
| Flutter SDK | Dart | 1,084 |
| **总计** | - | **11,870** |

## 🚀 快速开始

### 启动 Backend
```bash
cd backend
cp config.example.yaml config.yaml
docker-compose up -d
go run main.go
```

### 启动 Frontend
```bash
cd frontend
npm install
npm run dev
```

### 使用 iOS SDK
```swift
import YuleDKCS

YuleDKCS.initialize(apiKey: "your-api-key")
KeyManager.shared.issueKey(vehicleId: "VIN123") { result in
    // 处理结果
}
```

### 使用 Android SDK
```kotlin
import com.yuledkcs.YuleDKCS

YuleDKCS.initialize(context, apiKey)
YuleDKCS.keyManager.issueKey("VIN123") { result ->
    // 处理结果
}
```

## 📝 API 文档

详见 `backend/README.md` 和各 SDK 目录下的文档。

## 🎮 版本信息

- **版本**: v2.0.0
- **发布日期**: 2025-05-09
- **支持协议**: CCC, ICCE, ICCOA
