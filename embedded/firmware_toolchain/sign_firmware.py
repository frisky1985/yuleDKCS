#!/usr/bin/env python3
"""
sign_firmware.py — yuleDKCS 生产固件签名 + 加密打包

用法:
  # 1. 生成开发密钥对 (生产环境使用 HSM, 见 docs/FLASHING-GUIDE.md)
  python3 sign_firmware.py --gen-keys --key-dir ./keys

  # 2. 签名 + AES-256-GCM 加密固件
  python3 sign_firmware.py --in firmware.bin --out firmware.ydk \\
      --sign-key keys/dev_signing_key.pem --enc-key keys/dev_enc_key.bin \\
      --version 1.2.0

  输出: firmware.ydk (FirmwareHeader + AES-256-GCM 加密负载)
"""

import argparse
import hashlib
import os
import secrets
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from Crypto.Cipher import AES

from fw_header import (
    ALGO_ECDSA_P256, ENC_AES256_GCM, HEADER_LEN, SIG_LEN_P256,
    FirmwareHeader, pack_package,
)

ENC_KEY_LEN = 32  # AES-256


# ---------------------------------------------------------------------------
#  密钥管理 (开发环境; 生产使用 HSM, 密钥永不落盘到产线机器)
# ---------------------------------------------------------------------------
def generate_signing_key(path):
    """生成 ECDSA P-256 签名私钥 (PEM)."""
    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    with open(path, "wb") as f:
        f.write(pem)
    pub = key.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    with open(path.replace("_key.pem", "_pub.pem"), "wb") as f:
        f.write(pub)
    return path


def load_signing_key(path):
    with open(path, "rb") as f:
        return serialization.load_pem_private_key(f.read(), password=None)


def generate_enc_key(path):
    """生成 AES-256 加密密钥 (32 字节二进制)."""
    key = secrets.token_bytes(ENC_KEY_LEN)
    with open(path, "wb") as f:
        f.write(key)
    return path


def load_enc_key(path):
    with open(path, "rb") as f:
        key = f.read()
    if len(key) != ENC_KEY_LEN:
        raise ValueError(f"加密密钥必须 {ENC_KEY_LEN} 字节, got {len(key)}")
    return key


# ---------------------------------------------------------------------------
#  签名 / 加密
# ---------------------------------------------------------------------------
def sign_tsb(tsb: bytes, signing_key) -> bytes:
    """对待签区 (TSB) 做 ECDSA P-256 签名, 返回 64 字节 r||s."""
    sig = signing_key.sign(tsb, ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(sig)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def build_package(firmware: bytes, signing_key, enc_key: bytes,
                  version_major: int, version_minor: int) -> bytes:
    """签名 + 加密固件, 返回完整 .ydk 包."""
    header = FirmwareHeader(version_major=version_major,
                            version_minor=version_minor,
                            algo=ALGO_ECDSA_P256, enc=ENC_AES256_GCM)

    # 1. 随机 nonce, AES-256-GCM 加密
    header.nonce = secrets.token_bytes(12)
    cipher = AES.new(enc_key, AES.MODE_GCM, nonce=header.nonce)
    encrypted, tag = cipher.encrypt_and_digest(firmware)
    header.tag = tag
    header.payload_len = len(encrypted)

    # 2. 签名 (覆盖头部待签区 + 加密负载; 验证端先验签后解密)
    tsb = header.to_be_signed(encrypted)
    header.signature = sign_tsb(tsb, signing_key)

    return pack_package(header, encrypted)


def parse_version(version: str):
    parts = version.split(".")
    major = int(parts[0])
    minor = int(parts[1]) if len(parts) > 1 else 0
    return major, minor


# ---------------------------------------------------------------------------
#  CLI
# ---------------------------------------------------------------------------
def main(argv=None):
    parser = argparse.ArgumentParser(description="yuleDKCS 固件签名/加密工具")
    sub = parser.add_subparsers(dest="cmd")

    gen = sub.add_parser("gen-keys", help="生成开发密钥对")
    gen.add_argument("--key-dir", default="./keys")

    p = sub.add_parser("build", help="签名+加密固件")
    p.add_argument("--in", dest="inp", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--sign-key", required=True)
    p.add_argument("--enc-key", required=True)
    p.add_argument("--version", default="1.0.0")

    args = parser.parse_args(argv)

    if args.cmd == "gen-keys":
        os.makedirs(args.key_dir, exist_ok=True)
        sk = generate_signing_key(os.path.join(args.key_dir, "dev_signing_key.pem"))
        ek = generate_enc_key(os.path.join(args.key_dir, "dev_enc_key.bin"))
        print(f"[OK] 签名密钥: {sk} (+dev_signing_pub.pem)")
        print(f"[OK] 加密密钥: {ek}")
        print("[!] 生产环境禁止使用 dev 密钥, 详见 docs/FLASHING-GUIDE.md")
        return 0

    if args.cmd == "build":
        with open(args.inp, "rb") as f:
            firmware = f.read()
        signing_key = load_signing_key(args.sign_key)
        enc_key = load_enc_key(args.enc_key)
        v_maj, v_min = parse_version(args.version)
        package = build_package(firmware, signing_key, enc_key, v_maj, v_min)
        with open(args.out, "wb") as f:
            f.write(package)
        digest = hashlib.sha256(package).hexdigest()[:16]
        print(f"[OK] {args.out}: {len(package)} 字节 "
              f"(固件 {len(firmware)} B, 版本 {v_maj}.{v_min}, sha256={digest}...)")
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
