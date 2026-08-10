#!/usr/bin/env python3
"""
fw_header.py — yuleDKCS 生产固件包头部结构

固件包 (.ydk) 二进制布局 (固定头部 + 加密负载):

  [0]   4B   magic      "YDKC" (0x59444B43)
  [4]   2B   version_major
  [6]   2B   version_minor
  [8]   2B   header_len   (当前 48, 预留扩展)
  [10]  1B   algo         0x01=ECDSA-P256, 0x02=SM2(预留)
  [11]  1B   enc          0x01=AES-256-GCM
  [12]  4B   reserved
  [16]  4B   payload_len  加密负载字节数
  [20]  12B  nonce        AES-GCM nonce
  [32]  16B  tag          AES-GCM 认证标签
  [48]  2B   sig_len      签名长度 (P-256=64, SM2=64)
  [50]  2B   reserved2
  [52]  64B  signature    (r||s)
  [116] --   payload      AES-256-GCM 加密的固件体

签名输入 (TSB, To-Be-Signed):
  magic .. reserved2 (52 字节, 不含 signature) + 明文固件体
"""

import struct

MAGIC = b"YDKC"
MAGIC_U32 = 0x59444B43

ALGO_ECDSA_P256 = 0x01
ALGO_SM2 = 0x02

ENC_AES256_GCM = 0x01

HEADER_LEN = 116
SIG_LEN_P256 = 64
NONCE_LEN = 12
TAG_LEN = 16

PACKAGE_EXT = ".ydk"


class FirmwareHeader:
    """固件包头部 (116 字节定长)."""

    __slots__ = (
        "version_major", "version_minor", "algo", "enc",
        "payload_len", "nonce", "tag", "signature",
    )

    def __init__(self, version_major=1, version_minor=0,
                 algo=ALGO_ECDSA_P256, enc=ENC_AES256_GCM):
        self.version_major = version_major
        self.version_minor = version_minor
        self.algo = algo
        self.enc = enc
        self.payload_len = 0
        self.nonce = b"\x00" * NONCE_LEN
        self.tag = b"\x00" * TAG_LEN
        self.signature = b"\x00" * SIG_LEN_P256

    # ------------------------------------------------------------------
    def pack(self):
        """序列化头部 (116 字节)."""
        if len(self.nonce) != NONCE_LEN:
            raise ValueError(f"nonce 必须 {NONCE_LEN} 字节, got {len(self.nonce)}")
        if len(self.tag) != TAG_LEN:
            raise ValueError(f"tag 必须 {TAG_LEN} 字节, got {len(self.tag)}")
        return struct.pack(
            "<IHHHBB4sI12s16sHH64s",
            MAGIC_U32,
            self.version_major, self.version_minor,
            HEADER_LEN,
            self.algo, self.enc,
            b"\x00\x00\x00\x00",
            self.payload_len,
            self.nonce,
            self.tag,
            len(self.signature), 0,
            self.signature,
        )

    @classmethod
    def parse(cls, data):
        """从 116 字节解析头部."""
        if len(data) < HEADER_LEN:
            raise ValueError(f"头部不足 {HEADER_LEN} 字节, got {len(data)}")
        (magic, v_maj, v_min, hlen, algo, enc, _rsv,
         plen, nonce, tag, sig_len, _rsv2, sig) = struct.unpack(
            "<IHHHBB4sI12s16sHH64s", data[:HEADER_LEN])
        if magic != MAGIC_U32:
            raise ValueError(f"magic 不匹配: 0x{magic:08X} != 0x{MAGIC_U32:08X}")
        if hlen != HEADER_LEN:
            raise ValueError(f"header_len={hlen}, 期望 {HEADER_LEN}")
        h = cls(version_major=v_maj, version_minor=v_min, algo=algo, enc=enc)
        h.payload_len = plen
        h.nonce = nonce
        h.tag = tag
        h.signature = sig[:sig_len] if sig_len else b""
        return h

    # ------------------------------------------------------------------
    def to_be_signed(self, payload_plain):
        """签名输入: 头部 52 字节 (无签名) + 明文负载."""
        sig_placeholder = b"\x00" * SIG_LEN_P256
        header_no_sig = struct.pack(
            "<IHHHBB4sI12s16sHH64s",
            MAGIC_U32,
            self.version_major, self.version_minor,
            HEADER_LEN,
            self.algo, self.enc,
            b"\x00\x00\x00\x00",
            self.payload_len,
            self.nonce,
            self.tag,
            SIG_LEN_P256, 0,
            sig_placeholder,
        )
        # 截掉签名 64 字节 → 52 字节待签区
        return header_no_sig[:52] + payload_plain

    @classmethod
    def to_be_signed_from_package(cls, package: bytes):
        """从完整 .ydk 包重建签名输入 (验签用)."""
        if len(package) < HEADER_LEN:
            raise ValueError("包过短")
        h = cls.parse(package)
        if len(package) < HEADER_LEN + h.payload_len:
            raise ValueError("包长与 payload_len 不符")
        encrypted = package[HEADER_LEN:HEADER_LEN + h.payload_len]
        return h.to_be_signed(encrypted), encrypted, h


def pack_package(header: FirmwareHeader, encrypted_payload: bytes) -> bytes:
    """组装完整 .ydk 包."""
    header.payload_len = len(encrypted_payload)
    return header.pack() + encrypted_payload
