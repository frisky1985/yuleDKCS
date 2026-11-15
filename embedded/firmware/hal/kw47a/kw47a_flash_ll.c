/*
 * kw47a_flash_ll.c — KW47A 内部 Flash 页擦/编程低层驱动（平台无关）
 *
 * 规则：
 *   - 只擦单页，addr 页对齐；
 *   - 编程前目标区必须为 0xFF（未擦除拒绝），编程后逐字节回读；
 *   - 擦/编程由 critical_enter/exit 包裹（若提供）；
 *   - 未绑定、越界、空指针、未对齐一律 -1。
 */

#include "kw47a_flash_ll.h"

#include <stdbool.h>
#include <string.h>

#define KW47A_FLASH_CHUNK   32u

static kw47a_flash_port_t s_port;
static bool s_bound;

static bool flash_config_valid(const kw47a_flash_port_t *p)
{
    if (p == NULL || p->erase == NULL || p->program == NULL || p->read == NULL) {
        return false;
    }
    /* page_size: 2 的幂且 >= 4（因而也是 4 的倍数） */
    if (p->page_size < 4u ||
        (p->page_size & (p->page_size - 1u)) != 0u) {
        return false;
    }
    if (p->size < p->page_size || (p->size % p->page_size) != 0u) {
        return false;
    }
    if (p->size > (UINT32_MAX - p->base)) {
        return false;
    }
    return true;
}

/* addr + len 是否完整落在绑定区域内（len 由调用方保证 > 0）。 */
static bool flash_range_ok(uint32_t addr, uint32_t len)
{
    uint32_t off;

    if (!s_bound) {
        return false;
    }
    if (addr < s_port.base) {
        return false;
    }
    off = addr - s_port.base;
    if (off >= s_port.size || len > (s_port.size - off)) {
        return false;
    }
    return true;
}

static void flash_enter(void)
{
    if (s_port.critical_enter != NULL) {
        s_port.critical_enter();
    }
}

static void flash_exit(void)
{
    if (s_port.critical_exit != NULL) {
        s_port.critical_exit();
    }
}

/* 目标区必须全为 0xFF（已擦除）。 */
static int flash_blank_check(uint32_t addr, uint32_t len)
{
    uint8_t buf[KW47A_FLASH_CHUNK];
    uint32_t done = 0;

    while (done < len) {
        uint32_t n = len - done;
        uint32_t i;

        if (n > sizeof(buf)) {
            n = sizeof(buf);
        }
        if (s_port.read(addr + done, buf, n) != 0) {
            return -1;
        }
        for (i = 0; i < n; i++) {
            if (buf[i] != 0xFFu) {
                return -1;
            }
        }
        done += n;
    }
    return 0;
}

/* 编程后回读逐字节比对。 */
static int flash_verify(uint32_t addr, const uint8_t *src, uint32_t len)
{
    uint8_t buf[KW47A_FLASH_CHUNK];
    uint32_t done = 0;

    while (done < len) {
        uint32_t n = len - done;

        if (n > sizeof(buf)) {
            n = sizeof(buf);
        }
        if (s_port.read(addr + done, buf, n) != 0) {
            return -1;
        }
        if (memcmp(buf, src + done, n) != 0) {
            return -1;
        }
        done += n;
    }
    return 0;
}

int kw47a_flash_ll_bind(const kw47a_flash_port_t *port)
{
    if (!flash_config_valid(port)) {
        return -1;
    }
    s_port = *port;
    s_bound = true;
    return 0;
}

void kw47a_flash_ll_unbind(void)
{
    (void)memset(&s_port, 0, sizeof(s_port));
    s_bound = false;
}

int kw47a_flash_ll_erase(uint32_t addr)
{
    uint32_t off;
    int rc;

    if (!flash_range_ok(addr, 1u)) {
        return -1;
    }
    off = addr - s_port.base;
    if ((off % s_port.page_size) != 0u) {
        return -1;
    }
    if (s_port.page_size > (s_port.size - off)) {
        return -1;
    }

    flash_enter();
    rc = s_port.erase(addr, s_port.page_size);
    flash_exit();

    return (rc == 0) ? 0 : -1;
}

int kw47a_flash_ll_program(uint32_t addr, const uint8_t *src, uint32_t len)
{
    uint32_t off;
    int rc;

    if (src == NULL || len == 0u || (len % 4u) != 0u) {
        return -1;
    }
    if ((addr % 4u) != 0u) {
        return -1;
    }
    if (!flash_range_ok(addr, len)) {
        return -1;
    }
    off = addr - s_port.base;
    /* 不得跨页 */
    if ((off / s_port.page_size) != ((off + len - 1u) / s_port.page_size)) {
        return -1;
    }
    if (flash_blank_check(addr, len) != 0) {
        return -1;
    }

    flash_enter();
    rc = s_port.program(addr, src, len);
    flash_exit();

    if (rc != 0) {
        return -1;
    }
    return flash_verify(addr, src, len);
}

int kw47a_flash_ll_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    if (dst == NULL || len == 0u) {
        return -1;
    }
    if (!flash_range_ok(addr, len)) {
        return -1;
    }
    return (s_port.read(addr, dst, len) == 0) ? 0 : -1;
}

uint32_t kw47a_flash_ll_page_size(void)
{
    return s_bound ? s_port.page_size : 0u;
}
