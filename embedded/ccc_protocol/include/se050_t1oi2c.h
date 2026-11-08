/**
 * @file se050_t1oi2c.h
 * @brief SE050 T=1' over I2C (T1oI2C) transport layer.
 *
 * The SE050 secure element does NOT accept raw ISO 7816-4 APDUs on the I2C
 * bus. It requires the T=1' block protocol defined in NXP AN12413:
 *
 *   NAD | PCB | LEN | INF (LEN bytes) | CRC16 (2 bytes, little-endian)
 *
 * This module frames/unframes APDUs and manages block chaining. It sits
 * below se050_scp03.c and above the platform I2C driver.
 *
 * Reference: NXP AN12413 "SE050 T=1 over I2C (T1oI2C) protocol".
 */

#ifndef SE050_T1OI2C_H
#define SE050_T1OI2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Protocol constants (AN12413) ────────────────────────── */
#define SE050_T1_NAD_HOST       0x5AU   /**< NAD used by host → SE050 */
#define SE050_T1_NAD_SE         0x5BU   /**< NAD used by SE050 → host */
#define SE050_T1_MAX_INF        254U    /**< Max INF per I-block */
#define SE050_T1_MAX_FRAME      (1U + 1U + 1U + SE050_T1_MAX_INF + 2U)

/* PCB masks */
#define SE050_T1_PCB_BLOCK_MASK 0x01U
#define SE050_T1_PCB_CHAINING   0x20U
#define SE050_T1_PCB_R_BLOCK    0x80U
#define SE050_T1_PCB_S_BLOCK    0xC0U
#define SE050_T1_PCB_S_WTX_REQ  0xC3U
#define SE050_T1_PCB_S_WTX_RSP  0xE3U

/* ── Error codes ─────────────────────────────────────────── */
#define SE050_T1_OK               0
#define SE050_T1_ERR_IO         (-1)    /**< I2C transfer failed */
#define SE050_T1_ERR_CRC        (-2)    /**< CRC mismatch on received frame */
#define SE050_T1_ERR_FRAME     (-3)    /**< Malformed frame (NAD/LEN/PCB) */
#define SE050_T1_ERR_BUFFER    (-4)    /**< Response larger than caller buffer */
#define SE050_T1_ERR_PROTOCOL  (-5)    /**< Unexpected block / retry exhausted */

/**
 * @brief Platform I/O hooks.
 *
 * write/read are mandatory. delay_us is optional and is invoked between
 * read retries; it lets a simulation backend advance a virtual clock
 * instead of busy-spinning (and maps to a real platform delay on target).
 *
 * @param addr  7-bit SE050 I2C address (0x48)
 */
typedef struct {
    int (*write)(uint8_t addr, const uint8_t *buf, uint16_t len);
    int (*read)(uint8_t addr, uint8_t *buf, uint16_t len);
    void (*delay_us)(uint32_t us);
} se050_t1_io_t;

/*
 * A real SE050 holds the I2C transaction (clock stretching / NACK) while it
 * is busy, so the first read of a response block must be retried. The retry
 * budget also covers SCP03 command processing time and S(WTX) handling.
 */
#define SE050_T1_READ_RETRY_MAX       100U   /**< Attempts per block header */
#define SE050_T1_READ_RETRY_DELAY_US  1000U  /**< Delay between attempts (us) */

/**
 * @brief Install custom I/O hooks (tests / alternative HAL).
 *
 * Passing NULL restores the default platform `i2c_transfer()` backend.
 */
void se050_t1_set_io(const se050_t1_io_t *io);

/**
 * @brief Compute T=1 CRC16 (ISO/IEC 3309, poly 0x1021, init 0xFFFF).
 */
uint16_t se050_t1_crc16(const uint8_t *data, uint16_t len);

/**
 * @brief Encode a single T=1' frame into @p out.
 *
 * @param nad    NAD byte (host or SE)
 * @param pcb    Protocol control byte
 * @param inf    Information field (may be NULL when len == 0)
 * @param len    INF length, 0..254
 * @param out    Output buffer, at least len + 5 bytes
 * @return Total frame length, or negative SE050_T1_ERR_* on error
 */
int se050_t1_encode(uint8_t nad, uint8_t pcb,
                    const uint8_t *inf, uint16_t len,
                    uint8_t *out, uint16_t out_size);

/**
 * @brief Decode and CRC-check a received T=1' frame.
 *
 * @param frame      Raw frame bytes
 * @param frame_len  Number of bytes available
 * @param pcb        [out] PCB byte
 * @param inf        [out] Static buffer receiving the INF field
 * @param inf_size   Size of @p inf
 * @param inf_len    [out] INF length
 * @return SE050_T1_OK or negative SE050_T1_ERR_*
 */
int se050_t1_decode(const uint8_t *frame, uint16_t frame_len,
                    uint8_t *pcb,
                    uint8_t *inf, uint16_t inf_size, uint16_t *inf_len);

/**
 * @brief Transceive one APDU over T=1' (block chaining handled internally).
 *
 * @param addr      SE050 I2C address
 * @param apdu      Command APDU (up to 65535 bytes; chained in 254-byte blocks)
 * @param apdu_len  APDU length
 * @param resp      [out] Response APDU (data + SW1 SW2), may be NULL if only
 *                  status is needed
 * @param resp_size Capacity of @p resp
 * @param resp_len  [out] Actual response length
 * @return SE050_T1_OK or negative SE050_T1_ERR_*
 */
int se050_t1_transceive(uint8_t addr,
                        const uint8_t *apdu, uint16_t apdu_len,
                        uint8_t *resp, uint16_t resp_size, uint16_t *resp_len);

/**
 * @brief Send an APDU and cache the response.
 *
 * Provided for the SCP03 layer, which historically split the exchange into
 * write() + read() calls. The response is available via
 * se050_t1_read_cached() until the next send.
 */
int se050_t1_send_apdu(uint8_t addr, const uint8_t *apdu, uint16_t apdu_len);

/**
 * @brief Copy the cached response produced by se050_t1_send_apdu().
 *
 * @param buffer     [out] Destination
 * @param size       Capacity of @p buffer
 * @param out_len    [out] Number of bytes actually copied
 * @return SE050_T1_OK or negative SE050_T1_ERR_*
 */
int se050_t1_read_cached(uint8_t *buffer, uint16_t size, uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* SE050_T1OI2C_H */
