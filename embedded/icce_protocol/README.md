# ICCE数字钥匙协议栈

基于ICCE规范的边缘计算数字钥匙实现，支持BLE 5.0 + UWB FiRa + 车端TCU集成。

## 目录结构

```
icce_protocol/
├── docs/
│   ├── technical_specification.md  # 技术规格书 (18KB)
│   └── module_design.md            # 模块设计 (38KB)
├── include/
│   └── icce_digital_key.h          # 公共头文件 (6.8KB)
├── src/
│   ├── icce_dk_core.c              # 核心调度+区域切换
│   ├── icce_edge.c                 # 边缘计算规则引擎
│   ├── icce_zone.c                 # 距离分区管理
│   ├── icce_uwb.c                  # UWB FiRa测距 (NCJ29D6)
│   ├── icce_vehicle.c              # 车辆TCU控制
│   └── icce_security.c             # 安全模块 (SE050)
├── tests/
└── CMakeLists.txt
```

## 核心特性

- **边缘计算规则引擎**: 本地决策，无需云端交互
- **5级距离分区**: FAR→MID→NEAR→VICINITY→INTERIOR
- **UWB FiRa**: 支持多会话并发测距
- **BLE 5.0**: 长距离唤醒+数据通道
- **TCU集成**: CAN总线车辆控制

## 分区规则

| 分区 | 距离范围 | 触发动作 |
|------|---------|---------|
| FAR | 20-50m | 寻车 |
| MID | 10-20m | 启动UWB测距 |
| NEAR | 3-10m | 接近通知 |
| VICINITY | 1-3m | 解锁/闭锁 |
| INTERIOR | <1m | 启动引擎（全权限）|
