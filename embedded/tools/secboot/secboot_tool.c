/**
 * @file secboot_tool.c
 * @brief Host-side manifest signing / image verification tool.
 *
 * Usage:
 *   secboot_tool sign <image> <priv_hex> <p256|sm2> <image_version>
 *                     <anti_rollback> <key_id_hex> <out_manifest>
 *   secboot_tool verify <image> <manifest> <pub_hex> [p256|sm2]
 *
 * Key files are hexadecimal text (whitespace allowed):
 *   private key 64 hex chars, public key 128 hex chars, key_id 32 hex chars.
 */

#include "secure_boot.h"
#include "crypto_random.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECBOOT_MAX_IMAGE (64U * 1024U * 1024U)

static int read_file(const char *path, uint8_t **out, size_t *out_len)
{
    FILE *f;
    long  size;
    uint8_t *buf;

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return -1;
    }
    if ((fseek(f, 0L, SEEK_END) != 0) || ((size = ftell(f)) < 0L) ||
        (fseek(f, 0L, SEEK_SET) != 0)) {
        fprintf(stderr, "error: cannot size %s\n", path);
        (void)fclose(f);
        return -1;
    }
    if ((unsigned long)size > SECBOOT_MAX_IMAGE) {
        fprintf(stderr, "error: %s is too large\n", path);
        (void)fclose(f);
        return -1;
    }

    buf = (uint8_t *)malloc((size_t)size + 1U);
    if (buf == NULL) {
        fprintf(stderr, "error: out of memory\n");
        (void)fclose(f);
        return -1;
    }
    if ((size > 0L) && (fread(buf, 1U, (size_t)size, f) != (size_t)size)) {
        fprintf(stderr, "error: cannot read %s\n", path);
        free(buf);
        (void)fclose(f);
        return -1;
    }
    buf[size] = 0U;
    (void)fclose(f);

    *out = buf;
    *out_len = (size_t)size;
    return 0;
}

static int hex_val(int c)
{
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    return -1;
}

static int read_hex_file(const char *path, uint8_t *out, size_t out_len)
{
    uint8_t *text;
    size_t   text_len;
    size_t   n = 0U;
    size_t   i;
    int      hi = -1;
    int      bad = 0;

    if (read_file(path, &text, &text_len) != 0) {
        return -1;
    }

    for (i = 0U; i < text_len; i++) {
        int c = (int)text[i];

        if (isspace(c)) {
            continue;
        }
        {
            int v = hex_val(c);
            if (v < 0) {
                bad = 1;
                break;
            }
            if (hi < 0) {
                hi = v;
            } else {
                if (n >= out_len) {
                    bad = 1;
                    break;
                }
                out[n++] = (uint8_t)((hi << 4) | v);
                hi = -1;
            }
        }
    }
    free(text);

    if ((hi >= 0) || (bad != 0) || (n != out_len)) {
        fprintf(stderr, "error: %s must contain exactly %lu hex chars\n",
                path, (unsigned long)(out_len * 2U));
        return -1;
    }
    return 0;
}

static int parse_key_type(const char *s, uint8_t *out)
{
    if (strcmp(s, "p256") == 0) {
        *out = SB_KEY_TYPE_P256;
        return 0;
    }
    if (strcmp(s, "sm2") == 0) {
        *out = SB_KEY_TYPE_SM2;
        return 0;
    }
    fprintf(stderr, "error: key type must be p256 or sm2\n");
    return -1;
}

static int parse_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;

    errno = 0;
    v = strtoul(s, &end, 0);
    if ((errno != 0) || (end == s) || (*end != '\0') || (v > 0xFFFFFFFFUL)) {
        fprintf(stderr, "error: invalid u32 value '%s'\n", s);
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int cmd_sign(int argc, char **argv)
{
    uint8_t  priv[32];
    uint8_t  key_id[SB_KEY_ID_SIZE];
    uint8_t  key_type;
    uint8_t *image = NULL;
    size_t   image_len = 0U;
    uint8_t  manifest[SB_MANIFEST_SIZE];
    size_t   manifest_len = 0U;
    uint32_t image_version;
    uint32_t anti_rollback;
    FILE    *f;
    int      rc;

    if (argc != 9) {
        fprintf(stderr, "usage: %s sign <image> <priv_hex> <p256|sm2> "
                        "<image_version> <anti_rollback> <key_id_hex> "
                        "<out_manifest>\n", argv[0]);
        return 2;
    }
    if ((read_hex_file(argv[3], priv, sizeof(priv)) != 0) ||
        (parse_key_type(argv[4], &key_type) != 0) ||
        (parse_u32(argv[5], &image_version) != 0) ||
        (parse_u32(argv[6], &anti_rollback) != 0) ||
        (read_hex_file(argv[7], key_id, sizeof(key_id)) != 0) ||
        (read_file(argv[2], &image, &image_len) != 0)) {
        free(image);
        return 2;
    }

    if (crypto_random_init() != CRYPTO_RANDOM_OK) {
        fprintf(stderr, "error: no entropy source available\n");
        free(image);
        return 1;
    }

    rc = sb_manifest_sign(priv, key_type, image_version, anti_rollback,
                          image, image_len, key_id,
                          manifest, sizeof(manifest), &manifest_len);
    free(image);
    if (rc != SB_OK) {
        fprintf(stderr, "error: sign failed (%d)\n", rc);
        return 1;
    }

    f = fopen(argv[8], "wb");
    if (f == NULL) {
        fprintf(stderr, "error: cannot create %s\n", argv[8]);
        return 1;
    }
    if (fwrite(manifest, 1U, manifest_len, f) != manifest_len) {
        fprintf(stderr, "error: cannot write %s\n", argv[8]);
        (void)fclose(f);
        return 1;
    }
    (void)fclose(f);

    printf("signed: %s -> %s (image_len=%lu version=%lu counter=%lu)\n",
           argv[2], argv[8], (unsigned long)image_len,
           (unsigned long)image_version, (unsigned long)anti_rollback);
    return 0;
}

static int cmd_verify(int argc, char **argv)
{
    uint8_t  pub[64];
    uint8_t  key_type = SB_KEY_TYPE_P256;
    uint8_t *image = NULL;
    size_t   image_len = 0U;
    uint8_t *manifest = NULL;
    size_t   manifest_len = 0U;
    sb_verify_result_t result;
    int rc;

    if ((argc != 5) && (argc != 6)) {
        fprintf(stderr, "usage: %s verify <image> <manifest> <pub_hex> "
                        "[p256|sm2]\n", argv[0]);
        return 2;
    }
    if ((argc == 6) && (parse_key_type(argv[5], &key_type) != 0)) {
        return 2;
    }
    if ((read_hex_file(argv[4], pub, sizeof(pub)) != 0) ||
        (read_file(argv[2], &image, &image_len) != 0) ||
        (read_file(argv[3], &manifest, &manifest_len) != 0)) {
        free(image);
        free(manifest);
        return 2;
    }

    (void)memset(&result, 0, sizeof(result));
    rc = sb_verify_image(image, image_len, manifest, manifest_len,
                         pub, key_type, 0U, &result);
    free(image);
    free(manifest);

    if (rc != SB_OK) {
        fprintf(stderr, "verify: FAIL (%d)\n", rc);
        return 1;
    }

    printf("verify: OK (image_len=%lu version=%lu counter=%lu key_type=%u)\n",
           (unsigned long)result.image_len,
           (unsigned long)result.image_version,
           (unsigned long)result.anti_rollback,
           (unsigned)result.key_type);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s {sign|verify} ...\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "sign") == 0) {
        return cmd_sign(argc, argv);
    }
    if (strcmp(argv[1], "verify") == 0) {
        return cmd_verify(argc, argv);
    }

    fprintf(stderr, "error: unknown command '%s'\n", argv[1]);
    return 2;
}
