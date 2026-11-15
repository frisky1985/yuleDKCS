/*
 * kw47a_flash_mcux.c — KW47A 内部 Flash 的 MCUXpresso PFLASH 适配层
 *
 * 仅在定义 KW47A_MCUX_SDK 时编译（非 SDK 构建为空翻译单元）。
 * 使用 NXP PFLASH 驱动（KW45B41Z pflash 示例同款 API）：
 *   FLASH_Init(&cfg)
 *   FLASH_GetProperty(&cfg, kFLASH_PropertyPflash0SectorSize, &sz)
 *   FLASH_Erase(&cfg, addr, len, kFLASH_ApiEraseKey)
 *   FLASH_Program(&cfg, addr, (uint8_t *)src, len)
 * 若所用 SDK 版本的函数签名不同，请在此处修正并保留说明，不要臆造其他 API。
 *
 * 注意：本适配层直接 memcpy 读取 Flash 映射地址，未处理 cache / 预取缓冲。
 * 芯片启用了 Flash 缓存或 Cortex-M33 I/D cache 时，必须在擦写前后按芯片
 * 手册关闭并失效相应 cache，否则回读校验可能读到旧数据。
 */

#if defined(KW47A_MCUX_SDK)

#include "kw47a_flash_ll.h"
#include "sec_nv.h"

#include "fsl_flash.h"

#include <stdbool.h>
#include <string.h>

static flash_config_t s_cfg;
static kw47a_flash_port_t s_port;
static bool s_inited;

static int mcux_flash_erase(uint32_t addr, uint32_t len)
{
    status_t st = FLASH_Erase(&s_cfg, addr, len, kFLASH_ApiEraseKey);

    return (st == kStatus_Success) ? 0 : -1;
}

static int mcux_flash_program(uint32_t addr, const uint8_t *src, uint32_t len)
{
    status_t st;

    if (src == NULL || len == 0u) {
        return -1;
    }
    st = FLASH_Program(&s_cfg, addr, (uint8_t *)src, len);
    return (st == kStatus_Success) ? 0 : -1;
}

static int mcux_flash_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    if (dst == NULL || len == 0u) {
        return -1;
    }
    /* 直接读 Flash 映射地址；cache 处理见文件头说明。 */
    (void)memcpy(dst, (const void *)addr, len);
    return 0;
}

int kw47a_flash_mcux_bind(void)
{
    uint32_t sector_size = 0u;
    status_t st;

    if (s_inited) {
        return 0;
    }

    (void)memset(&s_cfg, 0, sizeof(s_cfg));
    st = FLASH_Init(&s_cfg);
    if (st != kStatus_Success) {
        return -1;
    }
    st = FLASH_GetProperty(&s_cfg, kFLASH_PropertyPflash0SectorSize,
                           &sector_size);
    if (st != kStatus_Success) {
        return -1;
    }

    /*
     * 几何安全门禁：要求每个记录 Bank 能被物理扇区整除，且所有 Bank
     * 起始偏移按扇区对齐，否则单扇区擦除会越过 Bank 破坏相邻记录。
     * 当前 8KB Bank 兼容 8KB 扇区；若实际器件扇区更大，必须重新规划
     * 分区（见 docs/security/nv-persistence-design.md），此处拒绝绑定
     * 并安全降级 RAM-only，绝不冒险擦除。
     */
    if (sector_size == 0u ||
        (SEC_NV_CRL_BANK_SIZE % sector_size) != 0u ||
        (SEC_NV_LOCK_BANK_SIZE % sector_size) != 0u ||
        (SEC_NV_CRL_A_OFF % sector_size) != 0u ||
        (SEC_NV_CRL_B_OFF % sector_size) != 0u ||
        (SEC_NV_LOCK_A_OFF % sector_size) != 0u ||
        (SEC_NV_LOCK_B_OFF % sector_size) != 0u) {
        return -1;
    }

    s_port.base = SEC_NV_FLASH_BASE;
    s_port.size = SEC_NV_FLASH_SIZE;
    s_port.page_size = sector_size;
    s_port.erase = mcux_flash_erase;
    s_port.program = mcux_flash_program;
    s_port.read = mcux_flash_read;
    s_port.critical_enter = NULL;
    s_port.critical_exit = NULL;

    if (kw47a_flash_ll_bind(&s_port) != 0) {
        return -1;
    }
    s_inited = true;
    return 0;
}

#else

/* 非 SDK 构建：保持翻译单元非空，避免 -Wpedantic 空文件告警。 */
typedef int kw47a_flash_mcux_not_compiled_t;

#endif /* KW47A_MCUX_SDK */
