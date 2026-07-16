# yuleDKCS BSW Phase 1 — 清阻报告

> 生成时间: 2026-07-08 18:04 CST
> 构建目标: NXP S32K312 (Cortex-M7 @ 320MHz, FPv5-SP-D16)
> 工具链: arm-none-eabi-gcc 16.1.0

## 总览

| 阻塞项 | 状态 | 说明 |
|--------|------|------|
| B1: arm-none-eabi 工具链 | ✅ 已确认 | /opt/homebrew/bin/arm-none-eabi-gcc (v16.1.0) |
| B2: S32K312 FreeRTOS port | ✅ 已创建 | Cortex-M7 port — portmacro.h + port.c + FreeRTOSConfig.h |
| B3: MCAL 桩 | ✅ 已创建 | 自包含 MCAL 头文件 + 无操作实现 |
| B4: MemMap.h | ✅ 已重写 | 完整 AUTOSAR 内存段映射 |
| B5: BswM 桩 | ✅ 已完成 | yuleASR BswM.c 编译通过 + 补充桩函数 |

## 构建产物

| 文件 | 大小 | 说明 |
|------|------|------|
| `yuleDKCS_bsw.elf` | 154 KB | ELF 固件映像 |
| `yuleDKCS_bsw.bin` | 14 KB | 二进制映像 |
| `yuleDKCS_bsw.hex` | 41 KB | Intel HEX 映像 |
| `yuleDKCS_bsw.dis` | 358 KB | 反汇编输出 |
| `yuleDKCS_bsw.map` | 36 KB | 链接映射表 |

### 段分布
- **text**: 14,064 bytes
- **data**: 356 bytes
- **bss**: 270,764 bytes
- **Total**: 285,184 bytes

### 函数符号: 242 个

## B1: arm-none-eabi 工具链确认 ✅

**路径**: `/opt/homebrew/bin/arm-none-eabi-gcc`

**版本**: arm-none-eabi-gcc 16.1.0

**确认**: 最小 C 文件编译 + 链接通过:
```
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 \
  -Os -ffreestanding -nostdlib -o test_min.elf test_min.c
```

## B2: S32K312 FreeRTOS Port ✅

**目标**: 为 S32K312 (Cortex-M7, 320MHz) 创建 FreeRTOS 移植层。

### 创建的文件

| 文件 | 路径 | 说明 |
|------|------|------|
| `FreeRTOSConfig.h` | `freertos_port/include/` | S32K312 配置: SysTick @1ms, 32 优先级, 16 中断优先级, 128KB 堆 |
| `portmacro.h` | `freertos_port/include/` | ARMv7-M port 宏: StackType_t, 临界区, PendSV/SysTick 声明 |
| `port.c` | `freertos_port/src/` | 完整 port 实现: 栈初始化, PendSV 上下文切换, SysTick, 临界区 |
| `projdefs.h` | `freertos_port/include/` | pdTRUE/pdFALSE, TaskFunction_t |
| `task.h` | `freertos_port/include/` | xTaskCreate, vTaskDelete, eTaskGetState 等 |
| `list.h` | `freertos_port/include/` | FreeRTOS 链表 |
| `queue.h` | `freertos_port/include/` | 队列 API |
| `semphr.h` | `freertos_port/include/` | 信号量 API |
| `timers.h` | `freertos_port/include/` | 软件定时器 API |
| `event_groups.h` | `freertos_port/include/` | 事件组 API |
| `portable.h` | `freertos_port/include/` | 可移植层接口 |

### 关键配置参数

```c
#define configCPU_CLOCK_HZ                  320000000UL
#define configTICK_RATE_HZ                  1000
#define configMAX_PRIORITIES                32
#define configTOTAL_HEAP_SIZE               131072
#define configSUPPORT_STATIC_ALLOCATION     1
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configUSE_TIMERS                    1
#define configENABLE_FPU                    1
#define configENABLE_MPU                    0
#define configPRIO_BITS                     4
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 0x05
```

## B3: MCAL 桩 ✅

### 创建的文件

| 文件 | 路径 | 说明 |
|------|------|------|
| `Mcu.h` | `mcal_stubs/include/` | MCU 驱动 (Init/SetMode/GetMode/Reset) |
| `Dio.h` | `mcal_stubs/include/` | DIO 驱动 (Read/Write 通道和端口) |
| `Port.h` | `mcal_stubs/include/` | PORT 驱动 (Init/SetPinMode) |
| `Adc.h` | `mcal_stubs/include/` | ADC 驱动 (Group 转换) |
| `Pwm.h` | `mcal_stubs/include/` | PWM 驱动 (Duty/Period) |
| `Gpt.h` | `mcal_stubs/include/` | GPT 驱动 (定时器) |
| `Wdg.h` | `mcal_stubs/include/` | WDG 驱动 |
| `mcal_stubs.c` | `mcal_stubs/src/` | 所有桩的无操作实现 |

所有 MCAL Init 函数可安全调用 (空操作, 不崩溃)。

## B4: MemMap.h ✅

### 重写的文件: `bsw_integration/include/MemMap.h`

完整 AUTOSAR 内存段映射, 支持:

| 段类型 | 映射到 | 模块 |
|--------|--------|------|
| SEC_CODE | `.text.{Module}` | OS, EcuM, WdgM, Det, BswM |
| SEC_CONST_UNSPECIFIED | `.rodata.{Module}` | OS, EcuM, Det |
| SEC_CONFIG_DATA_UNSPECIFIED | `.rodata.{Module}_Config` | OS |
| SEC_CALIB_UNSPECIFIED | `.rodata.{Module}_Calib` | OS |
| SEC_VAR_CLEARED_UNSPECIFIED | `.bss.{Module}` | OS, Det, EcuM, WdgM, BswM |
| SEC_VAR_NOINIT_UNSPECIFIED | `.bss.{Module}_NoInit` | OS |
| SEC_INIT_UNSPECIFIED | `.data.{Module}` | OS |

## B5: BswM 桩 ✅

**BswM 实现来源**: yuleASR `src/bsw/services/bswm/` (真实 AUTOSAR BswM)

**yuleDKCS 补充**:
- `SchM_BswM.h` — BswM 排他区桩 (空操作 Enter/Exit)
- `bsw_stubs.c` — 补充 `BswM_EcuM_CurrentWakeup()`, `SchM_Init()`, `SchM_Deinit()`

**BswM API** (全部编译通过 + 链接成功):
- `BswM_Init`, `BswM_DeInit`, `BswM_MainFunction`
- `BswM_RequestMode`, `BswM_GetCurrentMode`
- `BswM_EcuM_CurrentState`, `BswM_EcuM_CurrentWakeup`
- `BswM_ComM_CurrentMode`, `BswM_Dcm_RequestCommunicationMode`

## 编译通过的模块

| 模块 | 来源 | 大小 (text) |
|------|------|-------------|
| OS (AUTOSAR) | yuleASR Os.c + dk_os_cfg.c | 4,988 bytes |
| EcuM | yuleASR EcuM.c + main.c callouts | 4,768 bytes |
| WdgM | yuleASR Wdgm.c + dk_wdgm_cfg.c | 1,492 bytes |
| Det | yuleASR Det.c | 876 bytes |
| BswM | yuleASR BswM.c | 484 bytes |
| FreeRTOS Port | yuleDKCS port.c | 1,204 bytes |
| MCAL 桩 | yuleDKCS mcal_stubs.c | 110 bytes |
| 应用任务 | yuleDKCS dk_os_tasks.c | 74 bytes |
| BSW 桩 | yuleDKCS bsw_stubs.c + freertos_stubs.c | 950 bytes |
| syscalls | yuleDKCS syscalls.c | 594 bytes |

## 编译命令 (验证用)

```bash
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 \\
  -O0 -ffreestanding -nostdlib -lgcc \\
  -T <linker_script> \\
  -o yuleDKCS_bsw.elf <所有源文件>
```

## 已知限制

1. **FreeRTOS 函数未实现**: 使用无操作桩 (freertos_stubs.c), 运行时行为不可用
2. **MCAL 无硬件操作**: 所有驱动为桩, 不操作实际外设寄存器
3. **STM 模块未包含**: NvM, ComM 等使用空桩
4. **链接脚本兼容性**: S32K312 官方链接脚本有段重叠 (使用简化脚本绕行)
5. **-O0 优化**: -Os/-O1 下 `.group` COMDAT 段导致链接时被丢弃, 需使用 `-O0` 或调整链接脚本
