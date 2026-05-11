# yuleDKCS 项目结构说明

本项目采用分层架构设计，按照业务模块划分为四个主要部分：

```
yuleDKCS/
├── backend/          # 后端服务
├── frontend/         # 前端管理后台
├── mobile/           # 移动SDK (Android/iOS/Flutter)
├── embedded/         # 嵌入式SDK
├── docs/             # 文档
├── deploy/           # 部署配置
└── README.md         # 项目主说明
```

## 目录结构详解

### backend/ - 后端服务
车云一体化后端，提供API服务、数据库管理、MQTT消息中介等。

```
backend/
├── api/              # API定义
├── cmd/              # 启动命令
├── internal/         # 内部实现
├── database/         # 数据库schema和迁移
├── mqtt/             # MQTT配置
├── ai-anomaly/       # AI异常检测服务
├── tests/            # 后端测试
└── deploy/           # 后端部署脚本
```

### frontend/ - 前端管理后台
React + TypeScript 构建的Web管理后台，用于钥匙管理、车辆管理、数据可视化等。

```
frontend/
├── src/
├── public/
├── package.json
└── tsconfig.json
```

### mobile/ - 移动SDK
数字钥匙移动端SDK，支持Android、iOS和Flutter。

```
mobile/
├── android/          # Android SDK (Kotlin)
├── ios/              # iOS SDK (Swift)
└── flutter/          # Flutter SDK
```

### embedded/ - 嵌入式SDK
车载嵌入式SDK，提供CCC/ICCOA/ICCE协议实现。

```
embedded/
├── sdk/              # SDK核心代码
├── include/          # 头文件
├── src/              # 源代码
├── tests/            # 嵌入式测试
├── examples/         # 示例代码
└── tools/            # 开发工具
```

### docs/ - 文档
项目相关文档，包括架构设计、协议规范、API文档、使用指南等。

```
docs/
├── architecture/     # 架构文档
├── protocol-specs/   # 协议规范
├── api/              # API文档
├── guides/           # 使用指南
└── reports/          # 测试报告
```

### deploy/ - 部署配置
各环境部署配置文件和脚本。

```
deploy/
├── docker/           # Docker配置
├── k8s/              # Kubernetes配置
├── scripts/          # 部署脚本
└── configs/          # 环境配置
```

## 开发规范

1. 各模块独立维护，通过API协议通信
2. 每个模块应包含独立的README和构建说明
3. 测试代码与业务代码分离
4. 文档与代码同步更新

## 版本信息

- 当前版本: v2.0.0
- 最后更新: 2026-05-11
