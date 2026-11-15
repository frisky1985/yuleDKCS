/**
 * @file sec_nv.c
 * @brief CRL / 产线锁定 NV 持久化 — 双 Bank + seq + CRC + commit-last
 *
 * 所有多字节字段显式小端读写, 从不把 Flash 内容映射为结构体。
 * 每次操作重新扫描两个 Bank, 不缓存记录。
 */

#include "sec_nv.h"

#include <stddef.h>
#include <string.h>

_Static_assert(SEC_NV_FLASH_BASE + SEC_NV_FLASH_SIZE <= 0x00080000u,
               "NV partition exceeds 512KB flash");
_Static_assert(SEC_NV_CRL_A_OFF + 2u * SEC_NV_CRL_BANK_SIZE <= SEC_NV_FLASH_SIZE,
               "CRL banks exceed NV partition");
_Static_assert(SEC_NV_LOCK_A_OFF + 2u * SEC_NV_LOCK_BANK_SIZE <= SEC_NV_FLASH_SIZE,
               "Lock banks exceed NV partition");
_Static_assert(SEC_NV_HDR_SIZE + SEC_NV_MAX_CRL + SEC_NV_CRL_META_SIZE <=
                   SEC_NV_CRL_BANK_SIZE,
               "max CRL record does not fit a bank");

typedef struct {
    bool     present;      /* Bank 非全 0xFF */
    bool     valid;        /* 魔数/类型/版本/commit/CRC 全部通过 */
    uint32_t seq;
    uint16_t payload_len;
} sec_nv_bank_t;

static sec_nv_ops_t g_ops;
static bool         g_bound;

/* 单 Bank 扫描/回读临时缓冲 (非记录缓存) */
static uint8_t g_scratch[SEC_NV_CRL_BANK_SIZE];

/* ── 小端读写 ─────────────────────────────────────────────── */

static uint16_t nv_rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t nv_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void nv_wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void nv_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

/* CRC-32 (多项式 0xEDB88320), 覆盖两段连续数据。 */
static uint32_t nv_crc32(const uint8_t *a, uint16_t a_len,
                         const uint8_t *b, uint16_t b_len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint16_t i;
    int b_bit;

    for (i = 0; i < a_len; i++) {
        crc ^= (uint32_t)a[i];
        for (b_bit = 0; b_bit < 8; b_bit++) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    for (i = 0; i < b_len; i++) {
        crc ^= (uint32_t)b[i];
        for (b_bit = 0; b_bit < 8; b_bit++) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return ~crc;
}

static bool nv_all_ff(const uint8_t *p, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        if (p[i] != 0xFFU) {
            return false;
        }
    }
    return true;
}

static bool nv_bound(void)
{
    return g_bound && g_ops.read != NULL && g_ops.write != NULL && g_ops.erase != NULL;
}

/* ── Bank 扫描 ────────────────────────────────────────────── */

/**
 * 读取整个 Bank 并判定: 空白 / 有记录且有效 / 非空白但无效(损坏)。
 * 返回 CCC_ERR_HARDWARE 仅表示底层读失败。
 */
static ccc_status_t nv_probe(uint32_t off, uint16_t bank_size, uint8_t type,
                             uint16_t max_payload, sec_nv_bank_t *info)
{
    uint16_t payload_len;
    uint32_t crc;

    info->present = false;
    info->valid = false;
    info->seq = 0;
    info->payload_len = 0;

    if (g_ops.read(off, g_scratch, bank_size) != 0) {
        return CCC_ERR_HARDWARE;
    }
    if (nv_all_ff(g_scratch, bank_size)) {
        return CCC_OK;
    }
    info->present = true;

    if (nv_rd32(&g_scratch[0]) != SEC_NV_RECORD_MAGIC) {
        return CCC_OK;                       /* 非空白但无记录 → 损坏 */
    }
    if (g_scratch[4] != type || g_scratch[5] != SEC_NV_VERSION) {
        return CCC_OK;
    }
    payload_len = nv_rd16(&g_scratch[12]);
    if (payload_len > max_payload ||
        (uint32_t)SEC_NV_HDR_SIZE + (uint32_t)payload_len > bank_size) {
        return CCC_OK;
    }
    if (nv_rd32(&g_scratch[20]) != SEC_NV_COMMIT_MAGIC) {
        return CCC_OK;                       /* commit 未写 → 半写记录 */
    }

    crc = nv_rd32(&g_scratch[16]);
    if (crc != nv_crc32(&g_scratch[0], 16, &g_scratch[SEC_NV_HDR_SIZE],
                        payload_len)) {
        return CCC_OK;
    }

    info->valid = true;
    info->seq = nv_rd32(&g_scratch[8]);
    info->payload_len = payload_len;
    return CCC_OK;
}

/**
 * 扫描双 Bank 并选出 seq 最大的有效记录。
 * @retval CCC_OK        找到有效记录, *off 为 Bank 起始偏移
 * @retval CCC_ERR_NOT_FOUND 两 Bank 均空白
 * @retval CCC_ERR_SECURITY  无有效记录但存在损坏 Bank (fail closed)
 */
static ccc_status_t nv_select(uint32_t off_a, uint32_t off_b, uint16_t bank_size,
                              uint8_t type, uint16_t max_payload,
                              uint32_t *off, uint16_t *payload_len)
{
    sec_nv_bank_t a;
    sec_nv_bank_t b;
    ccc_status_t st;

    st = nv_probe(off_a, bank_size, type, max_payload, &a);
    if (st != CCC_OK) {
        return st;
    }
    st = nv_probe(off_b, bank_size, type, max_payload, &b);
    if (st != CCC_OK) {
        return st;
    }

    if (b.valid && (!a.valid || b.seq > a.seq)) {
        *off = off_b;
        *payload_len = b.payload_len;
        return CCC_OK;
    }
    if (a.valid) {
        *off = off_a;
        *payload_len = a.payload_len;
        return CCC_OK;
    }
    if (a.present || b.present) {
        return CCC_ERR_SECURITY;
    }
    return CCC_ERR_NOT_FOUND;
}

/**
 * 为一次保存选择目标 (非活动) Bank, 并计算新 seq = 当前最大有效 seq + 1。
 */
static ccc_status_t nv_pick_save_bank(uint32_t off_a, uint32_t off_b,
                                      uint16_t bank_size, uint8_t type,
                                      uint16_t max_payload,
                                      uint32_t *off, uint32_t *seq)
{
    sec_nv_bank_t a;
    sec_nv_bank_t b;
    ccc_status_t st;

    st = nv_probe(off_a, bank_size, type, max_payload, &a);
    if (st != CCC_OK) {
        return st;
    }
    st = nv_probe(off_b, bank_size, type, max_payload, &b);
    if (st != CCC_OK) {
        return st;
    }

    if (a.valid && b.valid) {
        if (b.seq > a.seq) {
            *off = off_a;                    /* 活动 Bank = B */
            *seq = b.seq + 1u;
        } else {
            *off = off_b;                    /* 活动 Bank = A */
            *seq = a.seq + 1u;
        }
    } else if (a.valid) {
        *off = off_b;
        *seq = a.seq + 1u;
    } else if (b.valid) {
        *off = off_a;
        *seq = b.seq + 1u;
    } else {
        *off = off_a;
        *seq = 0u;
    }
    return CCC_OK;
}

/* ── 记录写入 (提交协议) ──────────────────────────────────── */

static ccc_status_t nv_write_record(uint32_t off, uint16_t bank_size, uint8_t type,
                                    uint32_t seq, const uint8_t *payload,
                                    uint16_t payload_len)
{
    uint8_t hdr[SEC_NV_HDR_SIZE];
    uint8_t hdr_rb[SEC_NV_HDR_SIZE];
    uint32_t crc;

    if ((uint32_t)SEC_NV_HDR_SIZE + (uint32_t)payload_len > bank_size) {
        return CCC_ERR_PARAM;
    }

    /* 1. 擦除非活动 Bank */
    if (g_ops.erase(off, bank_size) != 0) {
        return CCC_ERR_HARDWARE;
    }

    /* 2. 组头: commit 先保持 0xFFFFFFFF */
    (void)memset(hdr, 0, sizeof(hdr));
    nv_wr32(&hdr[0], SEC_NV_RECORD_MAGIC);
    hdr[4] = type;
    hdr[5] = SEC_NV_VERSION;
    nv_wr16(&hdr[6], 0);                    /* flags */
    nv_wr32(&hdr[8], seq);
    nv_wr16(&hdr[12], payload_len);
    nv_wr16(&hdr[14], 0);                   /* reserved */
    crc = nv_crc32(&hdr[0], 16, payload, payload_len);
    nv_wr32(&hdr[16], crc);
    nv_wr32(&hdr[20], 0xFFFFFFFFu);         /* commit 未提交 */
    nv_wr32(&hdr[24], 0);
    nv_wr32(&hdr[28], 0);

    /* 3. 写头 + payload */
    if (g_ops.write(off, hdr, (uint16_t)SEC_NV_HDR_SIZE) != 0) {
        return CCC_ERR_HARDWARE;
    }
    if (payload_len > 0U && g_ops.write(off + SEC_NV_HDR_SIZE, payload,
                                        payload_len) != 0) {
        return CCC_ERR_HARDWARE;
    }

    /* 4. 回读校验 */
    if (g_ops.read(off, hdr_rb, (uint16_t)SEC_NV_HDR_SIZE) != 0 ||
        g_ops.read(off + SEC_NV_HDR_SIZE, g_scratch, payload_len) != 0) {
        return CCC_ERR_HARDWARE;
    }
    if (memcmp(hdr_rb, hdr, SEC_NV_HDR_SIZE) != 0 ||
        memcmp(g_scratch, payload, payload_len) != 0) {
        return CCC_ERR_SECURITY;
    }

    /* 5. 最后单独写 commit */
    nv_wr32(&hdr[20], SEC_NV_COMMIT_MAGIC);
    if (g_ops.write(off + 20u, &hdr[20], 4u) != 0) {
        return CCC_ERR_HARDWARE;
    }

    /* 6. 再次回读并完整校验 */
    if (g_ops.read(off, hdr_rb, (uint16_t)SEC_NV_HDR_SIZE) != 0 ||
        g_ops.read(off + SEC_NV_HDR_SIZE, g_scratch, payload_len) != 0) {
        return CCC_ERR_HARDWARE;
    }
    if (nv_rd32(&hdr_rb[0]) != SEC_NV_RECORD_MAGIC ||
        hdr_rb[4] != type || hdr_rb[5] != SEC_NV_VERSION ||
        nv_rd16(&hdr_rb[12]) != payload_len ||
        nv_rd32(&hdr_rb[20]) != SEC_NV_COMMIT_MAGIC ||
        nv_rd32(&hdr_rb[16]) != nv_crc32(&hdr_rb[0], 16, g_scratch, payload_len)) {
        return CCC_ERR_SECURITY;
    }
    return CCC_OK;
}

/* ── 公共接口 ─────────────────────────────────────────────── */

ccc_status_t sec_nv_bind(const sec_nv_ops_t *ops)
{
    if (ops == NULL || ops->read == NULL || ops->write == NULL ||
        ops->erase == NULL) {
        return CCC_ERR_PARAM;
    }
    g_ops = *ops;
    g_bound = true;
    return CCC_OK;
}

ccc_status_t sec_nv_init(void)
{
    if (!nv_bound()) {
        return CCC_ERR_NOT_INIT;
    }
    if (SEC_NV_CRL_B_OFF + SEC_NV_CRL_BANK_SIZE > SEC_NV_FLASH_SIZE ||
        SEC_NV_LOCK_B_OFF + SEC_NV_LOCK_BANK_SIZE > SEC_NV_FLASH_SIZE) {
        return CCC_ERR_PARAM;
    }
    return CCC_OK;
}

void sec_nv_reset_for_test(void)
{
    (void)memset(&g_ops, 0, sizeof(g_ops));
    (void)memset(g_scratch, 0xFF, sizeof(g_scratch));
    g_bound = false;
}

ccc_status_t sec_nv_save_crl(const uint8_t *blob, uint16_t len)
{
    uint8_t  payload[SEC_NV_MAX_CRL + SEC_NV_CRL_META_SIZE];
    uint32_t off;
    uint32_t seq;
    uint32_t epoch;
    ccc_status_t st;

    if (!nv_bound()) {
        return CCC_ERR_NOT_INIT;
    }
    if (blob == NULL || len < 12u || len > (uint16_t)SEC_NV_MAX_CRL) {
        return CCC_ERR_PARAM;
    }

    (void)memcpy(payload, blob, len);
    /* 8B 元数据: installed_at(4) + source(1) + reserved(3) */
    (void)memset(&payload[len], 0, SEC_NV_CRL_META_SIZE);

    st = nv_pick_save_bank(SEC_NV_CRL_A_OFF, SEC_NV_CRL_B_OFF,
                           (uint16_t)SEC_NV_CRL_BANK_SIZE, SEC_NV_TYPE_CRL,
                           (uint16_t)(SEC_NV_MAX_CRL + SEC_NV_CRL_META_SIZE),
                           &off, &seq);
    if (st != CCC_OK) {
        return st;
    }

    st = nv_write_record(off, (uint16_t)SEC_NV_CRL_BANK_SIZE, SEC_NV_TYPE_CRL,
                         seq, payload, (uint16_t)(len + SEC_NV_CRL_META_SIZE));
    if (st != CCC_OK) {
        return st;
    }

    /* Flash 提交成功后才抬高 OTP 水位 (只增不减) */
    if (g_ops.otp_epoch_raise != NULL) {
        epoch = nv_rd32(&blob[8]);
        if (g_ops.otp_epoch_raise(epoch) != 0) {
            return CCC_ERR_HARDWARE;
        }
    }
    return CCC_OK;
}

ccc_status_t sec_nv_load_crl(uint8_t *blob, uint16_t cap, uint16_t *len)
{
    uint32_t off = 0;
    uint16_t payload_len = 0;
    uint16_t crl_len;
    uint32_t watermark = 0;
    ccc_status_t st;

    if (!nv_bound()) {
        return CCC_ERR_NOT_INIT;
    }
    if (blob == NULL || len == NULL || cap == 0U) {
        return CCC_ERR_PARAM;
    }

    st = nv_select(SEC_NV_CRL_A_OFF, SEC_NV_CRL_B_OFF,
                   (uint16_t)SEC_NV_CRL_BANK_SIZE, SEC_NV_TYPE_CRL,
                   (uint16_t)(SEC_NV_MAX_CRL + SEC_NV_CRL_META_SIZE),
                   &off, &payload_len);
    if (st != CCC_OK) {
        return st;
    }
    if (payload_len < SEC_NV_CRL_META_SIZE) {
        return CCC_ERR_SECURITY;
    }
    crl_len = (uint16_t)(payload_len - SEC_NV_CRL_META_SIZE);
    if (crl_len > cap) {
        return CCC_ERR_PARAM;
    }
    if (g_ops.read(off + SEC_NV_HDR_SIZE, g_scratch, payload_len) != 0) {
        return CCC_ERR_HARDWARE;
    }

    /* 防回滚: CRC 通过后再比较 OTP 水位 */
    if (g_ops.otp_epoch_read != NULL &&
        g_ops.otp_epoch_read(&watermark) == 0 &&
        nv_rd32(&g_scratch[8]) < watermark) {
        return CCC_ERR_STATE;
    }

    (void)memcpy(blob, g_scratch, crl_len);
    *len = crl_len;
    return CCC_OK;
}

ccc_status_t sec_nv_save_lock(const uint8_t station_id[16])
{
    uint8_t  payload[SEC_NV_LOCK_PAYLOAD];
    uint32_t off;
    uint32_t seq;
    ccc_status_t st;

    if (!nv_bound()) {
        return CCC_ERR_NOT_INIT;
    }

    st = nv_pick_save_bank(SEC_NV_LOCK_A_OFF, SEC_NV_LOCK_B_OFF,
                           (uint16_t)SEC_NV_LOCK_BANK_SIZE, SEC_NV_TYPE_LOCK,
                           (uint16_t)SEC_NV_LOCK_PAYLOAD, &off, &seq);
    if (st != CCC_OK) {
        return st;
    }

    (void)memset(payload, 0, sizeof(payload));
    payload[0] = 1u;                        /* state = locked */
    payload[1] = 0u;                        /* reason = EOL */
    nv_wr32(&payload[4], seq);              /* lock_epoch */
    if (station_id != NULL) {
        (void)memcpy(&payload[8], station_id, 16);
    }

    st = nv_write_record(off, (uint16_t)SEC_NV_LOCK_BANK_SIZE, SEC_NV_TYPE_LOCK,
                         seq, payload, (uint16_t)SEC_NV_LOCK_PAYLOAD);
    if (st != CCC_OK) {
        return st;
    }

    if (g_ops.otp_lock_set != NULL && g_ops.otp_lock_set() != 0) {
        return CCC_ERR_HARDWARE;            /* Flash 锁定仍生效 */
    }
    return CCC_OK;
}

ccc_status_t sec_nv_load_lock(bool *locked)
{
    uint32_t off = 0;
    uint16_t payload_len = 0;
    bool otp_locked;
    ccc_status_t st;

    if (!nv_bound()) {
        return CCC_ERR_NOT_INIT;
    }
    if (locked == NULL) {
        return CCC_ERR_PARAM;
    }

    otp_locked = (g_ops.otp_lock_read != NULL) ? g_ops.otp_lock_read() : false;

    st = nv_select(SEC_NV_LOCK_A_OFF, SEC_NV_LOCK_B_OFF,
                   (uint16_t)SEC_NV_LOCK_BANK_SIZE, SEC_NV_TYPE_LOCK,
                   (uint16_t)SEC_NV_LOCK_PAYLOAD, &off, &payload_len);
    if (st == CCC_OK) {
        if (payload_len != SEC_NV_LOCK_PAYLOAD ||
            g_ops.read(off + SEC_NV_HDR_SIZE, g_scratch, payload_len) != 0) {
            *locked = true;                 /* fail closed */
            return CCC_ERR_SECURITY;
        }
        *locked = (g_scratch[0] == 1u) || otp_locked;
        return CCC_OK;
    }
    if (st == CCC_ERR_NOT_FOUND) {
        *locked = otp_locked;
        return otp_locked ? CCC_OK : CCC_ERR_NOT_FOUND;
    }

    /* 损坏 (非空白) → fail closed */
    *locked = true;
    return CCC_ERR_SECURITY;
}
