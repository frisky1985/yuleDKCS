# yuleDKCS HIL 硬件在环测试规范

> **版本**: 1.0.0  
> **日期**: 2026-07-16  
> **状态**: 正式版  
> **关联阶段**: P0-2 硬件在环测试  
> **前置条件**: S32K312 EVB 就绪, 固件镜像 v1.2+

---

## 1. 测试目的与范围

### 1.1 测试目的

HIL（Hardware-in-the-Loop）测试旨在使用真实硬件（S32K312 EVB + KW47 BLE 模组 + ST25R501 NFC 读卡器 + SE050 安全芯片）验证 yuleDKCS 数字钥匙系统的车端嵌入式软件在实际硬件环境中的行为。

核心验证目标：

- **功能正确性**: 所有协议栈（CCC / ICCOA / ICCE）在真实硬件上正确运行
- **时序合规**: 解锁响应时间、NFC 刷卡延迟、BLE 连接速度满足性能指标
- **安全有效性**: SCP03 安全通道、防中继逻辑、故障注入场景下安全机制正确触发
- **稳定性**: 长时间运行、异常恢复、多设备并发场景下系统不崩溃
- **电源管理**: 休眠电流、唤醒延迟符合车载低功耗要求

### 1.2 测试范围

| 测试域 | 范围 | 深度 |
|--------|------|------|
| BLE 通信 | KW47 BLE 连接、GATT 服务、数据吞吐 | 正常 + 异常 + 边界 |
| NFC 通信 | ST25R501 刷卡、APDU 交易、多卡处理 | 正常 + 冲突 + 超时 |
| UWB 测距 | NCJ29D6 距离/角度测量 | 精度 + 稳定性 |
| SE050 安全 | SCP03 通道、密钥管理、签名验签 | 正常 + 故障注入 |
| 解锁流程 | BLE + NFC 两种解锁路径 | 端到端 + 时序 |
| 车况同步 | 状态推送、离线缓存、云端同步 | 实时 + 延迟 |
| 电源管理 | 休眠、唤醒、低电量 | 电流 + 延迟 |
| 故障注入 | 通信异常、芯片故障、电源掉电 | 系统鲁棒性 |

### 1.3 排除范围

- 云端服务端到端测试（由单独的系统集成测试覆盖）
- 手机 App UI 测试（由移动端测试覆盖）
- 安全认证/渗透测试（由独立的安全测试团队执行）
- 量产级 EMC/环境测试（由整车级测试覆盖）

---

## 2. 硬件清单

### 2.1 核心硬件

| 编号 | 硬件 | 型号 | 数量 | 用途 |
|------|------|------|:----:|------|
| H1 | S32K312 EVB | NXP S32K312-QFP176 EVB | 1 | 主控 MCU, 运行数字钥匙协议栈 |
| H2 | BLE 模组 | NXP KW47A (Murata Type 2ND) | 1 | BLE 5.4 通信 |
| H3 | NFC 读卡器 | ST ST25R501 | 1 | NFC Type 5 / ISO 14443A 通信 |
| H4 | UWB 模组 | NXP NCJ29D6 | 1 | UWB 精准测距 (6.5-8.0 GHz) |
| H5 | 安全芯片 | NXP SE050C1 | 1 | 安全元件, 密钥存储与密码学运算 |
| H6 | 调试器 | SEGGER J-Link PLUS | 1 | SWD 调试与固件烧录 |
| H7 | USB 串口线 | FTDI USB-UART (3.3V) | 1 | UART 日志输出 |

### 2.2 辅助硬件

| 编号 | 硬件 | 用途 |
|------|------|------|
| A1 | 手机（iPhone 14+/Android 13+）*2 | 模拟手机端 BLE 客户端 + NFC 刷卡 |
| A2 | NXP NFC 读卡器 | 独立 NFC 读卡器用于调试 |
| A3 | BLE 嗅探器（nRF5340 DK + Wireshark） | BLE 空中抓包分析 |
| A4 | 可调直流电源 12V/5A | EVB 供电 + 模拟车辆电源波动 |
| A5 | 数字万用表 / 示波器 | 电压/电流/时序测量 |
| A6 | 电磁屏蔽箱 | NFC/UWB 近距离测试环境控制 |
| A7 | 逻辑分析仪 16ch (例如 Saleae Logic Pro 16) | SPI/I2C/UART 总线抓取 |

### 2.3 推荐 NXP 原厂配件

- **S32K3-EVB** 包装内含 J-Link OB, 无需额外调试器
- **KW47 扩展板** RD-KW47-BLE 或 FRDM-KW47
- **NCJ29D6 扩展板** MDEK-2-REF-UWB-SK

---

## 3. 环境搭建

### 3.1 硬件接线图

```
                           ┌──────────────────────────┐
    ┌──────────────────────┤      S32K312 EVB          ├──────────────────────┐
    │                      │                           │                       │
    │   J1 (Debug USB)    │     ┌─────┐              │   J2 (UART)          │
    │   ◄──── J-Link OB   │     │MCU  │              │   ◄──── USB-UART     │
    │                      │     └──┬──┘              │   (115200 8N1)      │
    │                      │        │                  │                       │
    ├──────────────────────┼────────┼──────────────────┼──────────────────────┤
    │                      │        │                  │                       │
    │  ┌────────┐          │   SPI1 │  ┌──────────┐    │  ┌────────────┐      │
    │  │ KW47   │◄─────────┼─── UART┤  │ ST25R501 │    │  │ NCJ29D6   │      │
    │  │ BLE    │          │        │  │ NFC Reader│    │  │ UWB       │      │
    │  │ Module │          │        │  └──────────┘    │  │ Module    │      │
    │  └────────┘          │        │                  │  └────────────┘      │
    │                      │   I2C0 │                  │   SPI2               │
    │                      ├────────┼──────────────────┤                      │
    │                      │  ┌──────────┐            │                      │
    │                      │  │ SE050   │             │                      │
    │                      │  │ Secure  │             │                      │
    │                      │  │ Element │             │                      │
    │                      │  └──────────┘            │                      │
    └──────────────────────┴──────────────────────────┴──────────────────────┘
                                  │
                           ┌──────┴──────┐
                           │  12V DC     │
                           │  Power Supply│
                           └─────────────┘
```

### 3.2 接口映射

| MCU 外设 | 连接目标 | 引脚分配 | 速率 |
|----------|---------|---------|------|
| LPSPI1 | ST25R501 NFC | PTC13-16 | 10 MHz |
| LPSPI2 | NCJ29D6 UWB | PTD0-3 | 20 MHz |
| LPI2C0 | SE050 | PTA24-25 | 400 kHz |
| LPUART1 | KW47 BLE | PTB0-1 | 921600 bps |
| LPUART2 | 调试日志输出 | PTC6-7 | 115200 bps |
| SWD/JTAG | J-Link Debug | SWDIO/SWCLK | 4 MHz |

### 3.3 供电说明

1. **EVB 主供电**: 12V/3A 直流电源接入 J8 (DC-IN) 
   - S32K312 核心电压: 1.25V (内部 LDO)
   - I/O 电压: 3.3V / 5V
2. **KW47 BLE**: 由 EVB 通过 LPUART1 接口供电 (3.3V)
3. **ST25R501**: 需要独立 5V 供电（NFC 场强需求）
   - 也可从 EVB 5V 输出取电（最大 200mA，注意电流限制）
4. **NCJ29D6**: 3.3V 由 EVB 供电，峰值电流 150mA
5. **SE050**: 3.3V / 1.8V，由 EVB 供电

> ⚠️ **注意**: 首次上电前用万用表测量各供电节点对地阻抗，确认无短路

### 3.4 软件工具链

| 工具 | 版本 | 用途 |
|------|------|------|
| S32 Design Studio | 3.5+ | MCU 固件开发与调试 |
| ARM GCC | 10.3.1 (arm-none-eabi) | 命令行编译 |
| S32K3xx SDK | RTD 2.0.0 | MCAL 驱动包 |
| CMake | 3.22+ | 构建系统 |
| Python | 3.10+ | 自动化测试脚本 |
| pytest | 7.4+ | 测试框架 |
| nRF Connect | 2.6+ | BLE 扫描/连接测试 |
| Wireshark | 4.0+ | BLE 空中抓包 (需 nRF Sniffer) |
| Putty / screen | latest | UART 日志监控 |
| SEGGER J-Link | 7.94+ | 固件烧录与调试 |

### 3.5 固件烧录步骤

```bash
# 1. 连接 J-Link OB 到 PC (USB)
# 2. 确认设备识别
lsusb | grep -i "SEGGER\|J-Link"

# 3. 编译 HIL 测试固件
cd /Users/stefan/yuleDKCS/embedded
cmake -B build/hil -DCMAKE_TOOLCHAIN_FILE=toolchain/s32k312.cmake \
      -DENABLE_TESTS=ON -DENABLE_HIL_TESTS=ON -DDK_FAULT_INJECT_ENABLE=1
cmake --build build/hil -j4

# 4. 烧录固件
JLinkExe -device S32K312 -if SWD -speed 4000 -autoconnect 1 \
    -CommanderScript scripts/flash_hil.jlink

# 5. 打开 UART 日志
screen /dev/tty.usbserial-* 115200

# 6. 验证固件运行
# 应看到: "[HIL] yuleDKCS HIL test firmware v1.2.0 starting..."
```

### 3.6 环境验证清单

- [ ] EVB 上电后绿色 LED (PWR) 常亮
- [ ] UART 日志正常输出 (115200 8N1)
- [ ] J-Link 调试器识别 MCU
- [ ] KW47 BLE 模组广播可扫描到 (广播名: "yuleDKCS-XXXX")
- [ ] ST25R501 NFC 场强检测 (用 NFC 卡片靠近可触发)
- [ ] SE050 I2C 通信正常 (可通过 SCP03 建立安全通道)
- [ ] NCJ29D6 UWB 初始化成功 (日志显示 UWB ready)
- [ ] 网络时间同步 (ping google.com 成功)
- [ ] Python 测试环境可用 (`python3 -c "import pytest"` 无报错)
- [ ] nRF Connect 可扫描到 yuleDKCS BLE 设备

---

## 4. 测试用例矩阵

### 4.1 总览

| 测试域 | 用例数 | 执行时间 | 优先级 |
|--------|:------:|:---------:|:------:|
| BLE 连接稳定性 | 5 | ~15 min | P0 |
| UWB 测距精度 | 4 | ~20 min | P1 |
| NFC 通信 | 4 | ~10 min | P0 |
| SE050 SCP03 | 5 | ~15 min | P0 |
| 解锁响应时间 | 4 | ~10 min | P0 |
| 车况同步 | 3 | ~15 min | P1 |
| 电源管理 | 3 | ~30 min | P2 |
| 故障注入 | 6 | ~20 min | P1 |
| 唤醒源 | 3 | ~15 min | P1 |
| **合计** | **37** | **~150 min** | |

### 4.2 BLE 连接稳定性

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-BLE-01 | BLE 标准连接 | 手机 App 通过 BLE 连接 S32K312 EVB | EVB 上电, BLE 广播中 | 1. 手机打开 yuleDKCS App<br>2. 扫描 BLE 设备<br>3. 选择"yuleDKCS-XXXX"连接<br>4. 配对完成后记录连接耗时 | 连接建立成功, 耗时 < 3s | P0 |
| HIL-BLE-02 | RSSI 阈值测试 | 在不同信号强度下测试 BLE 连接稳定性 | EVB 上电, BLE 广播 | 1. 手机距离 EVB 0.1m/1m/5m/10m/20m<br>2. 各距离测试 5 次连接<br>3. 记录 RSSI 值和连接成功率 | 0.1-5m 成功率 100%, 10m > 80%, 20m > 50% | P0 |
| HIL-BLE-03 | 断线重连 | BLE 连接断开后自动/手动重连 | BLE 已连接 | 1. 断开 BLE 连接 (关手机蓝牙 / 拉远距离)<br>2. 等待 3s/5s/10s 后重连<br>3. 每种间隔测试 10 次 | 自动重连 < 3s (距离恢复后), 手动重连成功率 100% | P1 |
| HIL-BLE-04 | 多设备并发连接 | 多台手机同时连接同一 EVB | EVB 上电, BLE 广播 | 1. 手机 A 连接 EVB<br>2. 手机 B 连接 EVB<br>3. 手机 C 连接 EVB<br>4. 同时发送解锁指令 | 至少 3 台设备可同时连接, 均可发送指令 | P1 |
| HIL-BLE-05 | BLE GATT MTU 协商 | 验证 GATT MTU 协商为 512 bytes | BLE 已连接 | 1. 连接后发起 MTU 协商请求<br>2. 记录协商后的 MTU 值<br>3. 发送 256/512/1024 bytes 数据包 | MTU >= 512 bytes, 大数据包正确收发 | P2 |

### 4.3 UWB 测距精度

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-UWB-01 | 1m 近距离精度 | 手机距 EVB 1m 时的测距精度 | UWB 初始化完成, 手机支持 UWB | 1. 将手机固定在 1m 距离的支架上<br>2. 持续采集 100 组测距数据<br>3. 计算平均值、标准差、最大误差 | 平均值偏差 < ±10cm, 标准差 < 5cm | P1 |
| HIL-UWB-02 | 5m 中距离精度 | 手机距 EVB 5m 时的测距精度 | 同上 | 1. 手机固定在 5m 距离支架上<br>2. 采集 100 组数据<br>3. 统计分析 | 平均值偏差 < ±15cm, 标准差 < 10cm | P1 |
| HIL-UWB-03 | 10m 远距离精度 | 手机距 EVB 10m 时的测距精度 | 同上 | 1. 手机固定在 10m 距离支架上<br>2. 采集 100 组数据<br>3. 统计分析 | 平均值偏差 < ±25cm, 标准差 < 15cm | P1 |
| HIL-UWB-04 | 20m 极限距离 | 手机距 EVB 20m 时的测距稳定性 | 同上, 开阔环境 | 1. 手机固定在 20m 位置<br>2. 采集 100 组数据<br>3. 记录成功率 | 测距成功率 > 70%, 偏差 < ±50cm | P1 |

### 4.4 NFC 通信

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-NFC-01 | NFC 标准刷卡解锁 | 手机 NFC 靠近 EVB NFC 天线完成解锁 | EVB 上电, NFC 初始化完成 | 1. 手机靠近 NFC 天线 (< 4cm)<br>2. 触发 NFC 场检<br>3. APDU SELECT AID → GET CHALLENGE → 认证 → UNLOCK<br>4. 记录完整流程耗时 | 完整刷卡解锁 < 500ms | P0 |
| HIL-NFC-02 | NFC 多卡共存 | NFC 区域同时存在多张卡片/手机 | 多张卡片/手机 | 1. 在 NFC 天线区域放置 2 张 ISO 14443 卡片<br>2. 放置 1 台支持 NFC 的手机<br>3. 依次刷卡解锁 | 每张卡片都能正确识别, 无冲突丢失 | P1 |
| HIL-NFC-03 | NFC 超时处理 | 手机 NFC 刷卡过程中途移开 | NFC 通信进行中 | 1. 手机开始 NFC 认证<br>2. 在认证过程中移开手机<br>3. 等待 5s 后重新刷卡 | 原交易超时取消, 重新刷卡可正常完成 | P0 |
| HIL-NFC-04 | NFC 场强与距离 | 不同距离下 NFC 刷卡成功率 | EVB 上电 | 1. 从 0cm 到 10cm 每 0.5cm 测试一次<br>2. 每个距离测试 10 次<br>3. 记录刷卡成功率和场强 RSSI | 0-4cm 成功率 100%, 4-6cm > 50%, > 6cm = 0% | P2 |

### 4.5 SE050 SCP03 安全通道

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-SE-01 | SCP03 标准建链 | 使用正确密钥建立 SCP03 安全通道 | SE050 已初始化, 密钥注入完成 | 1. 调用 SE050 SCP03 建链 API<br>2. 使用预置 SCP03 密钥 (ENC/MAC/DEK)<br>3. 完成双向认证<br>4. 记录建链耗时 | 建链成功, 耗时 < 200ms | P0 |
| HIL-SE-02 | SCP03 建链失败(错误密钥) | 使用错误密钥建链应拒绝 | SE050 就绪 | 1. 使用错误 SCP03 ENC 密钥<br>2. 发起 SCP03 建链请求<br>3. 记录返回错误码 | SW69 84 (引用数据无效), 安全通道未建立 | P0 |
| HIL-SE-03 | 密钥注入与签名 | 向 SE050 注入新密钥并使用签名 | SCP03 通道已建立 | 1. 通过 SCP03 注入 ECDSA P-256 密钥对<br>2. 使用 SE050_Sign() 对已知数据签名<br>3. 验证签名正确性 | 签名成功, 验签通过, 密钥持久化 | P0 |
| HIL-SE-04 | 密钥更新 | 更新 SE050 中的密钥 | 同上 | 1. 通过 SCP03 PUT KEY 更新现有密钥<br>2. 验证新密钥可正常使用<br>3. 旧密钥已失效 | 更新成功, 新旧密钥切换正确 | P1 |
| HIL-SE-05 | 密钥删除 | 删除 SE050 中的指定密钥 | 同上 | 1. 调用删除 API 删除指定密钥<br>2. 尝试使用已删除密钥签名<br>3. 重新注入同名密钥 | 删除后密钥不可用, 重新注入后恢复正常 | P2 |

### 4.6 解锁响应时间

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-UL-01 | BLE 解锁 < 1s | 手机通过 BLE 完成解锁认证的端到端时间 | EVB 上电, BLE 已连接, 钥匙已绑定 | 1. 手机 App 点击解锁按钮<br>2. 计时从 App 发送解锁指令到收到 EVB 确认<br>3. 包含 BLE 传输 + 安全认证 + 执行时间<br>4. 重复 20 次, 取 P95 值 | 端到端解锁 < 1s (P95) | P0 |
| HIL-UL-02 | NFC 解锁 < 500ms | 手机 NFC 刷卡完成解锁 | EVB 上电, NFC 就绪 | 1. 手机靠近 NFC 天线开始交易<br>2. 计时从场检到解锁成功<br>3. 重复 20 次 | 端到端 NFC 解锁 < 500ms (P95) | P0 |
| HIL-UL-03 | UWB 自动解锁 | UWB 测距进入 2m 区域自动触发解锁 | UWB 初始化, BLE 安全通道已建立 | 1. 手机从 10m 外走向 EVB<br>2. 进入 2m 区域时自动触发解锁<br>3. 记录从进入区域到解锁成功的时间 | 自动解锁 < 800ms, 距离 2m 触发 | P1 |
| HIL-UL-04 | 解锁失败重试机制 | 首次解锁失败后自动/手动重试 | EVB 上电, BLE 已连接 | 1. 注入解锁失败 (如 SE050 暂时不可用)<br>2. 观察系统自动重试行为<br>3. 手动重试行为 | 自动重试 3 次, 间隔 500ms, 最终返回明确错误 | P1 |

### 4.7 车况同步

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-VS-01 | 状态变更推送 | 车辆状态变更后推送到 App | BLE 已连接 | 1. App 发送查询车况请求<br>2. EVB 返回门锁/车窗/引擎状态<br>3. 模拟状态变更 (如引擎熄火)<br>4. 验证推送 | 状态变更 < 1s 内推送到 App | P1 |
| HIL-VS-02 | 离线缓冲 | BLE 断开时状态变更的离线处理 | BLE 已连接 | 1. 断开 BLE<br>2. 模拟车况变更<br>3. 重连 BLE<br>4. 验证离线期间的状态变更同步 | 离线期间变更最多缓冲 100 条, 重连后完整同步 | P2 |
| HIL-VS-03 | 状态变更频控 | 频繁状态变更时的节流和去重 | BLE 已连接 | 1. 快速连续触发状态变更 (100ms 间隔, 50 次)<br>2. 验证发送到 App 的消息数<br>3. 验证去重算法 | 有效变更推送到 App, 无效/重复变更被过滤 | P1 |

### 4.8 电源管理

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-PM-01 | 休眠电流测量 | 系统进入深度休眠后的功耗 | EVB 运行中, 无 BLE 连接 | 1. 设置系统进入深度休眠<br>2. 用万用表串联测量休眠电流<br>3. 持续测量 5 分钟 | 休眠电流 < 100µA (12V 输入) | P2 |
| HIL-PM-02 | BLE 唤醒延迟 | BLE 广播信号唤醒系统的延迟 | 系统处于深度休眠 | 1. 手机靠近 EVB (BLE 检测模式)<br>2. 从 KW47 检测到 BLE 信号到系统完全唤醒<br>3. 用示波器测量唤醒信号到 UART 输出时间 | 唤醒延迟 < 50ms | P1 |
| HIL-PM-03 | 低电量模式 | 模拟电池电压降低时的系统行为 | EVB 运行中 | 1. 调低电源电压至 6V/9V/10.5V<br>2. 在每个电压下测试解锁功能<br>3. 记录系统行为和低压告警 | 6V: 低功耗模式仅 NFC 可用<br>10.5V: 全功能正常 | P2 |

### 4.9 故障注入

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-FI-01 | BLE 通信异常 | BLE 连接过程中数据包丢失/损坏 | BLE 连接中, DK_FAULT_INJECT_ENABLE=1 | 1. 启用 DK_FI_CCC_BLE_DISCONNECT 故障<br>2. 触发 BLE 操作<br>3. 观察系统行为 | 系统应检测到 BLE 断开, 触发重连, 不崩溃 | P1 |
| HIL-FI-02 | SE050 通信故障 | SE050 I2C 通信异常时的系统降级 | SCP03 通道已建立 | 1. 启用 DK_FI_CCC_SECURE_CHANNEL_FAIL<br>2. 触发签名/验签操作<br>3. 观察系统行为 | 返回 HARDWARE 错误, 应用层降级处理, 不崩溃 | P1 |
| HIL-FI-03 | NFC 通信故障 | NFC 读卡器 SPI 通信异常 | NFC 初始化完成 | 1. 启用 DK_FI_CCC_NFC_OOB_CORRUPT<br>2. 触发 NFC 刷卡<br>3. 观察系统行为 | NFC 交易中止, 返回明确错误码, 不阻塞 | P1 |
| HIL-FI-04 | 电源掉电恢复 | 供电突然中断后恢复 | EVB 运行中 | 1. EVB 正常运行时突然拔掉电源<br>2. 等待 5s 后重新上电<br>3. 验证系统恢复行为 | 系统正常重启, NV 数据不丢失, SE050 密钥完好 | P1 |
| HIL-FI-05 | 非法状态机转换 | 强制协议状态机发生非法转换 | BLE 已连接, 安全通道已建立 | 1. 启用 DK_FI_CCC_ILLEGAL_STATE<br>2. 尝试跳过认证状态的非法转换<br>3. 观察系统行为 | 非法转换被拒绝, 状态机回滚到上一个合法状态 | P0 |
| HIL-FI-06 | 签名绕过攻击模拟 | 模拟 ECDSA 签名验证被绕过 | BLE 已连接 | 1. 启用 DK_FI_CCC_SIGNATURE_BYPASS<br>2. 发送伪造签名的解锁请求<br>3. 观察系统行为 | 安全日志记录告警, 系统拒绝解锁 (注: 此测试验证检测机制仍然有效) | P1 |

### 4.10 唤醒源

| 编号 | 用例名称 | 描述 | 前置条件 | 测试步骤 | 预期结果 | P |
|:----:|----------|------|---------|---------|---------|:-:|
| HIL-WK-01 | BLE 唤醒 | BLE 广播信号唤醒休眠系统 | 系统深度休眠 | 1. 系统进入休眠<br>2. 手机靠近至 BLE 范围内<br>3. 测量唤醒到系统就绪的时间 | 唤醒延迟 < 50ms, 系统 500ms 内可响应解锁 | P1 |
| HIL-WK-02 | NFC 唤醒 | NFC 场唤醒休眠系统 | 系统深度休眠 | 1. 系统进入休眠<br>2. 手机靠近 NFC 天线<br>3. 测量唤醒到 NFC 就绪的时间 | NFC 唤醒 < 20ms, 完整刷卡 < 500ms | P1 |
| HIL-WK-03 | 定时唤醒 | RTC 定时器唤醒执行定期任务 | 系统深度休眠 | 1. 设置 5s 后定时唤醒<br>2. 测量实际唤醒时间偏差<br>3. 验证唤醒后执行的任务 (如车况上报) | 定时偏差 < 10ms, 任务正常执行 | P2 |

---

## 5. 测试执行流程

### 5.1 测试前准备

```bash
# 1. 连接硬件并上电
# 2. 烧录 HIL 测试固件
python3 tests/hil/hil_runner.py --flash

# 3. 执行环境自检
python3 tests/hil/hil_runner.py --check-env

# 4. 确认所有模块状态正常
python3 tests/hil/hil_runner.py --status

# 5. 执行全部 HIL 测试
python3 tests/hil/hil_runner.py --all

# 6. 执行指定测试域
python3 tests/hil/hil_runner.py --domain BLE
python3 tests/hil/hil_runner.py --domain SE050,NFC,UNLOCK

# 7. 执行单个测试
python3 tests/hil/hil_runner.py --test HIL-BLE-01
```

### 5.2 测试执行顺序（依硬件依赖排序）

```
Phase 1: 硬件基础验证 (10 min)
  ├── HIL-PM-01 休眠电流基线
  ├── HIL-SE-01  SCP03 标准建链
  └── 环境验证齐全

Phase 2: BLE 通信 (15 min)
  ├── HIL-BLE-01 BLE 标准连接
  ├── HIL-BLE-02 RSSI 阈值测试
  ├── HIL-BLE-03 断线重连
  └── HIL-BLE-05 BLE GATT MTU

Phase 3: 安全通道 (15 min)
  ├── HIL-SE-01~05 SE050 SCP03 全部测试
  └── HIL-UL-01 BLE 解锁时序

Phase 4: NFC 通信 (10 min)
  ├── HIL-NFC-01~04 NFC 全部测试
  └── HIL-UL-02 NFC 解锁时序

Phase 5: UWB 测距 (20 min)
  ├── HIL-UWB-01~04 UWB 全部测试
  └── HIL-UL-03 UWB 自动解锁

Phase 6: 车况与唤醒 (20 min)
  ├── HIL-VS-01~03 车况同步
  ├── HIL-WK-01~03 唤醒源
  └── HIL-PM-02 BLE 唤醒延迟

Phase 7: 故障注入 (20 min)
  ├── HIL-FI-01~06 全部故障测试
  └── HIL-FI-04 电源掉电恢复

Phase 8: 电源管理 (30 min)
  ├── HIL-PM-02 NFC 唤醒
  ├── HIL-PM-01 休眠电流
  ├── HIL-PM-03 低电量模式
  └── HIL-BLE-04 多设备并发
```

### 5.3 测试中断与恢复

| 场景 | 处理方式 |
|------|---------|
| BLE 连接断开 | 自动重试 3 次，间隔 2s，仍失败则标记为 FAIL 并继续下一用例 |
| SE050 通信异常 | 记录错误码，重启 SCP03 通道，重试当前用例 1 次 |
| NFC 刷卡超时 | 等待 3s 后重新刷卡，重试 2 次 |
| 电源波动 | 如果连续 3 个测试因电源问题失败，整组测试中止 |
| 测试脚本崩溃 | pytest 自动捕获异常，标记用例为 ERROR 并继续 |

---

## 6. 测试报告

### 6.1 报告格式

每次测试运行输出两份报告：

1. **JSON 格式** (CI 集成): `tests/hil/reports/hil-report-YYYYMMDD-HHMMSS.json`
2. **HTML 格式** (人类可读): `tests/hil/reports/hil-report-YYYYMMDD-HHMMSS.html`

### 6.2 JSON 报告结构

```json
{
  "metadata": {
    "firmware_version": "1.2.0",
    "hardware_config": "S32K312-EVB + KW47 + ST25R501 + SE050 + NCJ29D6",
    "tester": "auto",
    "start_time": "2026-07-16T10:00:00+08:00",
    "end_time": "2026-07-16T12:30:00+08:00",
    "duration_seconds": 9000
  },
  "summary": {
    "total": 37,
    "passed": 35,
    "failed": 1,
    "error": 1,
    "pass_rate": 94.6,
    "p0_pass_rate": 100.0,
    "p1_pass_rate": 91.7,
    "p2_pass_rate": 80.0
  },
  "results": [
    {
      "test_id": "HIL-BLE-01",
      "name": "BLE 标准连接",
      "domain": "BLE",
      "priority": "P0",
      "status": "PASSED",
      "duration_ms": 1240,
      "measurements": {
        "connection_time_ms": 1240,
        "rssi_dbm": -45
      },
      "details": "连接成功, 3次重试均通过"
    }
  ],
  "failures": [
    {
      "test_id": "HIL-UWB-04",
      "name": "20m 极限距离",
      "reason": "测距成功率仅 55%, 低于 70% 阈值",
      "environment": "室内环境, 走廊长度 22m",
      "suggestion": "建议在开阔室外重新测试"
    }
  ]
}
```

### 6.3 持续集成集成

Jenkins pipeline 集成:

```groovy
// Jenkinsfile 示例
pipeline {
    agent { label 'hil-rig-01' }
    stages {
        stage('HIL Power On') {
            steps { sh 'python3 tests/hil/hil_runner.py --power-on' }
        }
        stage('HIL Tests') {
            steps { sh 'python3 tests/hil/hil_runner.py --all --jenkins' }
        }
        stage('Report') {
            steps {
                junit 'tests/hil/reports/hil-junit.xml'
                archiveArtifacts 'tests/hil/reports/*.json'
            }
        }
    }
    post {
        failure { sh 'python3 tests/hil/hil_runner.py --power-off' }
        success { sh 'python3 tests/hil/hil_runner.py --power-off' }
    }
}
```

---

## 7. 测试环境维护

### 7.1 日常检查

| 检查项 | 频率 | 操作 |
|--------|:----:|------|
| EVB 供电电压 | 每次测试前 | 万用表测量 12V/3.3V/1.25V |
| UART 日志输出 | 每次测试前 | 确认日志正常打印 |
| BLE 广播 | 每次测试前 | nRF Connect 扫描确认 |
| NFC 场强 | 每周 | 用示波器测 NFC 天线波形 |
| SE050 通信 | 每次测试前 | SCP03 建链确认 |
| USB 线缆 | 每月 | 检查接触良好，必要时更换 |
| 隔离屏蔽箱 | 每月 | 确认屏蔽效能 > 60dB |

### 7.2 已知限制

| 限制 | 说明 | 缓解措施 |
|------|------|---------|
| UWB 测距受室内多径影响 | 10m+ 测距精度在室内环境下降 | 测试标注室内/室外环境，室内结果仅供参考 |
| NFC 场强受天线影响 | EVB 板载天线与成品天线场型不同 | 测试使用 EVB 自带天线，量产时需重新标定 |
| SE050 无法仿真/模拟 | 所有 SCP03 测试必须在真实 SE050 上进行 | 无缓解，必须使用真实芯片 |
| 电源管理测试受限 | EVB 的休眠电流不代表量产 PCB 水平 | 标注 EVB 级别，量产前使用原型 PCB 重测 |

### 7.3 问题报告模板

测试过程中发现问题时，请使用以下模板报告：

```
[HIL Bug Report]
Date: 2026-07-16
Test ID: HIL-XXX-XX
Hardware: S32K312 EVB rev.C / KW47 rev.B
Firmware: v1.2.0-abc1234
Description: (问题描述)
Steps to Reproduce: 1. ... 2. ... 3. ...
Actual Result: (实际结果)
Expected Result: (预期结果)
Logs: (关键日志片段)
```

---

## 附录 A：日志输出速查

```
[HIL] yuleDKCS HIL test firmware v1.2.0 starting...
[HIL] MCU: S32K312 rev.3, Core: 160 MHz
[HIL] BLE: KW47A initialized, MAC=AA:BB:CC:DD:EE:FF
[HIL] NFC: ST25R501 ready, firmware v2.1
[HIL] UWB: NCJ29D6 calibrated, session ID=0x12345678
[HIL] SE: SE050 SCP03 channel established
[HIL] TEST: Starting HIL-BLE-01 (BLE 标准连接)
[HIL] TEST: HIL-BLE-01 PASSED (duration=1240ms)
[HIL] TEST: Starting HIL-SE-01 (SCP03 标准建链)
[HIL] TEST: HIL-SE-01 PASSED (duration=45ms)
[HIL] ERROR: HIL-FI-05 检测到非法状态转换, 已阻断
[HIL] TEST: HIL-FI-05 PASSED (检测机制正常)
[HIL] SUMMARY: 37/37 tests passed (100.0%)
```

## 附录 B：GPIO 功能分配

| GPIO | 功能 | 方向 | 备注 |
|------|------|:----:|------|
| PTA0 | BLE_WAKE | Input | KW47 唤醒信号 |
| PTA1 | BLE_IRQ | Input | KW47 中断信号 |
| PTB2 | NFC_IRQ | Input | ST25R501 中断信号 |
| PTB3 | NFC_RESET | Output | ST25R501 复位信号 |
| PTC0 | UWB_IRQ | Input | NCJ29D6 中断信号 |
| PTC1 | UWB_RESET | Output | NCJ29D6 复位信号 |
| PTD0 | DEBUG_LED | Output | 调试 LED (低电平亮) |
| PTD1 | SLEEP_MODE | Output | 休眠状态指示 |
