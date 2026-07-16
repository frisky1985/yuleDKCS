# yuleDKCS BSW Phase 1 — 综合集成报告

> 日期: 2026-07-08
> 集成模块: OS + EcuM + WdgM
> 目标 MCU: NXP S32K312 (ARM Cortex-M7)
> 基础栈: yuleASR AUTOSAR BSW
> 应用层: yuleDKCS ICCE Digital Key Protocol Stack

---

## 1. 基建确认

### 1.1 源码位置

| 项目 | 路径 | 状态 |
|:-----|:-----|:-----:|
| yuleASR | `~/.openclaw/workspace/yuleASR/` | ✅ 存在 |
| yuleDKCS | `~/yuleDKCS/` | ✅ 存在 |

### 1.2 yuleASR BSW 模块目录结构

| 模块 | 位置 | 状态 |
|:-----|:-----|:-----:|
| **OS** | `src/bsw/os/` (3 .c + 6 .h) | ✅ FreeRTOS 封装的 OSEK AUTOSAR OS |
| **EcuM** | `src/bsw/services/ecum/` (1 .c + 2 .h) | ✅ 完整多阶段启动/关闭状态机 |
| **WdgM** | `src/bsw/services/wdgm/` (2 .c + 3 .h) | ✅ Alive/Deadline/Logical 三型监督 |

### 1.3 yuleDKCS 嵌入式项目结构

| 组件 | 路径 | 说明 |
|:-----|:-----|:-----|
| ICCE Protocol | `embedded/icce_protocol/` | 主要应用 (BLE+UWB+安全+车辆接口) |
| CCC Protocol | `embedded/ccc_protocol/` | CCC 兼容层 |
| ICCoA Protocol | `embedded/iccoa_protocol/` | ICCoA 兼容层 |
| System Arch | `embedded/system_architecture/` | 系统设计文档 + 接口规范 |
| Tests | `embedded/tests/` | 单元测试 |

### 1.4 S32K312 平台支持

yuleASR 已包含 S32K312 平台:
- `src/platform/s32k312/include/` — ECC/Lockstep/RAM Safety 头文件
- `src/platform/s32k312/src/` — ECC/Lockstep/RAM Safety 实现
- `src/platform/s32k312/linker/` — Mpu_Cfg.ld, s32k312.ld 链接器脚本

---

## 2. OS 集成

### 2.1 架构

```
传统 while(1) 方式 → AUTOSAR OSEK OS 方式
───────────────────────────────────────────
┌─ main() 手写循环 ─┐    ┌─ StartOS() → FreeRTOS ─┐
│ while(1) {         │    │ ┌─ Task Init           │
│   init();          │    │ ├─ Task 10ms (UWB/BLE) │
│   uwb_ranging();   │    │ ├─ Task 50ms (Edge)    │
│   edge_eval();     │    │ ├─ Task 100ms (CAN)    │
│   can_tx();        │    │ └─ Task Background     │
│   delay(10ms);     │    │                        │
│ }                  │    │ Alarm → ActivateTask   │
└────────────────────┘    └────────────────────────┘
```

### 2.2 任务定义

| 任务 | 周期 | 优先级 | 功能 |
|:-----|:----:|:------:|:-----|
| OsDkTask_Init | 一次性 | 最高 | ICCE 协议栈初始化 |
| OsDkTask_10ms | 10ms | 高 | UWB 测距 + BLE 轮询 + WdgM MainFunction + EcuM MainFunction |
| OsDkTask_50ms | 50ms | 中 | 边缘规则评估 + 区域变化检测 |
| OsDkTask_100ms | 100ms | 低 | 车辆状态同步 + CAN + 钥匙过期检查 |
| OsDkTask_Background | 空闲 | 最低 | 后台维护、日志 |

### 2.3 Alarm 映射

| Alarm | 周期 | 动作 |
|:------|:----:|:-----|
| OsDkAlarm_10ms_Task | 10ms | ActivateTask(OsDkTask_10ms) |
| OsDkAlarm_50ms_Task | 50ms | ActivateTask(OsDkTask_50ms) |
| OsDkAlarm_100ms_Task | 100ms | ActivateTask(OsDkTask_100ms) |
| OsDkAlarm_EcuM_MainFunction | 10ms | EcuM_MainFunction 周期触发 |

### 2.4 集成文件

| 文件 | 路径 | 说明 |
|:-----|:-----|:-----|
| `main.c` | `bsw_integration/src/main.c` | 主入口: EcuM_Init() → StartOS() |
| `dk_os_tasks.c` | `bsw_integration/src/dk_os_tasks.c` | 五任务入口实现 |
| `dk_os_cfg.c` | `bsw_integration/src/dk_os_cfg.c` | yuleDKCS 定制任务/警报/资源表 |
| `Os_Cfg_Dk.h` | `bsw_integration/include/Os_Cfg_Dk.h` | DK 专用 OS 配置参数 |

---

## 3. EcuM 集成

### 3.1 状态机集成

```
EcuM 状态机 ─── yuleDKCS 映射
──────────────────────────────
OFF ──→ STARTUP (Phase 1: MCU init)
             ↓
STARTUP_ONE ─→ Mcu_Init, 时钟, 基本外设
STARTUP_TWO ─→ BSW 模块初始化 (MCAL: DIO/CAN/GPT/SPI)
STARTUP_THREE → ICce_dk_init() (协议栈加载)
       ↓
RUN ───→ OsTask 10ms/50ms/100ms 周期运行
  │        WdgM 监督实体活跃
  │        EcuM MainFunction 维持状态
  │
POST_RUN ─→ 释放 RUN 请求后进入
  │
SHUTDOWN ─→ GoOffOne: NvM 写
  │           GoOffTwo: ShutdownOS
  ↓
SLEEP ───→ BLE 低功耗扫描 / UWB 被动监听
```

### 3.2 唤醒源配置

| 唤醒源 | 类型 | 验证超时 | 功能 |
|:------|:----:|:--------:|:-----|
| POWER | 硬件 | 100ms | 上电启动 |
| RESET | 硬件 | 100ms | 复位唤醒 |
| TIMER | 定时器 | 100ms | 周期性唤醒 |
| CAN | 网络 | 100ms | CAN 总线唤醒 |
| GPIO | 硬件 | 100ms | 按键/门控 |
| BLE (yuleDKCS) | 无线 | 200ms | BLE 低功耗广播 |
| UWB (yuleDKCS) | 无线 | 200ms | UWB 被动监听 |

### 3.3 集成文件

| 文件 | 路径 | 说明 |
|:-----|:-----|:-----|
| `dk_ecum_callouts.c` | `bsw_integration/src/dk_ecum_callouts.c` | EcuM callout 实现 (启动/关闭/休眠/唤醒) |
| `main.c` | `bsw_integration/src/main.c` | EcuM_Config 全局实例 |

---

## 4. WdgM 集成

### 4.1 监督实体

| SEID | 名称 | 监督类型 | 检查点 | 监督周期 |
|:----|:-----|:--------:|:------:|:--------:|
| 0 | Main Cycle | Alive | 5 (每个任务一个) | 10ms |
| 1 | BLE/UWB | Alive | 2 (RX + Ranging) | 10ms |
| 2 | Vehicle CAN | Alive | 1 | 10ms |
| 3 | Storage (NvM) | Alive | 1 | 10ms |
| 4 | Safety Monitor | Alive | 1 | 10ms |

### 4.2 定时配置

| 参数 | 值 | 说明 |
|:-----|:--:|:-----|
| 监督周期 | 10ms | 与 WdgM_MainFunction 同步 |
| 过期容忍 | 3 周期 | 30ms 无报告即超时 |
| 存活阈值 | 5 | 存活计数器上限 |
| IWD 超时 | 100ms | 独立看门狗 |
| WWD 周期 | 50ms | 窗口看门狗 |
| 错误阈值 | 3 | 连续失败触发安全响应 |

### 4.3 集成文件

| 文件 | 路径 | 说明 |
|:-----|:-----|:-----|
| `dk_wdgm_cfg.c` | `bsw_integration/src/dk_wdgm_cfg.c` | 监督实体/检查点配置 + 回调 |
| `WdgM_Cfg_Dk.h` | `bsw_integration/include/WdgM_Cfg_Dk.h` | DK 专用看门狗配置参数 |

---

## 5. 编译验证

### 5.1 构建系统

```
bsw_integration/CMakeLists.txt —— yuleDKCS BSW Phase 1 构建入口
```

### 5.2 构建目标

| 目标 | 说明 |
|:-----|:-----|
| `yuleDKCS_bsw.elf` | S32K312 ELF 固件映像 |
| `yuleDKCS_bsw.bin` | 二进制烧录文件 |
| `yuleDKCS_bsw.hex` | Intel HEX 烧录文件 |
| `yuleDKCS_bsw.dis` | 反汇编清单 |
| `yuleDKCS_bsw.map` | 链接地址映射 |

### 5.3 编译命令

```bash
# 1. 配置
mkdir -p ~/yuleDKCS/embedded/bsw_integration/build
cd ~/yuleDKCS/embedded/bsw_integration/build

# 2. 配置 (S32K312 交叉编译)
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=.../arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Release

# 3. 构建
make bsw_phase1_all

# 4. 大小检查
make bsw_phase1_size

# 5. 清理
make bsw_phase1_clean
```

### 5.4 依赖链

```
main.c
  ├── Os.h, Os_Cfg.h, EcuM.h, EcuM_Cfg.h, Wdgm.h, WdgM_Cfg.h
  ├── icce_digital_key.h
  └── dk_ecum_callouts.c → EcuM.c
      └── icce_digital_key.h

dk_os_tasks.c
  ├── Os.h, Os_Cfg.h
  └── icce_digital_key.h

dk_os_cfg.c
  └── Os.h, Os_Internal.h, Os_Cfg.h, Os_Cfg_Dk.h

dk_wdgm_cfg.c
  └── Wdgm.h, WdgM_Cfg.h, WdgM_Cfg_Dk.h, WdgM_MemMap.h

基础设施桩:
  ├── MemMap.h         (BSP — S32K312 SDK / 本 repo 空桩)
  ├── Compiler.h       (BSP — S32K312 SDK / 本 repo 空桩)
  ├── Det.h            (本 repo 空桩)
  ├── WdgM_MemMap.h    (本 repo 空桩)
  └── WdgIf.h          (yuleASR src/bsw/ecual/wdgif/include/)
```

---

## 6. 功能等价性验证

### 6.1 既有功能保持

| yuleDKCS 功能 | 迁移前 | 迁移后 |
|:--------------|:-------|:-------|
| ICCE 协议栈初始化 | `main()` 内调用 | `OsDkTask_Init` → `EcuM_DriverInitThree` |
| UWB 测距 | 回调式 | `OsDkTask_10ms` 周期轮询 |
| BLE 数据处理 | 回调式 | `OsDkTask_10ms` 周期轮询 |
| 边缘规则评估 | `on_uwb_ranging()` 内 | `OsDkTask_50ms` 周期评估 |
| CAN 通信 | 循环中调用 | `OsDkTask_100ms` 周期收发 |
| 钥匙过期检查 | 循环中调用 | `OsDkTask_100ms` 周期检查 |
| 电源管理 | 无 | `EcuM` 状态机管理 |

### 6.2 新增功能

| 功能 | 提供者 | 说明 |
|:-----|:-------|:-----|
| 确定性调度 | AUTOSAR OS | 固定周期任务取代 while(1) + delay |
| 看门狗监督 | WdgM | 5 个监督实体系统级存活保证 |
| 多阶段启动 | EcuM | 可管理的启动/关闭/休眠序列 |
| 唤醒源管理 | EcuM | BLE/UWB/CAN/GPIO 唤醒源验证 |

---

## 7. 阻塞项与风险

### 7.1 当前阻塞项

| # | 项目 | 严重程度 | 说明 |
|:-:|:-----|:--------:|:-----|
| B1 | **arm-none-eabi 工具链** | 中 | 当前环境可能未安装。编译前需确认: `arm-none-eabi-gcc --version` |
| B2 | **FreeRTOS 移植** | 中 | yuleASR OS 需要 S32K312 的 FreeRTOS 移植。若无可用的 FreeRTOS port，需要: 1) 适配 Cortex-M7 SysTick 中断 2) 实现 port.c |
| B3 | **MCAL 驱动** | 低 | 当前代码使用 MCAL 函数调用的注释桩。实际产品需集成 NXP S32 SDK MCAL 二进制库 |
| B4 | **MemMap.h 等基础设施** | 低 | 由本 repo 提供空桩，正式产品需替换为 NXP S32 SDK 提供的版本 |
| B5 | **BswM 依赖** | 中 | EcuM 依赖 BswM 进行模式管理，但 BswM 不在 Phase 1 范围内。当前代码使用无操作桩 |

### 7.2 下一步建议

| 优先级 | 任务 | 说明 |
|:------:|:-----|:-----|
| P0 | 确认 arm-none-eabi 工具链 | `which arm-none-eabi-gcc` |
| P0 | FreeRTOS S32K312 port | 查找或移植 FreeRTOS V11.x 到 S32K312 |
| P1 | CMake toolchain file | 编写 `arm-none-eabi-gcc.cmake` toolchain 文件 |
| P1 | NXP S32 SDK 集成 | 集成 MCAL 二进制/源码 + MemMap.h |
| P2 | BswM 集成 (Phase 2) | BSW Mode Manager 模式仲裁 |

---

## 8. 文件清单

### 8.1 新增文件 (bsw_integration/)

| 文件 | 大小 | 功能 |
|:-----|:---:|:-----|
| `CMakeLists.txt` | 8.3 KB | 构建系统定义 |
| `include/MemMap.h` | 2.0 KB | AUTOSAR 内存映射桩 |
| `include/WdgM_MemMap.h` | 1.1 KB | WdgM 内存映射桩 |
| `include/Compiler.h` | 0.7 KB | AUTOSAR 编译器抽象桩 |
| `include/Det.h` | 0.9 KB | 开发错误追踪桩 |
| `include/Os_Cfg_Dk.h` | 3.4 KB | DK 专用 OS 配置参数 |
| `include/WdgM_Cfg_Dk.h` | 3.0 KB | DK 专用看门狗配置参数 |
| `src/main.c` | 5.8 KB | 主入口 + EcuM 配置 |
| `src/dk_os_tasks.c` | 3.9 KB | OS 任务入口 |
| `src/dk_os_cfg.c` | 7.7 KB | OS 配置表 (任务/警报/资源) |
| `src/dk_ecum_callouts.c` | 4.5 KB | EcuM callout 实现 |
| `src/dk_wdgm_cfg.c` | 4.6 KB | WdgM 配置实现 |

### 8.2 依赖的 yuleASR 模块 (不变)

| 模块 | 路径 | 版本 |
|:-----|:-----|:----|
| OS Core | `src/bsw/os/src/Os.c` | 1.0.0 |
| OS Timing Protection | `src/bsw/os/src/Os_TimingProtection.c` | 1.0.0 |
| OS Headers | `src/bsw/os/include/` | 6 files |
| EcuM Core | `src/bsw/services/ecum/src/EcuM.c` | 2.0.0 |
| EcuM Headers | `src/bsw/services/ecum/include/` | 2 files |
| WdgM Core | `src/bsw/services/wdgm/src/Wdgm.c` | 1.0.0 |
| WdgM Headers | `src/bsw/services/wdgm/include/` | 3 files |
| WdgIf | `src/bsw/ecual/wdgif/include/WdgIf.h` | 1.0.0 |

---

## 9. 总结

BSW Phase 1 (OS + EcuM + WdgM) 集成完成度:

| 项目 | 状态 |
|:-----|:----:|
| 基建确认 | ✅ 完成 |
| OS 集成 | ✅ 完成 (5任务 + 4Alarm + 3资源) |
| EcuM 集成 | ✅ 完成 (完整 callout + 7唤醒源) |
| WdgM 集成 | ✅ 完成 (5监督实体 + 10检查点) |
| 构建系统 | ✅ 完成 (CMake + S32K312) |
| 功能等价性 | ✅ 所有既有功能已映射到 OS 任务 |
| 编译验证 | ⚠️ 待执行 (需 arm-none-eabi + FreeRTOS port) |

**阻塞项**: B1 (工具链), B2 (FreeRTOS port for S32K312)
**总体进展**: Phase 1 集成代码完成, 待工具链环境就绪后可编译验证。
