#!/usr/bin/env python3
"""
verify_firmware.py — yuleDKCS 固件包验签 + 解密 (烧录前校验)

用法:
  python3 verify_firmware.py --in firmware.ydk --pub-key keys/dev_signing_pub.pem \\
      --enc-key keys/dev_enc_key.bin --out firmware_plain.bin

  --check-only  仅验签, 不解密输出
"""

import argparse
import hashlib
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from Crypto.Cipher import AES

from fw_header import (
    ALGO_ECDSA_P256, ENC_AES256_GCM, HEADER_LEN,
    FirmwareHeader,
)


class VerifyError(Exception):
    pass


def load_public_key(path):
    with open(path, "rb") as f:
        return serialization.load_pem_public_key(f.read())


def verify_signature(tsb: bytes, signature: bytes, public_key) -> None:
    """验证 ECDSA P-256 签名 (r||s)."""
    if len(signature) != 64:
        raise VerifyError(f"签名长度 {len(signature)} != 64")
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    der = utils.encode_dss_signature(r, s)
    try:
        public_key.verify(der, tsb, ec.ECDSA(hashes.SHA256()))
    except Exception as e:
        raise VerifyError(f"签名验证失败: {e}")


def verify_package(package: bytes, public_key, enc_key: bytes,
                   check_only=False):
    """验签 + 解密 .ydk 包, 返回明文固件."""
    if len(package) < HEADER_LEN:
        raise VerifyError("包过短")

    header = FirmwareHeader.parse(package)
    if header.algo != ALGO_ECDSA_P256:
        raise VerifyError(f"不支持的签名算法 0x{header.algo:02X}")
    if header.enc != ENC_AES256_GCM:
        raise VerifyError(f"不支持的加密算法 0x{header.enc:02X}")

    tsb, encrypted, _ = FirmwareHeader.to_be_signed_from_package(package)
    verify_signature(tsb, header.signature, public_key)

    # AES-256-GCM 解密 (GCM 自带完整性校验, tag 不匹配即抛错)
    cipher = AES.new(enc_key, AES.MODE_GCM, nonce=header.nonce)
    try:
        plain = cipher.decrypt_and_verify(encrypted, header.tag)
    except ValueError as e:
        raise VerifyError(f"解密/完整性校验失败 (tag 不匹配): {e}")

    if check_only:
        return None
    return plain


def main(argv=None):
    parser = argparse.ArgumentParser(description="yuleDKCS 固件验签/解密工具")
    parser.add_argument("--in", dest="inp", required=True)
    parser.add_argument("--pub-key", required=True)
    parser.add_argument("--enc-key", required=True)
    parser.add_argument("--out")
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args(argv)

    with open(args.inp, "rb") as f:
        package = f.read()
    public_key = load_public_key(args.pub_key)
    with open(args.enc_key, "rb") as f:
        enc_key = f.read()

    try:
        plain = verify_package(package, public_key, enc_key,
                               check_only=args.check_only)
    except VerifyError as e:
        print(f"[FAIL] {e}", file=sys.stderr)
        return 1

    if args.check_only:
        print(f"[OK] 验签通过: {args.inp} "
              f"({len(package)} B, sha256={hashlib.sha256(package).hexdigest()[:16]}...)")
        return 0

    with open(args.out, "wb") as f:
        f.write(plain)
    print(f"[OK] 验签通过 + 解密: {args.out} ({len(plain)} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
