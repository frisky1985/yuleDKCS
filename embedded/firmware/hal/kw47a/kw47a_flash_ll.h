/*
 * kw47a_flash_ll.h — KW47A 内部 Flash 页擦/编程的低层驱动（平台无关）
 *
 * 本层只做参数/对齐/范围检查、擦除后空白校验与编程回读校验，
 * 具体硬件操作通过 kw47a_flash_port_t 注入（MCUX 适配层或测试假 Flash）。
 * 地址均为绝对地址（Flash 映射地址），不是分区偏移。
 */

#ifndef KW47A_FLASH_LL_H
#define KW47A_FLASH_LL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t base;                       /* 区域起始绝对地址 */
    uint32_t size;                       /* 区域字节数, 必须为 page_size 整数倍 */
    uint32_t page_size;                  /* 页大小, 必须 2 的幂且 >= 4 */
    int (*erase)(uint32_t addr, uint32_t len);
    int (*program)(uint32_t addr, const uint8_t *src, uint32_t len);
    int (*read)(uint32_t addr, uint8_t *dst, uint32_t len);
    void (*critical_enter)(void);        /* 可 NULL */
    void (*critical_exit)(void);         /* 可 NULL */
} kw47a_flash_port_t;

/*
 * 绑定端口（结构体按值复制）。
 * 返回 0 成功；port 为 NULL、ops 缺失或参数非法时返回 -1（保持原有绑定不变）。
 */
int kw47a_flash_ll_bind(const kw47a_flash_port_t *port);

/* 解除绑定（幂等）。 */
void kw47a_flash_ll_unbind(void);

/*
 * 擦除单页：addr 必须页对齐且整页在区域内。
 * 返回 0 成功，-1 失败（未绑定 / 越界 / 未对齐 / 硬件失败）。
 */
int kw47a_flash_ll_erase(uint32_t addr);

/*
 * 编程：addr 4 字节对齐、len > 0 且 4 字节对齐、不得跨页、不得越界。
 * 编程前要求目标区为全 0xFF（未擦除返回 -1）；编程后逐字节回读比对。
 * 擦写期间用 critical_enter/exit 包裹（若提供）。
 */
int kw47a_flash_ll_program(uint32_t addr, const uint8_t *src, uint32_t len);

/* 读取任意长度（len > 0），范围必须在区域内。 */
int kw47a_flash_ll_read(uint32_t addr, uint8_t *dst, uint32_t len);

/* 已绑定返回 page_size，未绑定返回 0。 */
uint32_t kw47a_flash_ll_page_size(void);

#ifdef __cplusplus
}
#endif

#endif /* KW47A_FLASH_LL_H */
