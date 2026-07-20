# ICCOA数字钥匙协议栈

基于ICCOA DK 3.0/4.0规范的车端协议栈实现。

## 目录结构

```
iccoa_protocol/
├── docs/
│   └── SPEC.md              # 技术规格书
├── include/
│   └── iccoa_digital_key.h  # 公共头文件
├── src/
│   ├── iccoa/
│   │   ├── iccoa_dk_core.c  # 核心调度
│   │   ├── dk30/
│   │   │   └── iccoa_dk30.c # DK 3.0实现
│   │   └── dk40/
│   │       └── iccoa_dk40.c # DK 4.0实现
│   ├── auth/                # 授权认证 (待实现)
│   ├── ble/                 # BLE通信 (待实现)
│   └── service/             # 车辆服务 (待实现)
├── tests/
└── CMakeLists.txt
```

## 协议版本

- **DK 3.0**: BLE绑定/认证/控制，SOP/EOP帧格式
- **DK 4.0**: 增加UWB支持、多设备、远程分享，HMAC签名帧格式
