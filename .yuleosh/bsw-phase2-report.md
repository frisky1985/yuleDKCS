# BSW Phase 2 集成报告 — COM + DCM + DEM

## 概述

yuleDKCS BSW Phase 2 在 Phase 1 (OS + EcuM + WdgM) 基础上完成 **COM（通信栈）**、**DCM（诊断通信管理）**、**DEM（诊断事件管理）** 的集成。

## 集成模块

| 模块 | 源码来源 | 配置 |
|:-----|:---------|:-----|
| **COM** | `yuleASR/src/bsw/services/com/` | CAN 信号 → I-PDU 映射 (ICCE DK) |
| **PduR** | `yuleASR/src/bsw/services/pdur/` | 模块间 PDU 路由路径（4 路由） |
| **DCM** | `yuleASR/src/bsw/services/dcm/` | UDS 协议栈 (11 DID + 4 RID) |
| **DEM** | `yuleASR/src/bsw/services/dem/` | 16 个 DTC 事件（ECU/CAN/BLE/UWB/Security/NFC） |

## 新增/修改文件

### 新文件 (8 个)

| 文件 | 路径 | 说明 |
|:-----|:-----|:------|
| `ComStack_Types.h` | `bsw_integration/include/` | AUTOSAR PDU 通信类型定义 (PduIdType/PduInfoType 等) |
| `Com_Cfg_Dk.h` | `bsw_integration/include/` | COM 配置覆盖: 8 信号, 4 IPDU, 2 组 |
| `Dcm_Cfg_Dk.h` | `bsw_integration/include/` | DCM 配置覆盖: yuleDKCS DID/RID 定义, DCM_SID_DEINIT 修复 |
| `Dem_Cfg_Dk.h` | `bsw_integration/include/` | DEM 配置覆盖: 16 个 ICCE DK DTC 事件定义 |
| `dk_com_cfg.c` | `bsw_integration/src/` | COM 信号/IPDU 配置表 (ICCE DK CAN 映射) |
| `dk_dcm_cfg.c` | `bsw_integration/src/` | DCM 配置表 (11 DID + 4 RID) |
| `dk_dem_cfg.c` | `bsw_integration/src/` | DEM 配置表 (16 事件, 16 DTC, 64 入口) |
| `dk_diag_callbacks.c` | `bsw_integration/src/` | UDS DID read/write + RID 回调实现 |
| `dk_pdur_lcfg.c` | `bsw_integration/src/` | PduR 路由表 (4 路径: DK Status / DK Cmd / Diag Resp / Diag Req) |

### 修改文件 (5 个)

| 文件 | 变更 |
|:-----|:------|
| `CMakeLists.txt` | 添加 COM/DCM/DEM/PduR 库目标 + yuleDKCS 配置源 |
| `main.c` | 添加 COM/DCM/DEM/PduR init + Dcm_Start + Dem 操作周期 |
| `dk_os_tasks.c` | 添加 COM/DCM/DEM 主函数到 10ms/50ms/100ms 任务 |
| `bsw_stubs.c` | 添加 Dem_GetDTCStatus 包装器 + Dcm_Start/Dcm_Stop 存根 |
| `MemMap.h` | 添加 COM/DCM/DEM/PduR 内存段支持 |
| `Compiler.h` | 添加 COM_CONST/DCM_CONST/DEM_CONST 定义 |

## COM 配置

### 信号映射 (ICCE DK CAN)

| 信号 ID | 名称 | 位域 | 端序 | 传输属性 | IPDU |
|:--------|:-----|:-----|:-----|:---------|:-----|
| 0 | DK_SIG_BOOT_STATUS | bit 0-7 | LE | TriggeredOnChange | 0 (TX) |
| 1 | DK_SIG_BLE_CONNECTED | bit 8-15 | LE | TriggeredOnChange | 0 |
| 2 | DK_SIG_VEHICLE_LOCK_STATE | bit 16-23 | LE | TriggeredOnChange + Filter | 0 |
| 3 | DK_SIG_ENGINE_STARTED | bit 24-31 | LE | TriggeredOnChange | 0 |
| 4 | DK_SIG_CMD_DOOR_LOCK | bit 32-39 | LE | Triggered | 1 (RX) |
| 5 | DK_SIG_CMD_ENGINE_START | bit 40-47 | LE | Triggered | 1 |
| 6 | DK_SIG_CMD_TRUNK_OPEN | bit 48-55 | LE | Triggered | 1 |
| 7 | DK_SIG_CMD_WINDOW | bit 56-63 | LE | Triggered | 1 |

### I-PDU

| IPDU | 方向 | 长度 | 周期 | 用途 |
|:-----|:-----|:-----|:-----|:------|
| 0 (DK_IPDU_TX_DK_STATUS) | TX | 8B | 100ms | CAN ID 0x3C1 - DK 状态 |
| 1 (DK_IPDU_RX_DK_COMMAND) | RX | 8B | - | CAN ID 0x3C2 - DK 命令 |
| 2 (DK_IPDU_TX_DIAG_RESPONSE) | TX | 64B | - | 诊断响应 (CAN-FD) |
| 3 (DK_IPDU_RX_DIAG_REQUEST) | RX | 64B | - | 诊断请求 (CAN-FD) |

## DCM 配置 (UDS)

### DID 定义

| DID | 长度 | 会话 | 安全等级 | 读/写 | 描述 |
|:----|:-----|:-----|:---------|:------|:-----|
| 0xF180 | 20 | 默认 | R | 读 | ECU 标识 |
| 0xF190 | 64 | 扩展 | R | 读 | DK 状态 |
| 0xF191 | 32 | 扩展 | 1 | 读写 | DK 配置 |
| 0xF193 | 8 | 默认 | R | 读 | DK 会话 |
| 0xF194 | 4 | 扩展 | R | 读 | UWB 测距 |
| 0xF195 | 4 | 默认 | R | 读 | BLE 指标 |
| 0xF1A0 | 2 | 默认 | R | 读 | 车辆门锁 |
| 0x0100 | 8 | 默认 | R | 读 | 制造商数据 |
| 0x0101/0x0102 | 4 | 默认 | R | 读 | 软件版本 |
| 0x0103 | 4 | 默认 | R | 读 | 硬件版本 |

### RID 定义

| RID | 会话 | 安全 | 用途 |
|:----|:-----|:-----|:------|
| 0x0001 | 默认 | 0 | ECU 复位 |
| 0x0002 | 默认 | 0 | 安全种子 |
| 0x0100 | 扩展 | 1 | 诊断自测 |
| 0x0101 | 编程 | 2 | 工厂复位 |

## DEM 配置

### DTC 事件表 (16 个)

| EventID | DTC | 功能单元 | 去抖 | 优先级 |
|:--------|:----|:---------|:-----|:-------|
| 1 | 0x010101 | ECU | 计数 | 1 (ECU 内部错误) |
| 2 | 0x010102 | ECU | 时间 1s | 2 (电压异常) |
| 3 | 0x010103 | ECU | 计数 | 2 (温度异常) |
| 4 | 0x010104 | ECU | 无 | 1 (NvM CRC) |
| 5 | 0x010301 | CAN | 监控 | 1 (CAN Bus Off) |
| 6 | 0x010302 | CAN | 计数 | 2 (CAN 超时) |
| 7 | 0x020101 | BLE | 计数 | 2 (BLE 通信错误) |
| 8 | 0x020102 | BLE | 计数 | 3 (BLE 断连) |
| 9 | 0x020201 | UWB | 计数 | 2 (UWB 通信错误) |
| 10 | 0x020202 | UWB | 时间 1s | 3 (UWB 测距超时) |
| 11 | 0x030101 | 安全 | 无 | 1 (安全访问违规) |
| 12 | 0x030102 | 安全 | 计数 | 1 (密钥超限) |
| 13 | 0x030103 | 安全 | 无 | 1 (证书过期) |
| 14 | 0x030201 | 安全 | 计数 | 2 (安全通道失败) |
| 15 | 0x030301 | 安全 | 无 | 1 (防降级违规) |
| 16 | 0x040101 | NFC | 计数 | 2 (NFC 错误) |

## PduR 路由

| 路径 | 源模块 | 源 PDU | 目的模块 | 类型 |
|:-----|:-------|:-------|:---------|:-----|
| 0 | COM | DK_IPDU_TX_DK_STATUS (0) | CanIf | 立即 |
| 1 | CanIf | DK_IPDU_RX_DK_COMMAND (1) | COM | 立即 |
| 2 | DCM | DK_IPDU_TX_DIAG_RESPONSE (2) | CanIf | 立即 |
| 3 | CanIf | DK_IPDU_RX_DIAG_REQUEST (3) | DCM | 立即 |

## 代码修复

1. **DCM_SID_DEINIT 缺失**: Dcm.h 声明 `#define DCM_SERVICE_ID_DEINIT DCM_SID_DEINIT` 但 DCM_SID_DEINIT 未定义。在 Dcm_Cfg_Dk.h 中补充定义 `(0x06U)`。

2. **DCM_SECURITY_DELAY_TIME 命名不一致**: Dcm.c 使用 `DCM_SECURITY_DELAY_TIME`，但 Cfg.h 定义 `DCM_SECURITY_DELAY_TIME_MS`。在 Dcm_Cfg_Dk.h 中添加别名。

3. **Dem_GetDTCStatus 缺失**: Dcm.c 调用 `Dem_GetDTCStatus()` 但 Dem.h 中只声明 `Dem_GetStatusOfDTC()` (同名签名的包装器)。在 bsw_stubs.c 中添加包装函数。

4. **Dcm_Start/Dcm_Stop 未实现**: Dcm.h 声明但 Dcm.c 未实现。在 bsw_stubs.c 中添加空实现。

5. **DTC 参数数组越界**: Dcm.c 使用 `DEM_NUM_DTCS` (64) 遍历 `Dem_Config.DtcParameters`，需要数组至少有 DEM_NUM_DTCS 个元素。dk_dem_cfg.c 中填充保留条目。

## yuleASR 源码修复

Phase 2 集成过程中发现并修复了 yuleASR 的以下源码缺陷：

| 文件 | 缺陷 | 修复 |
|:-----|:-----|:-----|
| `Dcm.c` | `DCM_SID_DEINIT` 未定义 | 通过 compile_definitions 补丁 `DCM_SID_DEINIT=6` |
| `Dcm.c` | `DCM_SECURITY_DELAY_TIME` 未定义（只有 `_MS` 变体） | 通过 compile_definitions 补丁 |
| `Dcm.c` | `DCM_E_REQUEST_SEQUENCE_ERROR` 未定义（只有 `DCM_E_REQUESTSEQUENCEERROR`） | 通过 compile_definitions 别名 |
| `Dcm.c` | `Dem_GetDTCStatus()` 未声明（原型为 `Dem_GetStatusOfDTC`） | 通过 compile_definitions 宏别名: `Dem_GetDTCStatus=Dem_GetStatusOfDTC` |
| `Dcm.c` | `Dem_Config` 外部变量未声明 | 在 Dcm.c 中添加 `extern const Dem_ConfigType Dem_Config;` |
| `Dcm.c` | `Dcm_ResetToDefaultSession()` 返回类型与声明不一致 | 将实现改为 `void` + 移除无效的 return 语句 |
| `Dcm.c` | `responseData[4]` 越界访问 index 4 | 数组改为 `[5]` |
| `Dem.c` | `DEM_SID_GETOPERATIONCYCLESTATE` 未定义 | 通过 compile_definitions 补丁 |
| `Dem.c` | `Dem_DTCEntryType` 缺少 `AgingThreshold` 字段 | 在 `Dem_Int.h` 的 struct 中添加该字段 |
| `Dem_Int.h` | 未包含 `Dem.h`（导致 `DEM_MODULE_ID` 等未定义） | 添加 `#include "Dem.h"` |
| `freestanding_includes/string.h` | GCC 16 freestanding 下 `size_t` 未定义 | 改为包含 `<stddef.h>` |

## 编译验证

### 工具链
- arm-none-eabi-gcc v16.1.0 (目标: cortex-m7, float-abi=hard, fpv5-sp-d16)
- CMSIS FreeRTOS port for S32K312

### 单文件编译测试 (全部通过)

```
PduR.c       ✅  (yuleASR PDU Router)
Com.c        ✅  (yuleASR Communication Stack)
Dem.c        ✅  (yuleASR Diagnostic Event Manager)
Dem_Int.c    ✅  (yuleASR DEM Internal)
Dcm.c        ✅  (yuleASR Diagnostic Communication Manager)
dk_com_cfg.c ✅  (yuleDKCS COM config — 8 signals, 4 IPDUs)
dk_pdur_lcfg.c ✅ (yuleDKCS PduR config — 4 routing paths)
dk_dcm_cfg.c ✅  (yuleDKCS DCM config — 11 DID + 4 RID)
dk_dem_cfg.c ✅  (yuleDKCS DEM config — 16 events, 64 DTC entries)
dk_diag_callbacks.c ✅ (yuleDKCS UDS callbacks — DID read/write + RID)
```

编译选项: `-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -Os -ffreestanding -Wall -Wextra -Werror`

### 模块依赖
Phase 2 库链接顺序: `com → pdur → dcm → dem → dk_diag → stubs`
构建目标: `bsw_phase2_all` / `bsw_phase2_size` / `bsw_phase2_info`

---

*Report generated 2026-07-08 21:45 CST*
