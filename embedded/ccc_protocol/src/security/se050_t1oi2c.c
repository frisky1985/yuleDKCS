/**
 * @file se050_t1oi2c.c
 * @brief SE050 T=1' over I2C transport implementation (NXP AN12413).
 */

#include "se050_t1oi2c.h"

#include <string.h>

/* Platform I2C primitive (provided by BSP/HAL). */
extern int32_t i2c_transfer(uint8_t dev, uint8_t addr,
                            const uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len);

/* ── Default platform I/O ────────────────────────────────── */
static int default_write(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    return (i2c_transfer(addr, addr, buf, len, NULL, 0) == 0) ? 0 : -1;
}

static int default_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    return (i2c_transfer(addr, (uint8_t)(addr | 0x01U), NULL, 0, buf, len) == 0)
               ? 0 : -1;
}

static const se050_t1_io_t g_default_io = { default_write, default_read };
static const se050_t1_io_t *g_io = &g_default_io;

void se050_t1_set_io(const se050_t1_io_t *io)
{
    g_io = (io != NULL) ? io : &g_default_io;
}

/* ── CRC16 (ISO/IEC 3309 / XMODEM style) ─────────────────── */
uint16_t se050_t1_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  b;

    if (data == NULL) return crc;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (b = 0; b < 8U; b++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ── Frame encode / decode ───────────────────────────────── */
int se050_t1_encode(uint8_t nad, uint8_t pcb,
                    const uint8_t *inf, uint16_t len,
                    uint8_t *out, uint16_t out_size)
{
    uint16_t crc;

    if (out == NULL) return SE050_T1_ERR_BUFFER;
    if (len > SE050_T1_MAX_INF) return SE050_T1_ERR_FRAME;
    if (out_size < (uint16_t)(len + 5U)) return SE050_T1_ERR_BUFFER;
    if (len > 0U && inf == NULL) return SE050_T1_ERR_FRAME;

    out[0] = nad;
    out[1] = pcb;
    out[2] = (uint8_t)len;            /* 0 means 256; we never emit 256 */
    if (len > 0U) {
        memcpy(&out[3], inf, len);
    }

    crc = se050_t1_crc16(out, (uint16_t)(len + 3U));
    out[3U + len]      = (uint8_t)(crc & 0xFFU);
    out[3U + len + 1U] = (uint8_t)(crc >> 8);

    return (int)(len + 5U);
}

/* LEN == 0 means 256 bytes only for I-blocks; R/S-blocks carry no INF. */
static uint16_t t1_inf_len(uint8_t pcb, uint8_t len_byte)
{
    if (len_byte == 0U) {
        return ((pcb & 0x80U) != 0U) ? 0U : 256U;
    }
    return (uint16_t)len_byte;
}

int se050_t1_decode(const uint8_t *frame, uint16_t frame_len,
                    uint8_t *pcb,
                    uint8_t *inf, uint16_t inf_size, uint16_t *inf_len)
{
    uint16_t len;
    uint16_t crc_rx;
    uint16_t crc_calc;

    if (frame == NULL || frame_len < 5U) return SE050_T1_ERR_FRAME;
    if (frame[0] != SE050_T1_NAD_SE)     return SE050_T1_ERR_FRAME;

    if (pcb != NULL) *pcb = frame[1];

    len = t1_inf_len(frame[1], frame[2]);
    if (frame_len < (uint16_t)(len + 5U)) return SE050_T1_ERR_FRAME;
    if (inf != NULL && len > inf_size)    return SE050_T1_ERR_BUFFER;

    crc_rx   = (uint16_t)frame[3U + len] | ((uint16_t)frame[4U + len] << 8);
    crc_calc = se050_t1_crc16(frame, (uint16_t)(len + 3U));
    if (crc_rx != crc_calc) return SE050_T1_ERR_CRC;

    if (inf != NULL && len > 0U) memcpy(inf, &frame[3], len);
    if (inf_len != NULL) *inf_len = len;

    return SE050_T1_OK;
}

/* ── Low-level frame I/O ─────────────────────────────────── */
static int t1_write_frame(const uint8_t *frame, uint16_t len, uint8_t addr)
{
    return (g_io->write(addr, frame, len) == 0) ? SE050_T1_OK : SE050_T1_ERR_IO;
}

static int t1_read_frame(uint8_t addr,
                         uint8_t *pcb,
                         uint8_t *inf, uint16_t inf_size, uint16_t *inf_len)
{
    uint8_t  header[3];
    uint8_t  frame[SE050_T1_MAX_FRAME];
    uint16_t len;

    if (g_io->read(addr, header, 3U) != 0) return SE050_T1_ERR_IO;

    len = t1_inf_len(header[1], header[2]);
    if (len > SE050_T1_MAX_INF) return SE050_T1_ERR_FRAME;

    memcpy(frame, header, 3U);
    if (g_io->read(addr, &frame[3], (uint16_t)(len + 2U)) != 0) {
        return SE050_T1_ERR_IO;
    }

    return se050_t1_decode(frame, (uint16_t)(len + 5U), pcb, inf, inf_size, inf_len);
}

static int t1_send_simple(uint8_t addr, uint8_t pcb)
{
    uint8_t frame[5];
    int n = se050_t1_encode(SE050_T1_NAD_HOST, pcb, NULL, 0, frame, sizeof(frame));
    if (n < 0) return n;
    return t1_write_frame(frame, (uint16_t)n, addr);
}

/* ── APDU transceive with block chaining ─────────────────── */
#define T1_MAX_RETRY 3U

int se050_t1_transceive(uint8_t addr,
                        const uint8_t *apdu, uint16_t apdu_len,
                        uint8_t *resp, uint16_t resp_size, uint16_t *resp_len)
{
    uint8_t  resp_buf[512];
    uint16_t resp_used = 0;
    uint16_t offset = 0;
    uint8_t  host_bn = 0;
    uint8_t  card_bn = 0;
    uint8_t  retries = 0;
    uint8_t  last_frame[SE050_T1_MAX_FRAME];
    uint16_t last_len = 0;

    if (apdu == NULL || apdu_len == 0U) return SE050_T1_ERR_FRAME;

    /* ---- Send the command in I-blocks ---- */
    while (offset < apdu_len) {
        uint16_t chunk = (uint16_t)(apdu_len - offset);
        uint8_t  pcb;

        if (chunk > SE050_T1_MAX_INF) chunk = SE050_T1_MAX_INF;

        pcb = host_bn;
        if ((uint16_t)(offset + chunk) < apdu_len) {
            pcb |= SE050_T1_PCB_CHAINING;
        }

        int enc = se050_t1_encode(SE050_T1_NAD_HOST, pcb,
                                  &apdu[offset], chunk,
                                  last_frame, sizeof(last_frame));
        if (enc < 0) return SE050_T1_ERR_FRAME;
        last_len = (uint16_t)enc;

        if (t1_write_frame(last_frame, last_len, addr) != SE050_T1_OK) {
            return SE050_T1_ERR_IO;
        }
        offset = (uint16_t)(offset + chunk);

        /* A chained block must be acknowledged by the card. */
        if (offset < apdu_len) {
            uint8_t  r_pcb = 0;
            uint8_t  dummy[1];
            uint16_t dummy_len = 0;
            int      rc;

            do {
                rc = t1_read_frame(addr, &r_pcb, dummy, sizeof(dummy), &dummy_len);
                if (rc == SE050_T1_ERR_CRC && retries < T1_MAX_RETRY) {
                    /* Corrupted ACK: retransmit the same I-block. */
                    retries++;
                    if (t1_write_frame(last_frame, last_len, addr) != SE050_T1_OK) {
                        return SE050_T1_ERR_IO;
                    }
                    rc = t1_read_frame(addr, &r_pcb, dummy, sizeof(dummy), &dummy_len);
                }
                if (rc != SE050_T1_OK) return rc;

                if ((r_pcb & SE050_T1_PCB_S_BLOCK) == SE050_T1_PCB_S_BLOCK) {
                    /* S(WTX request) → respond with S(WTX response) */
                    if (r_pcb == SE050_T1_PCB_S_WTX_REQ) {
                        (void)t1_send_simple(addr, SE050_T1_PCB_S_WTX_RSP);
                        retries++;
                        if (retries > T1_MAX_RETRY * 4U) return SE050_T1_ERR_PROTOCOL;
                        continue;
                    }
                    continue;
                }
                /* Expect an R-block ACK for the chained block. */
                if ((r_pcb & SE050_T1_PCB_R_BLOCK) != SE050_T1_PCB_R_BLOCK) {
                    return SE050_T1_ERR_PROTOCOL;
                }
                host_bn ^= 1U;
                break;
            } while (1);
        }
    }

    /* ---- Receive the response (possibly chained) ---- */
    for (;;) {
        uint8_t  r_pcb = 0;
        uint8_t  inf[SE050_T1_MAX_INF];
        uint16_t inf_len = 0;
        int      rc = t1_read_frame(addr, &r_pcb, inf, sizeof(inf), &inf_len);

        if (rc == SE050_T1_ERR_CRC && retries < T1_MAX_RETRY) {
            /* Corrupted response block: retransmit our last block; the card
             * will resend its response. */
            retries++;
            if (last_len > 0U &&
                t1_write_frame(last_frame, last_len, addr) != SE050_T1_OK) {
                return SE050_T1_ERR_IO;
            }
            continue;
        }
        if (rc != SE050_T1_OK) return rc;

        if ((r_pcb & SE050_T1_PCB_S_BLOCK) == SE050_T1_PCB_S_BLOCK) {
            if (r_pcb == SE050_T1_PCB_S_WTX_REQ) {
                (void)t1_send_simple(addr, SE050_T1_PCB_S_WTX_RSP);
            }
            continue;
        }
        if ((r_pcb & SE050_T1_PCB_R_BLOCK) == SE050_T1_PCB_R_BLOCK) {
            /* Card requests retransmit of our last block. */
            if (retries++ >= T1_MAX_RETRY) return SE050_T1_ERR_PROTOCOL;
            if (last_len > 0U &&
                t1_write_frame(last_frame, last_len, addr) != SE050_T1_OK) {
                return SE050_T1_ERR_IO;
            }
            continue;
        }

        /* I-block: append INF. */
        if ((uint16_t)(resp_used + inf_len) > sizeof(resp_buf)) {
            return SE050_T1_ERR_BUFFER;
        }
        memcpy(&resp_buf[resp_used], inf, inf_len);
        resp_used = (uint16_t)(resp_used + inf_len);

        if ((r_pcb & SE050_T1_PCB_CHAINING) == 0U) {
            break;  /* final block */
        }

        /* Acknowledge and request the next block. */
        if (t1_send_simple(addr, (uint8_t)(SE050_T1_PCB_R_BLOCK | card_bn)) !=
            SE050_T1_OK) {
            return SE050_T1_ERR_IO;
        }
        card_bn ^= 1U;
    }

    if (resp_len != NULL) {
        uint16_t copy = resp_used;

        if (copy > resp_size) copy = resp_size;
        if (resp != NULL && copy > 0U) memcpy(resp, resp_buf, copy);
        *resp_len = copy;
    }

    return SE050_T1_OK;
}

/* ── Cached-response adapter for the SCP03 layer ──────────── */
static uint8_t  s_cached[512];
static uint16_t s_cached_len;

int se050_t1_send_apdu(uint8_t addr, const uint8_t *apdu, uint16_t apdu_len)
{
    uint16_t len = 0;
    int rc = se050_t1_transceive(addr, apdu, apdu_len,
                                 s_cached, sizeof(s_cached), &len);
    s_cached_len = (rc == SE050_T1_OK) ? len : 0;
    return rc;
}

int se050_t1_read_cached(uint8_t *buffer, uint16_t size, uint16_t *out_len)
{
    uint16_t copy;

    if (buffer == NULL || out_len == NULL) return SE050_T1_ERR_BUFFER;
    if (s_cached_len == 0U) return SE050_T1_ERR_IO;

    copy = (s_cached_len > size) ? size : s_cached_len;
    memcpy(buffer, s_cached, copy);
    *out_len = copy;

    return (copy == s_cached_len) ? SE050_T1_OK : SE050_T1_ERR_BUFFER;
}
