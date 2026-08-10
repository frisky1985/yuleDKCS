# KW47A 平台移植指南

## 前提条件

1. 下载 NXP MCUXpresso SDK for KW47A:
   ```bash
   git clone https://github.com/NXP/mcux-sdk.git -b release/2.16.x
   # 设置环境变量
   export MCUX_SDK_PATH=/path/to/mcux-sdk
   ```

2. 安装 ARM 交叉编译工具链:
   ```bash
   brew install arm-none-eabi-gcc  # macOS
   # 或 apt install gcc-arm-none-eabi  # Linux
   ```

3. 安装 CMake + Ninja:
   ```bash
   brew install cmake ninja
   ```

## 构建

```bash
cd firmware
./build.sh debug      # Debug 构建
./build.sh release    # Release 构建 (含 LTO)
```

## 目录结构

```
firmware/
├── CMakeLists.txt          # 根构建文件
├── build.sh                # 一键构建脚本
├── cmake/
│   └── arm-none-eabi.cmake # 交叉编译工具链
├── hal/
│   ├── hal_interface.h     # HAL API 接口
│   ├── CMakeLists.txt
│   └── kw47a/
│       ├── kw47a_board.h   # 板级配置
│       ├── kw47a_flash.ld  # 链接脚本 (A/B分区)
│       └── hal_kw47a.c     # KW47A HAL 实现
├── include/
│   └── fw_version.h        # 固件版本
└── src/
    ├── CMakeLists.txt
    └── main.c              # 固件入口
```

## 移植步骤

1. 安装工具链 + 下载 MCUX SDK
2. 调整 `hal/kw47a/kw47a_board.h` 中的引脚配置
3. 调整 `hal/kw47a/kw47a_flash.ld` 中的 Flash 分区大小
4. 补充 `hal/kw47a/hal_kw47a.c` 中未实现的 HAL 函数
5. 运行 `./build.sh debug`
6. 用 J-Link 烧录 `build/debug/yuleDKCS_kw47a.hex`
