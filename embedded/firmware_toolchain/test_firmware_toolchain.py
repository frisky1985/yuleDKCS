#!/usr/bin/env python3
"""
test_firmware_toolchain.py — 固件签名/加密工具链单元测试

覆盖:
  - 头部打包/解析往返
  - 签名+加密 → 验签+解密 正路径
  - 篡改检测: payload 1 字节 / signature / magic / version / nonce / tag
  - 错误密钥 (公钥不匹配)
  - CLI gen-keys + build + verify 端到端
"""

import hashlib
import os
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from fw_header import (
    MAGIC_U32, HEADER_LEN, FirmwareHeader, pack_package,
)
import sign_firmware
import verify_firmware
from verify_firmware import VerifyError, verify_package


def make_keypair(tmpdir):
    sk_path = os.path.join(tmpdir, "sign_key.pem")
    sign_firmware.generate_signing_key(sk_path)
    pk_path = sk_path.replace("_key.pem", "_pub.pem")
    ek_path = os.path.join(tmpdir, "enc_key.bin")
    sign_firmware.generate_enc_key(ek_path)
    signing_key = sign_firmware.load_signing_key(sk_path)
    enc_key = sign_firmware.load_enc_key(ek_path)
    public_key = verify_firmware.load_public_key(pk_path)
    return signing_key, enc_key, public_key


def build_firmware(signing_key, enc_key, payload=b"\x00\x01\x02\x03" * 64,
                   v_maj=1, v_min=2):
    return sign_firmware.build_package(payload, signing_key, enc_key, v_maj, v_min)


class TestHeader(unittest.TestCase):
    def test_pack_parse_roundtrip(self):
        h = FirmwareHeader(version_major=2, version_minor=3)
        h.payload_len = 4096
        h.nonce = b"\xAA" * 12
        h.tag = b"\xBB" * 16
        h.signature = b"\xCC" * 64
        data = h.pack()
        self.assertEqual(len(data), HEADER_LEN)
        h2 = FirmwareHeader.parse(data)
        self.assertEqual(h2.version_major, 2)
        self.assertEqual(h2.version_minor, 3)
        self.assertEqual(h2.payload_len, 4096)
        self.assertEqual(h2.nonce, b"\xAA" * 12)
        self.assertEqual(h2.tag, b"\xBB" * 16)
        self.assertEqual(h2.signature, b"\xCC" * 64)

    def test_parse_bad_magic(self):
        data = bytearray(b"\x00" * HEADER_LEN)
        data[0:4] = b"\xFF\xFF\xFF\xFF"
        with self.assertRaises(ValueError):
            FirmwareHeader.parse(bytes(data))

    def test_parse_short(self):
        with self.assertRaises(ValueError):
            FirmwareHeader.parse(b"\x00" * 10)

    def test_to_be_signed_stable(self):
        h = FirmwareHeader()
        h.nonce = b"\x01" * 12
        h.tag = b"\x02" * 16
        h.payload_len = 16
        tsb1 = h.to_be_signed(b"A" * 16)
        tsb2 = h.to_be_signed(b"A" * 16)
        self.assertEqual(tsb1, tsb2)
        self.assertEqual(len(tsb1), 52 + 16)


class TestSignVerify(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.signing_key, self.enc_key, self.public_key = \
            make_keypair(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_roundtrip(self):
        payload = os.urandom(4096)
        pkg = build_firmware(self.signing_key, self.enc_key, payload, 1, 2)
        plain = verify_package(pkg, self.public_key, self.enc_key)
        self.assertEqual(plain, payload)

    def test_empty_payload(self):
        pkg = build_firmware(self.signing_key, self.enc_key, b"")
        plain = verify_package(pkg, self.public_key, self.enc_key)
        self.assertEqual(plain, b"")

    def test_version_roundtrip(self):
        pkg = build_firmware(self.signing_key, self.enc_key, b"x", 3, 7)
        h = FirmwareHeader.parse(pkg)
        self.assertEqual((h.version_major, h.version_minor), (3, 7))

    # ---------------- 篡改检测 ----------------
    def test_tamper_payload_byte(self):
        payload = b"\xAB" * 256
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, payload))
        pkg[HEADER_LEN + 10] ^= 0xFF  # 翻转加密负载第 11 字节
        with self.assertRaises((VerifyError, ValueError)):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_tamper_signature(self):
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, b"data"))
        pkg[52] ^= 0x01  # 签名首字节翻转
        with self.assertRaises(VerifyError):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_tamper_magic(self):
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, b"data"))
        pkg[0] ^= 0xFF
        with self.assertRaises(ValueError):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_tamper_version(self):
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, b"data"))
        struct.pack_into("<H", pkg, 4, 99)  # 改版本号
        with self.assertRaises(VerifyError):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_tamper_nonce(self):
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, b"data"))
        pkg[20] ^= 0x01  # nonce 首字节
        with self.assertRaises((VerifyError, ValueError)):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_tamper_tag(self):
        pkg = bytearray(build_firmware(self.signing_key, self.enc_key, b"data"))
        pkg[32] ^= 0x01  # tag 首字节
        with self.assertRaises((VerifyError, ValueError)):
            verify_package(bytes(pkg), self.public_key, self.enc_key)

    def test_wrong_public_key(self):
        # 用另一对密钥的公钥验签 → 失败
        other_tmp = tempfile.TemporaryDirectory()
        _, _, other_pub = make_keypair(other_tmp.name)
        pkg = build_firmware(self.signing_key, self.enc_key, b"data")
        with self.assertRaises(VerifyError):
            verify_package(pkg, other_pub, self.enc_key)
        other_tmp.cleanup()

    def test_wrong_enc_key(self):
        other_tmp = tempfile.TemporaryDirectory()
        _, other_enc, _ = make_keypair(other_tmp.name)
        pkg = build_firmware(self.signing_key, self.enc_key, b"data")
        with self.assertRaises((VerifyError, ValueError)):
            verify_package(pkg, self.public_key, other_enc)
        other_tmp.cleanup()

    def test_short_package(self):
        with self.assertRaises((VerifyError, ValueError)):
            verify_package(b"\x00" * 10, self.public_key, self.enc_key)


class TestCLI(unittest.TestCase):
    def test_cli_end_to_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            key_dir = os.path.join(tmp, "keys")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__), "sign_firmware.py"),
                 "gen-keys", "--key-dir", key_dir],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)

            fw = os.path.join(tmp, "fw.bin")
            with open(fw, "wb") as f:
                f.write(os.urandom(2048))

            ydk = os.path.join(tmp, "fw.ydk")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__), "sign_firmware.py"),
                 "build", "--in", fw, "--out", ydk,
                 "--sign-key", os.path.join(key_dir, "dev_signing_key.pem"),
                 "--enc-key", os.path.join(key_dir, "dev_enc_key.bin"),
                 "--version", "2.1.0"],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)

            out = os.path.join(tmp, "fw_plain.bin")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__), "verify_firmware.py"),
                 "--in", ydk, "--pub-key", os.path.join(key_dir, "dev_signing_pub.pem"),
                 "--enc-key", os.path.join(key_dir, "dev_enc_key.bin"),
                 "--out", out],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)

            with open(fw, "rb") as f1, open(out, "rb") as f2:
                self.assertEqual(f1.read(), f2.read())

            # check-only 模式
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__), "verify_firmware.py"),
                 "--in", ydk, "--pub-key", os.path.join(key_dir, "dev_signing_pub.pem"),
                 "--enc-key", os.path.join(key_dir, "dev_enc_key.bin"),
                 "--check-only"],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == "__main__":
    unittest.main()
