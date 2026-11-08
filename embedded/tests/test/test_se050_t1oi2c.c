/*
 * test_se050_t1oi2c.c — SE050 T=1' over I2C transport tests
 *
 * Uses a simulated SE050 card that speaks the same T=1' framing.
 * Verifies: CRC16 known vector, frame encode/decode, simple APDU exchange,
 * chained command blocks, chained response blocks, cached-response adapter.
 */

#include "se050_t1oi2c.h"
#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else      { g_fail++; printf("  FAIL: %s\n", msg); } \
} while (0)

/* ── Simulated SE050 card ─────────────────────────────────── */
static uint8_t  g_q[8192];
static uint16_t g_q_len, g_q_pos;

static uint8_t  g_cmd[1024];
static uint16_t g_cmd_len;

/* Pending response payload for chained-response tests */
static uint8_t  g_card_resp[512];
static uint16_t g_card_resp_len, g_card_resp_off;
static int      g_card_corrupt_once;

/* Last command block + last response frame, for retransmission handling */
static uint8_t  g_last_cmd[300];
static uint16_t g_last_cmd_len;
static uint8_t  g_last_resp[300];
static uint16_t g_last_resp_len;

static void q_reset(void) { g_q_len = 0; g_q_pos = 0; }

static void q_push(uint8_t pcb, const uint8_t *inf, uint16_t len)
{
    g_q_len = (uint16_t)se050_t1_encode(SE050_T1_NAD_SE, pcb, inf, len,
                                        &g_q[g_q_len], (uint16_t)(sizeof(g_q) - g_q_len));
}

/* Queue the next chunk of g_card_resp (254 bytes max, chaining if more). */
static void q_push_resp_chunk(void)
{
    uint16_t remain = (uint16_t)(g_card_resp_len - g_card_resp_off);
    uint16_t chunk  = (remain > SE050_T1_MAX_INF) ? SE050_T1_MAX_INF : remain;
    uint8_t  pcb    = (remain > chunk) ? SE050_T1_PCB_CHAINING : 0x00;
    uint16_t start  = g_q_len;

    q_push(pcb, &g_card_resp[g_card_resp_off], chunk);
    g_card_resp_off = (uint16_t)(g_card_resp_off + chunk);

    /* Remember the clean I-block frame for a retransmission request. */
    if ((pcb & SE050_T1_PCB_S_BLOCK) != SE050_T1_PCB_S_BLOCK) {
        g_last_resp_len = (uint16_t)(g_q_len - start);
        if (g_last_resp_len <= sizeof(g_last_resp)) {
            memcpy(g_last_resp, &g_q[start], g_last_resp_len);
        }
    }

    if (g_card_corrupt_once) {
        /* Corrupt the CRC of the queued frame (last 2 bytes). */
        g_q[g_q_len - 1] ^= 0xFF;
        g_card_corrupt_once = 0;
    }
}

static int mock_write(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    uint8_t  pcb;
    uint8_t  inf[SE050_T1_MAX_INF];
    uint16_t inf_len = 0;
    uint16_t crc;

    (void)addr;

    if (len < 5) return -1;
    if (buf[0] != SE050_T1_NAD_HOST) return -1;

    crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (crc != se050_t1_crc16(buf, (uint16_t)(len - 2))) return -1;

    pcb = buf[1];

    if ((pcb & SE050_T1_PCB_S_BLOCK) == SE050_T1_PCB_S_BLOCK) {
        return 0;  /* S-block response from host: ignore */
    }

    if ((pcb & SE050_T1_PCB_R_BLOCK) == SE050_T1_PCB_R_BLOCK) {
        /* Host acknowledges a chained response block → send the next one. */
        if (g_card_resp_off < g_card_resp_len) {
            q_push_resp_chunk();
        }
        return 0;
    }

    /* I-block from host: parse manually (se050_t1_decode validates the SE NAD). */
    if (g_q_len == 0U && len == g_last_cmd_len &&
        memcmp(buf, g_last_cmd, len) == 0 && g_last_resp_len > 0U) {
        /* Retransmission: re-send the same response block. */
        memcpy(g_q, g_last_resp, g_last_resp_len);
        g_q_len = g_last_resp_len;
        g_q_pos = 0;
        return 0;
    }
    memcpy(g_last_cmd, buf, len);
    g_last_cmd_len = len;
    g_last_resp_len = 0;

    inf_len = (buf[2] == 0U) ? 256U : (uint16_t)buf[2];
    if ((uint16_t)(inf_len + 5U) != len) return -1;
    if (inf_len > 0U) {
        memcpy(inf, &buf[3], inf_len);
    }

    if (g_cmd_len + inf_len <= sizeof(g_cmd)) {
        memcpy(&g_cmd[g_cmd_len], inf, inf_len);
        g_cmd_len = (uint16_t)(g_cmd_len + inf_len);
    }

    if ((pcb & SE050_T1_PCB_CHAINING) != 0U) {
        /* Acknowledge the chained block. */
        q_push((uint8_t)(SE050_T1_PCB_R_BLOCK | (pcb & 0x01U)), NULL, 0);
        return 0;
    }

    /* Final block: build the response. */
    if (g_card_resp_len == 0U) {
        /* Default: echo command bytes + SW 9000 */
        uint16_t echo = (g_cmd_len > 32U) ? 32U : g_cmd_len;
        memcpy(g_card_resp, g_cmd, echo);
        g_card_resp[echo]      = 0x90;
        g_card_resp[echo + 1U] = 0x00;
        g_card_resp_len = (uint16_t)(echo + 2U);
        g_card_resp_off = 0;
    }
    q_push_resp_chunk();
    return 0;
}

static int mock_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    (void)addr;

    if (g_q_pos >= g_q_len) return -1;     /* no response available */

    if (len == 3U) {
        memcpy(buf, g_q, 3U);
        g_q_pos = 3;
        return 0;
    }

    if ((uint16_t)(g_q_pos + len) > g_q_len) return -1;
    memcpy(buf, &g_q[g_q_pos], len);
    g_q_pos = (uint16_t)(g_q_pos + len);

    if (g_q_pos >= g_q_len) q_reset();     /* frame consumed */
    return 0;
}

/* Unused default backend symbol required by se050_t1oi2c.c */
int32_t i2c_transfer(uint8_t dev, uint8_t addr,
                     const uint8_t *tx, uint16_t tx_len,
                     uint8_t *rx, uint16_t rx_len)
{
    (void)dev; (void)addr; (void)tx; (void)tx_len; (void)rx; (void)rx_len;
    return -1;
}

static void reset_card(void)
{
    q_reset();
    g_cmd_len = 0;
    g_card_resp_len = 0;
    g_card_resp_off = 0;
    g_card_corrupt_once = 0;
    g_last_cmd_len = 0;
    g_last_resp_len = 0;
}

int main(void)
{
    se050_t1_io_t io = { mock_write, mock_read };
    se050_t1_set_io(&io);

    printf("=== SE050 T=1' transport tests ===\n");

    /* 1. CRC16 known vector (CRC-16/CCITT-FALSE) */
    printf("\n[1] CRC16\n");
    CHECK(se050_t1_crc16((const uint8_t *)"123456789", 9) == 0x29B1U,
          "CRC16(\"123456789\") == 0x29B1");

    /* 2. Frame encode/decode */
    printf("\n[2] Frame codec\n");
    {
        const uint8_t inf[5] = { 0x80, 0x50, 0x00, 0x00, 0x08 };
        uint8_t  frame[32];
        uint8_t  pcb = 0;
        uint8_t  out[16];
        uint16_t out_len = 0;
        int n = se050_t1_encode(SE050_T1_NAD_SE, 0x00, inf, 5, frame, sizeof(frame));

        CHECK(n == 10, "frame length = 5 + 5");
        CHECK(se050_t1_decode(frame, (uint16_t)n, &pcb, out, sizeof(out), &out_len)
                  == SE050_T1_OK && out_len == 5
                  && memcmp(out, inf, 5) == 0,
              "decode round-trips INF");
        frame[n - 1] ^= 0xFF;
        CHECK(se050_t1_decode(frame, (uint16_t)n, &pcb, out, sizeof(out), &out_len)
                  == SE050_T1_ERR_CRC,
              "corrupted CRC rejected");
    }

    /* 3. Simple APDU exchange */
    printf("\n[3] Simple transceive\n");
    {
        const uint8_t apdu[4] = { 0x80, 0xCA, 0x00, 0x00 };
        uint8_t  resp[64];
        uint16_t resp_len = 0;

        reset_card();
        CHECK(se050_t1_transceive(0x48, apdu, 4, resp, sizeof(resp), &resp_len)
                  == SE050_T1_OK, "transceive returns OK");
        CHECK(resp_len == 6 && resp[4] == 0x90 && resp[5] == 0x00,
              "response contains echo + SW 9000");
        CHECK(g_cmd_len == 4 && memcmp(g_cmd, apdu, 4) == 0,
              "card received exact APDU");
    }

    /* 4. Chained command (300 bytes) */
    printf("\n[4] Chained command\n");
    {
        uint8_t  apdu[300];
        uint8_t  resp[64];
        uint16_t resp_len = 0;
        uint16_t i;

        for (i = 0; i < sizeof(apdu); i++) apdu[i] = (uint8_t)i;

        reset_card();
        CHECK(se050_t1_transceive(0x48, apdu, sizeof(apdu),
                                  resp, sizeof(resp), &resp_len) == SE050_T1_OK,
              "chained command transceive OK");
        CHECK(g_cmd_len == sizeof(apdu) && memcmp(g_cmd, apdu, sizeof(apdu)) == 0,
              "card reassembled 300-byte APDU");
    }

    /* 5. Chained response (300 bytes) */
    printf("\n[5] Chained response\n");
    {
        const uint8_t apdu[4] = { 0x00, 0xCA, 0x00, 0x00 };
        uint8_t  resp[400];
        uint16_t resp_len = 0;
        uint16_t i;

        reset_card();
        for (i = 0; i < 300; i++) g_card_resp[i] = (uint8_t)(0xA0 + (i & 0x0F));
        g_card_resp_len = 300;
        g_card_resp_off = 0;

        CHECK(se050_t1_transceive(0x48, apdu, 4, resp, sizeof(resp), &resp_len)
                  == SE050_T1_OK, "chained response transceive OK");
        CHECK(resp_len == 300, "response reassembled to 300 bytes");
        CHECK(resp[0] == 0xA0 && resp[299] == (uint8_t)(0xA0 + (299 & 0x0F)),
              "response content intact");
    }

    /* 6. Corrupted response block triggers retransmission */
    printf("\n[6] CRC retry\n");
    {
        const uint8_t apdu[4] = { 0x00, 0xCA, 0x00, 0x00 };
        uint8_t  resp[32];
        uint16_t resp_len = 0;

        reset_card();
        g_card_corrupt_once = 1;
        CHECK(se050_t1_transceive(0x48, apdu, 4, resp, sizeof(resp), &resp_len)
                  == SE050_T1_OK, "recovers from one corrupted block");
    }

    /* 7. Cached-response adapter */
    printf("\n[7] Cached adapter\n");
    {
        const uint8_t apdu[4] = { 0x80, 0xCA, 0x00, 0x00 };
        uint8_t  out[64];
        uint16_t out_len = 0;

        reset_card();
        CHECK(se050_t1_send_apdu(0x48, apdu, 4) == SE050_T1_OK,
              "send_apdu OK");
        CHECK(se050_t1_read_cached(out, sizeof(out), &out_len) == SE050_T1_OK
                  && out_len == 6 && out[4] == 0x90,
              "read_cached returns response");
    }

    printf("\n=== 结果: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
