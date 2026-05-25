/******************************************************************************
 * @file    test_hal_uwb.c
 * @brief   DW3000 UWB 驱动单元测试
 * @author  YuleTech
 * @date    2026-05-25
 *
 * @note    使用 mock SPI 总线验证驱动逻辑
 *          不依赖实际硬件，可在开发环境直接运行
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dw3000_spi_bus.h"
#include "uwb_hal.h"

/******************************************************************************
 * 测试框架宏
 ******************************************************************************/
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name)                                              \
    do {                                                        \
        printf("  TEST: %s ... ", name);                        \
        if (test_##name()) {                                    \
            printf("PASS\n");                                   \
            g_tests_passed++;                                   \
        } else {                                                \
            printf("FAIL\n");                                   \
            g_tests_failed++;                                   \
        }                                                       \
    } while (0)

#define ASSERT(cond)                                            \
    do {                                                        \
        if (!(cond)) {                                          \
            printf("\n    ASSERT FAIL: %s (line %d)\n",         \
                   #cond, __LINE__);                            \
            return 0;                                           \
        }                                                       \
    } while (0)

/******************************************************************************
 * Mock SPI 总线 — 使用静态数组模拟 DW3000 寄存器
 ******************************************************************************/
#define MOCK_SPI_MEM_SIZE 4096
static uint8_t s_mock_spi_mem[MOCK_SPI_MEM_SIZE];

static int mock_spi_init(void)
{
    memset(s_mock_spi_mem, 0, MOCK_SPI_MEM_SIZE);

    /* 预置 DW3000 设备 ID (DEV_ID = 0xDECA0300) */
    s_mock_spi_mem[0x00] = 0x00;
    s_mock_spi_mem[0x01] = 0x03;
    s_mock_spi_mem[0x02] = 0xCA;
    s_mock_spi_mem[0x03] = 0xDE;

    return 0;
}

static int mock_spi_read(uint32_t reg_addr, uint8_t *data, size_t len)
{
    if (!data || len == 0) return -1;
    size_t offset = (size_t)(reg_addr & 0xFFF);
    if (offset + len > MOCK_SPI_MEM_SIZE) return -1;
    memcpy(data, s_mock_spi_mem + offset, len);
    return 0;
}

static int mock_spi_write(uint32_t reg_addr, const uint8_t *data, size_t len)
{
    if (!data || len == 0) return -1;
    size_t offset = (size_t)(reg_addr & 0xFFF);
    if (offset + len > MOCK_SPI_MEM_SIZE) return -1;
    memcpy(s_mock_spi_mem + offset, data, len);
    return 0;
}

static int mock_spi_transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t len)
{
    if (!tx_data && !rx_data) return -1;
    if (tx_data && rx_data) {
        memcpy(rx_data, tx_data, len);
    }
    return 0;
}

static const dw3000_spi_bus_t s_mock_spi_bus = {
    .read     = mock_spi_read,
    .write    = mock_spi_write,
    .transfer = mock_spi_transfer,
};

/******************************************************************************
 * 测试用例
 ******************************************************************************/

/* === T1: SPI 总线注册 === */
static int test_spi_bus_register(void)
{
    ASSERT(dw3000_spi_bus_get() == NULL);

    dw3000_spi_bus_register(&s_mock_spi_bus);
    ASSERT(dw3000_spi_bus_get() == &s_mock_spi_bus);

    const dw3000_spi_bus_t *bus = dw3000_spi_bus_get();
    ASSERT(bus->read != NULL);
    ASSERT(bus->write != NULL);
    ASSERT(bus->transfer != NULL);

    return 1;
}

/* === T2: SPI 总线读写 === */
static int test_spi_bus_read_write(void)
{
    /* 写入已知值 */
    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    int ret = mock_spi_write(0x100, test_data, 4);
    ASSERT(ret == 0);

    /* 回读验证 */
    uint8_t read_buf[4] = {0};
    ret = mock_spi_read(0x100, read_buf, 4);
    ASSERT(ret == 0);
    ASSERT(read_buf[0] == 0xAA);
    ASSERT(read_buf[1] == 0xBB);
    ASSERT(read_buf[2] == 0xCC);
    ASSERT(read_buf[3] == 0xDD);

    return 1;
}

/* === T3: 默认 SPI 总线 === */
static int test_spi_bus_default(void)
{
    const dw3000_spi_bus_t *def = dw3000_spi_bus_default();
    ASSERT(def != NULL);
    ASSERT(def->read != NULL);
    ASSERT(def->write != NULL);
    ASSERT(def->transfer != NULL);

    /* 验证默认总线可读写 */
    uint8_t data[] = {0x01, 0x02, 0x03};
    ASSERT(def->write(0x200, data, 3) == 0);

    uint8_t read_buf[3] = {0};
    ASSERT(def->read(0x200, read_buf, 3) == 0);
    ASSERT(read_buf[0] == 0x01);
    ASSERT(read_buf[2] == 0x03);

    return 1;
}

/* === T4: SPI 参数验证 === */
static int test_spi_invalid_params(void)
{
    int ret;

    /* NULL 数据指针 */
    ret = mock_spi_read(0, NULL, 4);
    ASSERT(ret != 0);

    ret = mock_spi_write(0, NULL, 4);
    ASSERT(ret != 0);

    /* 零长度 */
    uint8_t buf[4];
    ret = mock_spi_read(0, buf, 0);
    ASSERT(ret != 0);

    return 1;
}

/******************************************************************************
 * 主测试入口
 ******************************************************************************/
int main(void)
{
    printf("\n========================================\n");
    printf("  DW3000 UWB HAL 单元测试\n");
    printf("========================================\n\n");

    mock_spi_init();

    TEST(spi_bus_register);
    TEST(spi_bus_read_write);
    TEST(spi_bus_default);
    TEST(spi_invalid_params);

    printf("\n========================================\n");
    printf("  结果: %d 通过, %d 失败, 共 %d 项\n",
           g_tests_passed, g_tests_failed,
           g_tests_passed + g_tests_failed);
    printf("========================================\n\n");

    return g_tests_failed > 0 ? 1 : 0;
}
