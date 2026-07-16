# NXP RTD Driver Integration Guide — yuleDKCS S32K312

## 概述

本文档指导如何将 **NXP Real-Time Drivers (RTD)** 集成到 yuleDKCS 项目中，
替换当前的寄存器级 mcal_stubs 实现。

| 项目 | 内容 |
|:-----|:------|
| **目标 MCU** | NXP S32K312 (Cortex-M7, 2MB Flash / 512KB SRAM) |
| **RTD 版本** | SW32K3_RTD_4.4_2.0.0 (或更新) |
| **工具链** | arm-none-eabi-gcc 16.x |
| **适配层** | `bsw_integration/mcal/rtd_adapter.h` + `rtd_adapter.c` |

---

## 1. 下载指引

### 1.1 从哪里下载

NXP RTD (S32K3 系列) 需要通过 **NXP 官网** 或 **NXP Software Portal** 获取：

| 方式 | 链接 | 说明 |
|:-----|:------|:------|
| **NXP 官网** | [NXP S32K3 软件页面](https://www.nxp.com/products/processors-and-microcontrollers/arm-processors/s32-automotive-platform/s32k-general-purpose-mcus/s32k3-arm-cortex-m7-based-mcus:S32K3) | 需要 NXP 账号注册 |
| **NXP Software Portal** | [software.nxp.com](https://software.nxp.com) | 搜索 "S32K3 RTD" |
| **NXP Flexera** (旧版) | [nxp.flexnetoperations.com](https://nxp.flexnetoperations.com) | 需要许可证 |

### 1.2 推荐版本

| 组件 | 版本 | 说明 |
|:-----|:------|:------|
| **SW32K3_RTD** | 4.4.2.0.0 或更高 | MCAL 驱动包 |
| **S32 Configuration Tools** | 最新 | EB tresos 或 S32CT |
| **S32 Design Studio** | 2024.x 或更高 | IDE（可选，用于生成 RTD 配置） |

### 1.3 下载后目录结构

```
SW32K3_RTD_4.4_2.0.0/
├── RTD/
│   ├── MCAL/
│   │   ├── Mcu/          # MCU 驱动 (时钟/PLL/复位)
│   │   ├── Dio/          # Digital I/O
│   │   ├── Port/         # Port 引脚配置
│   │   ├── Adc/          # ADC 转换器
│   │   ├── Pwm/          # PWM/FTM 输出
│   │   ├── Gpt/          # 通用定时器 (PIT/LPIT)
│   │   └── Wdg/          # 看门狗 (WDOG)
│   ├── include/          # 公共头文件
│   └── src/              # 公共源码
├── S32K3xx/              # S32K3 系列特定
└── docs/                 # 文档和手册
```

---

## 2. 架构概览

```
┌─────────────────────────────────────────────────────┐
│                  BSW / Application                    │
├─────────────────────────────────────────────────────┤
│               rtd_adapter.h / rtd_adapter.c           │  ← THIS LAYER
├─────────────────────────────────────┬───────────────┤
│  rtd_adapter.c (RTD_ENABLED=1)     │  Stub Mode    │
│  ┌──────────┐ ┌──────────┐         │  (RTD_ENABLED │
│  │ Mcu_RTD  │ │ Port_RTD │  ...    │   = 0)        │
│  └────┬─────┘ └────┬─────┘         │               │
│       │            │               │  ┌──────────┐ │
│  ┌────▼────────────▼─────────┐     │  │ mcal_stu-│ │
│  │  NXP RTD MCAL Libraries   │     │  │ bs.c     │ │
│  └────────────┬──────────────┘     │  └──────────┘ │
│               │                    │               │
│         ┌─────▼──────────────┐     │               │
│         │ S32K312 寄存器层   │     │               │
│         └────────────────────┘     │               │
└────────────────────────────────────┴───────────────┘
```

---

## 3. 文件替换清单

### 3.1 桩文件 vs RTD 源文件映射

| 桩文件 (当前) | RTD 源文件 | 包含路径 |
|:--------------|:-----------|:---------|
| `mcal_stubs/src/mcal_stubs.c` | `RTD/MCAL/Mcu/src/Mcu.c` | `RTD/MCAL/Mcu/include/` |
| ↑ (同上文件) | `RTD/MCAL/Port/src/Port.c` | `RTD/MCAL/Port/include/` |
| ↑ | `RTD/MCAL/Dio/src/Dio.c` | `RTD/MCAL/Dio/include/` |
| ↑ | `RTD/MCAL/Adc/src/Adc.c` | `RTD/MCAL/Adc/include/` |
| ↑ | `RTD/MCAL/Pwm/src/Pwm.c` | `RTD/MCAL/Pwm/include/` |
| ↑ | `RTD/MCAL/Gpt/src/Gpt.c` | `RTD/MCAL/Gpt/include/` |
| ↑ | `RTD/MCAL/Wdg/src/Wdg.c` | `RTD/MCAL/Wdg/include/` |
| `mcal_stubs/src/memif_impl.c` | 保持为 yuleDKCS 自有实现 | — |
| `mcal_stubs/include/Mcu.h` | 替换为 `RTD/MCAL/Mcu/include/Mcu.h` | |
| `mcal_stubs/include/Port.h` | 替换为 `RTD/MCAL/Port/include/Port.h` | |
| `mcal_stubs/include/Dio.h` | 替换为 `RTD/MCAL/Dio/include/Dio.h` | |
| `mcal_stubs/include/Adc.h` | 替换为 `RTD/MCAL/Adc/include/Adc.h` | |
| `mcal_stubs/include/Pwm.h` | 替换为 `RTD/MCAL/Pwm/include/Pwm.h` | |
| `mcal_stubs/include/Gpt.h` | 替换为 `RTD/MCAL/Gpt/include/Gpt.h` | |
| `mcal_stubs/include/Wdg.h` | 替换为 `RTD/MCAL/Wdg/include/Wdg.h` | |
| `mcal_stubs/include/Mcal.h` | 保留（非 AUTOSAR 标准，自有内存操作）| |
| `mcal_stubs/include/MemIf.h` | 保留 | |

### 3.2 配置文件

RTD 驱动需要一组 `.cfg` / `_Cfg.h` 配置文件，通常由 EB tresos 生成：

| 配置文件 | 生成方式 | 说明 |
|:---------|:---------|:------|
| `Mcu_Cfg.h` | EB tresos → MCAL Mcu | 时钟树、PLL 配置 |
| `Port_Cfg.h` | EB tresos → MCAL Port | 引脚复用表 |
| `Dio_Cfg.h` | EB tresos → MCAL Dio | 通道/端口定义 |
| `Adc_Cfg.h` | EB tresos → MCAL Adc | ADC 组/通道配置 |
| `Pwm_Cfg.h` | EB tresos → MCAL Pwm | PWM 通道参数 |
| `Gpt_Cfg.h` | EB tresos → MCAL Gpt | 定时器通道参数 |
| `Wdg_Cfg.h` | EB tresos → MCAL Wdg | 看门狗超时参数 |

---

## 4. CMakeLists.txt 变更

### 4.1 需要添加的 CMake 变量

```cmake
# ============================================================
# RTD 路径 — RTD_ENABLED=1 时必须设置
# ============================================================
set(RTD_ROOT /path/to/SW32K3_RTD_4.4_2.0.0/RTD CACHE PATH "NXP RTD root")

# MCAL 源文件
set(RTD_MCAL_SRCS
    ${RTD_ROOT}/MCAL/Mcu/src/Mcu.c
    ${RTD_ROOT}/MCAL/Port/src/Port.c
    ${RTD_ROOT}/MCAL/Dio/src/Dio.c
    ${RTD_ROOT}/MCAL/Adc/src/Adc.c
    ${RTD_ROOT}/MCAL/Pwm/src/Pwm.c
    ${RTD_ROOT}/MCAL/Gpt/src/Gpt.c
    ${RTD_ROOT}/MCAL/Wdg/src/Wdg.c
)

# MCAL 包含路径
set(RTD_MCAL_INCLUDES
    ${RTD_ROOT}/MCAL/Mcu/include
    ${RTD_ROOT}/MCAL/Port/include
    ${RTD_ROOT}/MCAL/Dio/include
    ${RTD_ROOT}/MCAL/Adc/include
    ${RTD_ROOT}/MCAL/Pwm/include
    ${RTD_ROOT}/MCAL/Gpt/include
    ${RTD_ROOT}/MCAL/Wdg/include
    ${RTD_ROOT}/include
    # EB tresos 生成的配置文件路径
    ${YULEDKCS_ROOT}/embedded/rtd_cfg
)
```

### 4.2 编译定义

```cmake
# 启用 RTD
add_compile_definitions(RTD_ENABLED=1)

# S32K3 平台宏
add_compile_definitions(
    CPU_S32K312
    S32K3XX
    GHS
)
```

### 4.3 修改 mcal_stubs 库

将 `mcal_stubs` 替换为 RTD MCAL 库：

```cmake
# 原: register-level 桩
# add_library(mcal_stubs STATIC
#     ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/mcal_stubs.c
#     ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/memif_impl.c
# )

# 新: RTD 驱动库
add_library(mcal_stubs STATIC
    ${RTD_MCAL_SRCS}
    ${YULEDKCS_ROOT}/embedded/mcal_stubs/src/memif_impl.c   # 保留自有实现
)

target_include_directories(mcal_stubs
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${RTD_MCAL_INCLUDES}
        ${YULEDKCS_ROOT}/embedded/rtd_cfg                    # 配置文件
)
```

### 4.4 添加 RTD 适配层

```cmake
# RTD Adapter 库（始终编译，通过宏开关切换）
add_library(rtd_adapter STATIC
    ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal/rtd_adapter.c
)

target_include_directories(rtd_adapter
    PUBLIC
        ${BSW_INCLUDE_DIRS}
        ${YULEDKCS_ROOT}/embedded/bsw_integration/mcal
        ${YULEDKCS_ROOT}/embedded/mcal_stubs/include
        ${RTD_MCAL_INCLUDES}           # RTD 模式下使用
)

# 链接到最终目标
target_link_libraries(yuleDKCS_bsw.elf
    ...
    rtd_adapter
    mcal_stubs
    ...
)
```

---

## 5. 验证步骤

### 5.1 桩模式验证（当前）

确保现有构建保持正常工作：

```bash
cd ~/yuleDKCS/embedded
mkdir -p build-verify && cd build-verify
cmake ../bsw_integration \
    -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake
make -j$(nproc) yuleDKCS_bsw.elf

# 运行 RTD 适配器自测
make test_rtd_adapter
./test_rtd_adapter
```

### 5.2 RTD 模式验证（需要 RTD 包）

```bash
cd ~/yuleDKCS/embedded
mkdir -p build-rtd && cd build-rtd
cmake ../bsw_integration \
    -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake \
    -DRTD_ROOT=/path/to/SW32K3_RTD_4.4_2.0.0/RTD \
    -DRTD_ENABLED=ON
make -j$(nproc) yuleDKCS_bsw.elf

# 运行自测
make test_rtd_adapter && ./test_rtd_adapter
```

### 5.3 自测检查清单

| 检查项 | 预期 | 命令 |
|:-------|:-----|:------|
| 编译通过 | 无错误/警告 | `make yuleDKCS_bsw.elf` |
| 映像大小合理 | <.text > 4KB | `arm-none-eabi-size yuleDKCS_bsw.elf` |
| 适配器自测 | 全部 PASS | `./test_rtd_adapter` |
| 符号检查 | 无未定义引用 | `arm-none-eabi-nm yuleDKCS_bsw.elf` |
| 反汇编审查 | 适配器函数正确链接 | 查看 `yuleDKCS_bsw.dis` |

### 5.4 确认 RTD 正常工作

1. **编译标志验证**：查看编译日志确认 `RTD_ENABLED=1` 生效
2. **函数调用跟踪**：启用 `RTD_TRACE_ENABLED=1`，查看初始化日志
3. **功能验证**：在目标硬件上执行基本的 GPIO 翻转 + ADC 采样 + PWM 输出

---

## 6. 代码迁移清单

### 6.1 需要修改的文件

```diff
- embedded/mcal_stubs/src/mcal_stubs.c    # 替换为 RTD 源文件
- embedded/mcal_stubs/include/Mcu.h       # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Port.h      # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Dio.h       # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Adc.h       # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Pwm.h       # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Gpt.h       # 替换为 RTD 头文件
- embedded/mcal_stubs/include/Wdg.h       # 替换为 RTD 头文件

+ embedded/rtd_cfg/                       # EB tresos 生成的配置 (新增)
+ embedded/bsw_integration/mcal/rtd_adapter.h   # 适配层 (已创建)
+ embedded/bsw_integration/mcal/rtd_adapter.c   # 适配层 (已创建)
```

### 6.2 使用时注意事项

1. **初始化顺序**：MCU → PORT → DIO → ADC → PWM → GPT → WDG（`Rtd_InitAll` 已封装此顺序）
2. **看门狗最后**：WDG 一旦使能必须定时喂狗，否则 MCU 复位
3. **时钟配置**：通过 EB tresos 生成的 `Mcu_Cfg.h` 配置时钟树，S32K312 最大频率 160MHz
4. **引脚冲突**：`Port_Cfg.h` 中定义的引脚必须与硬件设计一致
5. **中断管理**：RTD 驱动依赖中断向量表，需确保 `startup_s32k312.c` 中的 VTor 设置正确

---

## 7. 问题排查

### 7.1 常见问题

| 症状 | 可能原因 | 解决方案 |
|:-----|:---------|:---------|
| 链接错误: undefined reference to `Mcu_Init` | RTD 库未链接 | 检查 `RTD_MCAL_SRCS` 路径 |
| 编译错误: `Mcu_Cfg.h` not found | 配置文件未生成 | 运行 EB tresos 生成配置 |
| 自测失败: 返回值异常 | 桩模式预期值差 | 检查 `rtd_adapter.c` 的桩返回值 |
| 链接错误: `_sbrk` undefined | syscalls 未实现 | 确认 `syscalls.c` 包含在 `bsw_platform` |
| 映像过大 (>256KB) | RTD 调试模式或全编译 | 启用优化 `-Os`，关闭 `RTD_TRACE_ENABLED` |
| 适配器函数未被调用 | 代码直接使用 MCAL API | 搜索 `#include "Mcu.h"` 等，替换为 `rtd_adapter.h` |

### 7.2 诊断命令

```bash
# 检查编译定义
arm-none-eabi-gcc -dM -E - < /dev/null | grep -i RTD

# 检查符号表
arm-none-eabi-nm yuleDKCS_bsw.elf | grep -E "(Rtd_|Mcu_|Port_|Dio_)" | sort

# 检查内存占用
arm-none-eabi-size yuleDKCS_bsw.elf

# 段详情
arm-none-eabi-objdump -h yuleDKCS_bsw.elf
```

---

## 8. 维护信息

| 维护者 | yuleDKCS Embedded Team |
|:-------|:-----------------------|
| 适配层版本 | 1.0.0 |
| 最后更新 | 2026-07-08 |
| 适用 RTD 版本 | 4.4.x (S32K3) |
| 兼容模式 | 桩模式 (默认) / RTD 模式 (宏开关) |

---

## 附录: API 调用对照表

| 适配层 API | 桩模式调用 | RTD 模式调用 |
|:-----------|:-----------|:-------------|
| `Rtd_Mcu_Init` | `Mcu_Init()` | `Mcu_Init()` (RTD) |
| `Rtd_Mcu_DistributePllClock` | `Mcu_DistributePllClock()` | `Mcu_DistributePllClock()` (RTD) |
| `Rtd_Mcu_GetResetReason` | 模拟寄存器读 | `*(RCM_SRS)` 寄存器读 |
| `Rtd_Port_Init` | `Port_Init()` | `Port_Init()` (RTD) |
| `Rtd_Port_SetPinDirection` | 无操作 (PCR 管理) | `Port_SetPinDirection()` (RTD) |
| `Rtd_Port_SetPinMode` | `Port_SetPinMode()` | `Port_SetPinMode()` (RTD) |
| `Rtd_Dio_WriteChannel` | `Dio_WriteChannel()` | `Dio_WriteChannel()` (RTD) |
| `Rtd_Dio_ReadChannel` | `Dio_ReadChannel()` | `Dio_ReadChannel()` (RTD) |
| `Rtd_Dio_WritePort` | `Dio_WritePort()` | `Dio_WritePort()` (RTD) |
| `Rtd_Dio_ReadPort` | `Dio_ReadPort()` | `Dio_ReadPort()` (RTD) |
| `Rtd_Adc_Init` | `Adc_Init()` | `Adc_Init()` (RTD) |
| `Rtd_Adc_StartGroupConversion` | `Adc_StartGroupConversion()` | `Adc_StartGroupConversion()` (RTD) |
| `Rtd_Adc_ReadGroup` | `Adc_ReadGroup()` (返回仿真值) | `Adc_ReadGroup()` (RTD) |
| `Rtd_Adc_StopGroupConversion` | `Adc_StopGroupConversion()` | `Adc_StopGroupConversion()` (RTD) |
| `Rtd_Pwm_Init` | `Pwm_Init()` | `Pwm_Init()` (RTD) |
| `Rtd_Pwm_SetDutyCycle` | `Pwm_SetDutyCycle()` | `Pwm_SetDutyCycle()` (RTD) |
| `Rtd_Pwm_SetPeriodAndDuty` | `Pwm_SetPeriodAndDuty()` | `Pwm_SetPeriodAndDuty()` (RTD) |
| `Rtd_Gpt_StartTimer` | `Gpt_StartTimer()` | `Gpt_StartTimer()` (RTD) |
| `Rtd_Gpt_StopTimer` | `Gpt_StopTimer()` | `Gpt_StopTimer()` (RTD) |
| `Rtd_Gpt_GetTimeElapsed` | `Gpt_GetTimeElapsed()` (返回 0) | `Gpt_GetTimeElapsed()` (RTD) |
| `Rtd_Gpt_EnableNotification` | `Gpt_EnableNotification()` | `Gpt_EnableNotification()` (RTD) |
| `Rtd_Wdg_Init` | `Wdg_Init()` | `Wdg_Init()` (RTD) |
| `Rtd_Wdg_GetVersionInfo` | `Wdg_GetVersionInfo()` | `Wdg_GetVersionInfo()` (RTD) |
| `Rtd_Wdg_SetMode` | `Wdg_SetMode()` | `Wdg_SetMode()` (RTD) |
| `Rtd_Wdg_Trigger` | `Wdg_SetMode()` (模式刷新) | `Wdg_SetMode()` 或 `Wdg_Trigger` (RTD) |
