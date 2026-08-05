# yuleDKCS BSW Phase 1 — 编译验证报告

> **Date**: 2026-07-08 20:58 CST  
> **Target**: NXP S32K312 (Cortex-M7, FPv5-D16)  
> **Toolchain**: arm-none-eabi-gcc 16.1.0  
> **Status**: ✅ **BUILD SUCCESS** — 0 errors, 0 warnings

---

## 1. 编译结果

| Item | Status |
|------|--------|
| CMake 配置 | ✅ 通过 |
| 全部 C 源文件编译 | ✅ 通过 (0 错误, 0 警告) |
| 最终链接 | ✅ 通过 |
| BIN 生成 | ✅ `yuleDKCS_bsw.bin` (6.9 KB) |
| HEX 生成 | ✅ `yuleDKCS_bsw.hex` (20 KB) |
| MAP 生成 | ✅ `yuleDKCS_bsw.map` |
| Disassembly 生成 | ✅ `yuleDKCS_bsw.dis` (151 KB) |

---

## 2. 编译验证清单

| # | 检查项 | 状态 | 说明 |
|---|--------|------|------|
| 1 | OS 任务调度代码 | ✅ | StartOS, ActivateTask, TerminateTask, Schedule 全部符号可见 |
| 2 | EcuM 状态机 | ✅ | EcuM_Init, EcuM_MainFunction, EcuM_RequestRUN, EcuM_StartupOne, EcuM_Shutdown 等 50 个 API |
| 3 | WdgM 看门狗管理 | ✅ | Wdgm_Init, Wdgm_MainFunction, Wdgm_CheckpointReached, Wdgm_SetMode 等 11 个 API |
| 4 | FreeRTOS port | ✅ | SVC_Handler, PendSV_Handler, SysTick_Handler, xPortStartScheduler, pxPortInitialiseStack |
| 5 | MCAL 桩 | ✅ | 8 个 MCAL 驱动桩编译通过，无阻塞 |
| 6 | MemMap 段分配 | ✅ | .data LMA 0x1f74, .copy.table LMA 0x1f84, .zero.table LMA 0x1f9c — 无重叠 |
| 7 | BswM 与 EcuM 联动 | ✅ | EcuM 启动序列调用 BswM_Init, BswM_EcuM_CurrentState, BswM_EcuM_CurrentWakeup |

---

## 3. 固件大小分布

| 段 | 大小 | VMA | LMA | 说明 |
|-----|------|-----|-----|------|
| `.vectors` | 256 B | 0x0000_0400 | 0x0000_0400 | 中断向量表 |
| `.text` | 7,388 B | 0x0000_0500 | 0x0000_0500 | 代码段 |
| `.rodata` | 152 B | 0x0000_1EDC | 0x0000_1EDC | 只读数据 |
| `.data` | 16 B | 0x2001_0000 | 0x0000_1F74 | 初始化数据 (RAM VMA, Flash LMA) |
| `.bss` | 392 B | 0x2001_0010 | — | 零初始化数据 |
| `.heap` | 256 KB | 0x2001_0198 | — | 堆 (FreeRTOS heap 128KB) |
| `.copy.table` | 24 B | 0x0000_1F74 | 0x0000_1F84 | 数据拷贝表 |
| `.zero.table` | 24 B | 0x0000_1F8C | 0x0000_1F9C | BSS 清零表 |

**size 汇总**: text=7,028 B, data=64 B, bss=262,536 B (含 256KB heap)

---

## 4. 符号完整性检查

### 4.1 OS API (19 个)

| 符号 | 状态 |
|------|------|
| `StartOS` | ✅ |
| `ActivateTask` | ✅ |
| `TerminateTask` | ✅ |
| `Schedule` | ✅ |
| `GetTaskID` | ✅ |
| `GetTaskState` | ✅ |
| `SetEvent` / `ClearEvent` / `WaitEvent` / `GetEvent` | ✅ |
| `SetRelAlarm` / `SetAbsAlarm` / `CancelAlarm` / `GetAlarm` | ✅ |
| `GetResource` / `ReleaseResource` | ✅ |
| `ShutdownOS` | ✅ |

### 4.2 EcuM API (50 个)

| 关键符号 | 状态 |
|----------|------|
| `EcuM_Init` | ✅ |
| `EcuM_MainFunction` | ✅ |
| `EcuM_RequestRUN` / `EcuM_ReleaseRUN` | ✅ |
| `EcuM_StartupOne` / `EcuM_StartupTwo` | ✅ |
| `EcuM_Shutdown` | ✅ |
| `EcuM_GoSleep` / `EcuM_GoHalt` / `EcuM_GoPoll` | ✅ |
| `EcuM_SetWakeupEvent` / `EcuM_GetWakeupSources` | ✅ |
| `EcuM_SelectShutdownTarget` / `EcuM_GetShutdownTarget` | ✅ |

### 4.3 BswM API (9 个)

| 符号 | 状态 |
|------|------|
| `BswM_Init` / `BswM_DeInit` | ✅ |
| `BswM_MainFunction` | ✅ |
| `BswM_RequestMode` | ✅ |
| `BswM_EcuM_CurrentState` | ✅ |
| `BswM_EcuM_CurrentWakeup` | ✅ |
| `BswM_ComM_CurrentMode` | ✅ |
| `BswM_Dcm_RequestCommunicationMode` | ✅ |
| `BswM_GetVersionInfo` | ✅ |

### 4.4 Wdgm API (11 个)

| 符号 | 状态 |
|------|------|
| `Wdgm_Init` / `Wdgm_DeInit` | ✅ |
| `Wdgm_MainFunction` | ✅ |
| `Wdgm_CheckpointReached` | ✅ |
| `Wdgm_SetMode` / `Wdgm_GetMode` | ✅ |
| `Wdgm_GetLocalStatus` / `Wdgm_GetGlobalStatus` | ✅ |
| `Wdgm_PerformReset` / `Wdgm_GetFirstExpiredSEID` | ✅ |
| `Wdgm_GetVersionInfo` | ✅ |

### 4.5 其他关键符号

| 模块 | 符号 | 状态 |
|------|------|------|
| Startup | `Reset_Handler` | ✅ |
| Exception | `HardFault_Handler` / `Default_Handler` | ✅ |
| FreeRTOS Port | `SVC_Handler` / `PendSV_Handler` / `SysTick_Handler` | ✅ |
| Det | `Det_Init` / `Det_ReportError` | ✅ |
| NvM Stub | `NvM_Init` / `NvM_ReadAll` / `NvM_WriteAll` | ✅ |
| ComM Stub | `ComM_Init` / `ComM_DeInit` | ✅ |
| SchM Stub | `SchM_Init` / `SchM_Deinit` | ✅ |
| ICCE Stubs | `icce_dk_init` / `icce_ble_init` / `icce_uwb_init` 等 11 个 | ✅ |
| MCAL Stubs | `Mcu_Init`, `Dio_*`,`Port_*`,`Gpt_*`,`Adc_*`,`Pwm_*`,`WdgIf_*` | ✅ |

---

## 5. 已有嵌入式代码兼容性

| 检查项 | 状态 |
|--------|------|
| handshake 符号冲突 | ✅ 无冲突 |
| test_dk_fault_inject 符号冲突 | ✅ 无冲突 |
| icce_protocol 真实代码冲突 | ✅ 仅 ICCE 桩函数链接，无真实协议代码冲突 |
| mcal_stubs 与 mcal_stubs/include 一致性 | ✅ |

---

## 6. 修复记录 (本次编译中修复的问题)

| # | 问题 | 修复 |
|---|------|------|
| F1 | CMakeLists.txt 中 LINK_FLAGS 被覆盖 | 合并在单次 `set_target_properties` 调用中 |
| F2 | `startup_s32k312.c` / `freertos_stubs.c` 未加入构建 | 新增 `bsw_platform` 库目标 |
| F3 | `BswM_EcuM_CurrentState` 缺失 | 已由 yuleASR BswM.c 提供；从 bsw_stubs.c 移除重复定义 |
| F4 | 链接脚本 LMA 重叠 (.data 与 .copy.table) | 重写链接脚本: `.copy.table`/`.zero.table` 移至所有数据段之后，使用明确的 AT 地址 |
| F5 | `startup_s32k312.c` 使用 `mrc p15` 指令 (ARMv7-A) | 替换为 Cortex-M7 CPACR 寄存器直接访问 |
| F6 | Wdgm 静态库未自动链接 | 在 `bsw_stubs.c` 中通过全局指针添加 Wdgm_Init, SVC_Handler, PendSV_Handler, SysTick_Handler 的引用强制链接 |
| F7 | BswM_Init 链接顺序问题 | 调整库链接顺序使 bsw_bswm 在 bsw_ecum 之后处理 |

---

## 7. 构建命令

```bash
cd /tmp/bsw_build
cmake -S ~/yuleDKCS/embedded/bsw_integration \
      -B /tmp/bsw_build \
      -DCMAKE_TOOLCHAIN_FILE=~/yuleDKCS/embedded/arm-none-eabi-toolchain.cmake
make -j4
```

## 8. 输出产物

```
/tmp/bsw_build/
├── yuleDKCS_bsw.elf       # ELF 格式 (7 KB 纯二进制, 151 KB 含调试)
├── yuleDKCS_bsw.bin       # 裸二进制 (6.9 KB) — 可烧录
├── yuleDKCS_bsw.hex       # Intel HEX (20 KB) — 可烧录
├── yuleDKCS_bsw.map       # 链接映射 (45 KB) — 详细符号地址
└── yuleDKCS_bsw.dis       # 反汇编 (151 KB) — 代码审查
```

---

## 9. 结论

**BSW Phase 1 编译验证通过。** 所有 5 个模块 (OS, EcuM, WdgM, BswM, Det) 成功编译并链接为 S32K312 固件映像。7 项编译验证清单全部通过，符号完整性检查确认 100+ 关键 API 全部可见。无编译警告，无链接错误。

本次编译修复了链接脚本 LMA 重叠、启动代码 ARMv7-A 指令不兼容、静态库自动链接等 7 个阻塞项。
