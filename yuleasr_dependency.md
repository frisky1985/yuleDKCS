# yuleASR 平台依赖关系

## 概述

yuleDKCS (Digital Key Connectivity System) 是 **上层应用**，yuleASR 是 **底层 AutoSAR BSW (Basic Software) 平台**。

```
┌───────────────────────────────────────────┐
│              yuleDKCS (应用层)              │
│  ┌─────────┐ ┌──────────┐ ┌────────────┐  │
│  │ CCC R3  │ │  ICCE    │ │   ICCOA    │  │
│  │ 协议栈   │ │ 协议栈   │ │  协议栈    │  │
│  └────┬────┘ └────┬─────┘ └─────┬──────┘  │
│       │           │             │          │
│  ┌────┴───────────┴─────────────┴──────┐   │
│  │      SDK 密码学库 (mbedTLS/SM)       │   │
│  └────────────────┬────────────────────┘   │
├───────────────────┼───────────────────────┤
│  yuleASR BSW 平台 (AutoSAR 基础软件层)     │
│  ┌─────────┐ ┌────────┐ ┌──────────────┐  │
│  │ HAL/MCAL│ │ OS/调度│ │ 通信栈(CAN/ │  │
│  │ 抽象层   │ │        │ │ LIN/Ethernet)│  │
│  └────┬────┘ └────┬───┘ └──────┬───────┘  │
│       │           │            │           │
│  ┌────┴───────────┴────────────┴───────┐   │
│  │            MCU (KW47)               │   │
│  └────────────────────────────────────┘   │
└───────────────────────────────────────────┘
```

## 组件层级

| 层级 | 项目 | 说明 |
|------|------|------|
| **应用层** | yuleDKCS | 数字钥匙连接系统，含多协议支持 (CCC/ICCE/ICCOA) |
| **SDK层** | yuleDKCS/sdk | 密码学库 (mbedTLS + SM2/SM3/SM4 国密) |
| **平台层** | yuleASR | AutoSAR BSW 平台，提供基础软件服务 |
| **硬件层** | KW47 MCU | NXP KW47 硬件平台 |

## 依赖方向

- **编译依赖**: yuleDKCS 编译时需要 yuleASR 的头文件和库
- **运行时依赖**: yuleDKCS 运行在 yuleASR 平台之上，调用 BSW API
- **SDK 关系**: yuleDKCS 的 `embedded/sdk/` 密码学库与 yuleASR 的 `third_party/` 互不冲突。yuleDKCS 使用独立的 mbedTLS 定制版，yuleASR third_party 包含其自身依赖。

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `YULEASR_ROOT` | yuleASR 项目根目录 | `/opt/yuleASR` |
| `YULEASR_BUILD` | yuleASR 构建输出目录 | `${YULEASR_ROOT}/build` |

## 集成方式

CMake 集成通过 `find_package(yuleASR)` 或 `add_subdirectory` 完成。
详情见 `embedded/CMakeLists.txt` 中的 yuleASR 相关配置。

## 初始化流程

```bash
# 1. 克隆并初始化 yuleASR
./scripts/setup.sh

# 2. 设置环境变量
export YULEASR_ROOT=/opt/yuleASR

# 3. 构建项目
./scripts/build.sh
```
