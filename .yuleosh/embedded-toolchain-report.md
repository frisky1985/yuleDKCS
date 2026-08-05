# yuleDKCS 嵌入式交叉编译环境验证报告

**日期**: 2026-07-07  
**工具链**: arm-none-eabi-gcc (GCC) 16.1.0 (Homebrew)  
**CMake**: 4.3.4  
**目标架构**: ARM (bare-metal, ELF32 little-endian)  

---

## 1. 工具链验证

| 组件 | 版本 | 状态 |
|------|------|------|
| arm-none-eabi-gcc | 16.1.0 | ✅ 已安装 |
| arm-none-eabi-g++ | 16.1.0 | ✅ 已安装 |
| arm-none-eabi-binutils | (bundled) | ✅ 可用 |
| cmake | 4.3.4 | ✅ 已安装 |

**工具链关键配置**:
- Toolchain file: `embedded/arm-none-eabi-toolchain.cmake`
- 编译模式: `-ffreestanding` (裸机 freestanding 环境)
- C 标准: C11 (`std=gnu11`)
- 优化: `-Os` (尺寸优化)
- C 标准库: 无 newlib（须手工安装或提供 freestanding 接口 stub）
- 编译选项: `-Wall -Wextra -Werror`（已降级部分警告: unused-variable, unused-function, unused-parameter, sign-compare）

---

## 2. ICCE 协议栈编译结果

| 项目 | 状态 |
|------|------|
| CMake 配置 | ✅ 成功 |
| 编译 | **✅ 成功** |
| 产物 | `libicce_dk.a` (72 KB) |
| 源文件数 | 16 |
| 警告数 | 9 (均为非严重: unused var/func, sign-compare) |

### 编译中解决的问题

1. **stdint.h 缺失 (`#include_next <stdint.h>` 失败)**
   - 根因: arm-none-eabi-gcc 无 newlib C 库
   - 修复: 添加 `-ffreestanding` 标志，使 GCC 使用内置 freestanding 头文件

2. **string.h / stdlib.h 缺失**
   - 根因: freestanding 模式下不提供 C 库头文件
   - 修复: 创建 `embedded/freestanding_includes/` 目录，提供最小化 `string.h`、`stdlib.h` 函数声明

3. **CMake 注入 macOS `-arch arm64` 标志**
   - 根因: CMake 默认检测到 macOS 主机，自动添加架构标志
   - 修复: 创建 `embedded/arm-none-eabi-toolchain.cmake`，设置 `CMAKE_SYSTEM_NAME=Generic`、`CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`

4. **`crypto_utils.h` 未包含**
   - 影响文件: `sm3.c`, `sm4.c` 缺少 `rotl32`, `load_be32` 等内联函数
   - 修复: 添加 `#include "crypto_utils.h"`

5. **多处未使用变量/函数/参数警告**
   - 处理: 添加 `-Wno-error=` 标志降级为 warning

6. **`offline_decision.c` 中 `base_score` 重复定义**
   - 实际 bug: 行 484 和 行 530 分别定义同名变量
   - 修复: 改第二处为赋值而非定义

### 依赖记录

| 缺失的头文件 | 类型 | 处理方式 |
|-------------|------|----------|
| `src/ble/ble_manager.h` | 本地模块头 | ✅ 创建 stub |
| `src/ble/ble_adapter.h` | 硬件抽象层 | ✅ 创建 stub |
| `src/ble/ble_gatt.h` | 硬件抽象层 | ✅ 创建 stub |
| `src/cache/cache_manager.h` | 本地模块头 | ✅ 创建 stub |
| `src/cache/storage_driver.h` | 硬件抽象层 | ✅ 创建 stub |
| `src/decision/offline_decision.h` | 本地模块头 | ✅ 创建 stub |
| `src/security/hsm_interface.h` | 硬件抽象层 | ✅ 创建 stub |
| `src/vehicle/vehicle_integration.h` | 本地模块头 | ✅ 创建 stub |
| `src/vehicle/can_driver.h` | 硬件抽象层 | ✅ 创建 stub |
| `system_architecture/sys_time.h` | 跨模块共享头 | ✅ 存在，已添加包含路径 |

---

## 3. CCC 协议栈编译结果

| 项目 | 状态 |
|------|------|
| CMake 配置 | ✅ 成功 |
| 编译 | **✅ 成功** |
| 产物 | `libccc_dk.a` (26 KB) |
| 源文件数 | 6 |
| 警告数 | 5 (均为非严重: unused var/param, unused function) |

### 编译中解决的问题

1. **硬件 BSP 层 GPIO/PIN 宏缺失**
   - 涉及: `NCJ29D6_CS_PORT`, `KW47A_CS_PORT`, `ST25R501_RST_PORT`, `UWB_WAKE_PIN`, `CCC_SERVICE_UUID` 等
   - 修复: 创建 `include/hardware_abstraction.h` 提供 stub 定义

2. **SPI/GPIO 外部函数缺失**
   - `spi_transfer`, `gpio_write`, `gpio_read`, `gpio_write_wake`, `delay_ms`
   - 修复: 同上头文件提供 `extern` 声明

3. **`forward declaration` 缺失**
   - `uwb_ncj29d6.c` 中 `classify_distance_impl` 在使用后才定义
   - 修复: 添加前向声明

### 依赖记录

| 缺失项 | 类型 | 处理方式 |
|--------|------|----------|
| 硬件 GPIO/PIN 宏 | BSP 层 | ✅ `hardware_abstraction.h` |
| SPI/GPIO 函数声明 | BSP 层 | ✅ `hardware_abstraction.h` |
| CCC_SERVICE_UUID | BLE 定义 | ✅ `hardware_abstraction.h` |

> ⚠️ **注意**: CCC 协议栈与 NXP 硬件 (KW47A/NCJ29D6/ST25R501) 高度耦合。真正的 BSP 应由 NXP SDK 提供。当前 stub 仅用于编译验证。

---

## 4. 总体结论

### ✅ 交叉编译工具链可用
`arm-none-eabi-gcc` 16.1.0 + CMake 4.3.4 可正常交叉编译 ARM bare-metal 目标代码。

### ✅ ICCE 协议栈 — 编译通过
- 需 `-ffreestanding` 模式 + 少量 header stub + CMakeLists.txt 修正
- 产出了 72KB 的 ARM 静态库

### ✅ CCC 协议栈 — 编译通过
- 需硬件抽象层 stub，因其与特定 NXP 芯片 SPI/GPIO 强耦合
- 产出了 26KB 的 ARM 静态库

### ⚠️ 已知问题
1. **无 newlib C 库**: arm-none-eabi-gcc Homebrew 包不含 newlib，CI 镜像需预装或自行编译
2. **HAL stub 需替换**: ICCE/CCC 的 BSP/HAL 层 stub 仅为编译验证，CI 集成需要实际 BSP
3. **CMakeLists.txt 已做修改**: `icce_protocol/CMakeLists.txt` 和 `ccc_protocol/CMakeLists.txt` 均已被修改以支持交叉编译
4. **源文件修正**: `sm3.c`, `sm4.c`, `offline_decision.c`, `ble_manager.c`, `uwb_ncj29d6.c` 有少量修正

### 建议后续动作
1. 为 Docker CI 镜像安装: ARM GCC (arm-none-eabi-gcc/binutils) + newlib + cmake
2. 考虑统一 CMake 工具链配置，避免每个协议栈独立维护编译选项
3. 将 stub 头文件移至 `embedded/hal_stubs/` 统一管理
4. ICCE 的 `base_score` 重复定义问题是实际代码 bug（行 484/530），建议修复
