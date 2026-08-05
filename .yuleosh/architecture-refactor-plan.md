# yuleDKCS 架构重构方案 — yuleHUB + ASPICE 分层

> **目标**: 对标银基 yuleHUB + ASPICE 代码分层
> **当前**: single-protocol gateway → **目标**: multi-device integration platform

---

## ASPICE 四层代码结构

```
ASPICE Layer        yuleDKCS 模块              银基对标
──────────────────────────────────────────────────────────────
SWC (应用层)        service/ 业务逻辑           应用层逻辑
RTE (运行时环境)    api/ + gateway/             接口层
BSW (基础服务层)    device-mgmt/ key-mgmt/     yuleHUB 平台
MCAL (硬件抽象)     adapter-*/ embedded/        车端软硬件
```

## 目标目录结构

```
backend/
├── hub/                          ← yuleHUB 平台 (重构当前 hub)
│   ├── api/                      REST/gRPC API (OEM facing)
│   ├── protocol/                 ← 协议适配层 ★NEW
│   │   ├── ccc/                  CCC 协议适配
│   │   ├── icce/                 ICCE 协议适配
│   │   ├── iccoa/                ICCOA 协议适配
│   │   └── bridge/              设备厂商桥接 (一次对接复用)
│   ├── device/                   ← 设备管理 ★NEW
│   │   ├── registry/             设备注册/激活
│   │   ├── provisioning/         密钥预置
│   │   └── status/               设备状态监控
│   ├── oem/                      ← OEM 多租户管理 ★NEW
│   │   ├── tenant/               租户隔离
│   │   ├── branding/             品牌定制
│   │   └── config/               OEM 配置
│   ├── gateway/                  网关层 (路由/鉴权/限流)
│   ├── security/                 ← 安全监控层 VSoC ★NEW
│   │   ├── monitor/              实时威胁检测
│   │   ├── audit/                审计日志
│   │   └── alert/                告警管理
│   ├── diagnostics/              ← 诊断平台 yulePIN ★NEW
│   │   ├── tracing/              全链路追踪
│   │   ├── log-collector/        日志采集
│   │   └── health/               健康检查
│   ├── compliance/               ← 合规测试套件 (已有)
│   ├── pkg/                      公共工具包
│   └── cmd/                      入口
│
├── dkcs/                         核心服务 (不变)
│   ├── keymgmt/
│   ├── service/
│   ├── repository/
│   ├── mq/
│   └── ...
│
└── adapters/                     ← 保持现有结构
    ├── adapter-ccc/
    ├── adapter-icce/
    ├── adapter-iccoa/
    └── adapter-core/

embedded/                         ← 车端软件 (增加ASPICE注释)
├── icce_protocol/                 ICCE 协议栈
├── ccc_protocol/                  CCC 协议栈
├── iccoa_protocol/                ICCOA 协议栈
├── unified_protocol/              统一协议层
└── system_architecture/           ASPICE 需求→架构追溯

frontend/                         移动端 (不变)
├── android/
└── ios/
```

## 执行计划

### Step 1: 重构 Hub → yuleHUB (Refactor)
- 创建 hub/protocol/ 下 CCC/ICCE/ICCOA 适配器目录
- 从 adapters 的 Java 实现中提取接口定义到 protocol/bridge
- 迁移现有 internal/ 到新结构
- 保持向后兼容（旧路径保留 alias）

### Step 2: 新增模块骨架
- hub/device/ — 设备注册/预置/状态
- hub/oem/ — 多租户管理
- hub/security/ — 安全监控
- hub/diagnostics/ — yulePIN诊断埋点

### Step 3: ASPICE 注释标记
- 在每个 Go package 添加 layer 注释
- 在嵌入式 C 文件头添加 ASPICE 模块 ID
- 生成 LST (Layer Structure Table)
