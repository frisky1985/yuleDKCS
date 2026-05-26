# DW3000 SPI HAL 接口设计

> 文档版本: v1.0
> 日期: 2026-05-25

## 设计目标

将 DW3000 驱动中的 SPI 操作抽象化，实现**平台无关**的驱动层。

## 当前问题

```
dw3000_read_reg() → 构建SPI头部 → return 0 (空操作)
dw3000_write_reg() → 构建SPI头部 → return 0 (空操作)
```

SPI 总线调用被注释为"平台特定"，导致驱动无法实际工作。

## 设计方案

### 架构

```
┌──────────────────────────────────────────────────┐
│  UWB Application (测距、定位)                      │
├──────────────────────────────────────────────────┤
│  dw3000_driver.c (平台无关逻辑)                   │
│  - 寄存器读写 → 通过 SPI 总线接口                  │
│  - 配置/测距/STS → 纯逻辑                         │
├──────────────────────────────────────────────────┤
│  dw3000_spi_bus.h (抽象接口)                      │
│  - read(uint32_t addr, data, len)                │
│  - write(uint32_t addr, data, len)                │
│  - transfer(tx, rx, len)                         │
├──────────────────────────────────────────────────┤
│  具体实现 (平台相关)                               │
│  - NXP SDK: fsl_dspi.c → SPI_MasterTransfer()    │
│  - STM32: HAL_SPI_TransmitReceive()               │
│  - 测试: 模拟SPI (mock)                           │
└──────────────────────────────────────────────────┘
```

### 接口定义

```c
// dw3000_spi_bus.h

typedef struct {
    /**
     * @brief 从 DW3000 寄存器读取数据
     * @param reg_addr  寄存器地址
     * @param data      输出缓冲区
     * @param len       读取长度
     * @return 0 成功, -1 失败
     */
    int (*read)(uint32_t reg_addr, uint8_t *data, size_t len);

    /**
     * @brief 向 DW3000 寄存器写入数据
     * @param reg_addr  寄存器地址
     * @param data      输入数据
     * @param len       数据长度
     * @return 0 成功, -1 失败
     */
    int (*write)(uint32_t reg_addr, const uint8_t *data, size_t len);

    /**
     * @brief SPI 全双工传输 (用于批量操作)
     * @param tx_data   发送数据 (可为 NULL)
     * @param rx_data   接收数据 (可为 NULL)
     * @param len       传输长度
     * @return 0 成功, -1 失败
     */
    int (*transfer)(const uint8_t *tx_data, uint8_t *rx_data, size_t len);
} dw3000_spi_bus_t;

/** 注册 SPI 总线 (必须在 dw3000_init 前调用) */
void dw3000_spi_bus_register(const dw3000_spi_bus_t *bus);
```

### 默认实现 (软件回退)

提供一组默认的软件模拟实现，用于开发测试环境：

```c
// dw3000_spi_bus_default.c
// 使用 mbedtls 内存回调模拟 SPI 响应
// 仅用于验证驱动逻辑正确性
```

## 测试方案

### 单元测试

```c
// test_hal_uwb.c
// 使用 mock_spi_bus 替代真实 SPI

static int mock_regs[256];  // 模拟寄存器空间
static int mock_read(...) { /* 从 mock_regs 读取 */ }
static int mock_write(...) { /* 写入 mock_regs */ }

void test_dw3000_init() {
    dw3000_spi_bus_register(&mock_bus);
    // 验证初始化序列
    // 验证 DEV_ID 读取
}

void test_dw3000_configure_sts() {
    // 验证 STS 配置寄存器写入
}
```

## 实施步骤

1. **创建 `dw3000_spi_bus.h`** — SPI 总线接口声明
2. **创建 `dw3000_spi_bus_default.c`** — 软件回退实现
3. **修改 `dw3000_driver.c`** — 将空 return 0 替换为 SPI 总线调用
4. **创建 `test_hal_uwb.c`** — 单元测试 (mock SPI)
5. **更新头文件包含** — 确保所有依赖正确
