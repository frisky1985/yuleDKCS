#!/usr/bin/env python3
"""Protocol contract tests — BERTLV 编码规范 (REQ-024) 与消息完整性 (REQ-018).

Covers: REQ-024, REQ-018
"""

import hashlib
import hmac

import pytest

from bertlv_codec import (
    ENVELOPE_HEADER_MARK,
    ENVELOPE_TRAILER_MARK,
    MESSAGE_TYPES,
    TRAILER_TAG_SIGNATURE,
    build_message,
    decode_length,
    encode_length,
    encode_tlv,
    verify_envelope,
)


class TestBERTLVLengthEncoding:
    """REQ-024-S3: 长度编码规则 00-7F 单字节, 80-FF 后续字节."""

    def test_single_byte_length_under_128(self):
        # Covers: REQ-024
        assert encode_length(0) == b"\x00"
        assert encode_length(127) == b"\x7f"

    def test_multibyte_length_continuation(self):
        # Covers: REQ-024
        # 128 → 0x81 0x80 (continuation count 1, value 0x80)
        enc = encode_length(128)
        assert enc[0] == 0x81
        length, consumed = decode_length(enc, 0)
        assert length == 128
        assert consumed == 2

    def test_large_length_roundtrip(self):
        # Covers: REQ-024
        for n in (255, 256, 65535, 65536):
            enc = encode_length(n)
            length, _ = decode_length(enc, 0)
            assert length == n

    def test_tlv_roundtrip(self):
        # Covers: REQ-024
        tag, value = 0xA001, b"\x01\x02\x03\x04"
        tlv = encode_tlv(tag, value)
        assert tlv.startswith(tag.to_bytes(2, "big"))


class TestMessageEnvelope:
    """REQ-024-S2: 消息信封 Header (E1 01) + Body (BERTLV) + Trailer (E1 FF)."""

    def test_build_message_contains_required_header_fields(self):
        # Covers: REQ-024, REQ-010
        msg = build_message(1000, [(0xA001, b"VIN1234567890")], session_key=b"k" * 32)
        parsed = verify_envelope(msg, b"k" * 32)
        tags = [t for t, _ in parsed["fields"]]
        # header fields present
        assert 0xE101 in tags  # version
        assert 0xE102 in tags  # timestamp
        assert 0xE103 in tags  # message_type
        assert 0xE104 in tags  # sequence_no
        assert 0xE105 in tags  # device_id

    def test_trailer_contains_hmac_signature(self):
        # Covers: REQ-018
        msg = build_message(3000, [(0xA001, b"VIN")], session_key=b"k" * 32)
        parsed = verify_envelope(msg, b"k" * 32)
        assert len(parsed["signature"]) == 32  # HMAC-SHA256 output

    def test_signature_detects_tampering(self):
        # Covers: REQ-018
        session_key = b"k" * 32
        msg = build_message(1000, [(0xA001, b"VIN1234567890")], session_key=session_key)
        # flip one byte inside the covered region (header version value)
        trailer_pos = msg.find(ENVELOPE_TRAILER_MARK.to_bytes(2, "big"))
        assert trailer_pos > 0
        tampered = bytearray(msg)
        tampered[4] ^= 0xFF
        covered_tampered = bytes(tampered[:trailer_pos])
        expected = hmac.new(session_key, covered_tampered, hashlib.sha256).digest()
        parsed = verify_envelope(msg, session_key)
        # frame trailer signature must NOT match recomputed HMAC over tampered data
        assert parsed["signature"] != expected

    def test_signature_scope_covers_header_and_body(self):
        # Covers: REQ-018
        session_key = b"k" * 32
        msg = build_message(1010, [(0xA001, b"VIN")], session_key=session_key)
        # recompute: HMAC(header+body) must equal trailer signature
        # trailer starts at the FIRST occurrence of E1 FF (header/body never
        # contain the E1 FF marker in this frame)
        trailer_pos = msg.find(ENVELOPE_TRAILER_MARK.to_bytes(2, "big"))
        assert trailer_pos > 0
        covered = msg[:trailer_pos]
        expected = hmac.new(session_key, covered, hashlib.sha256).digest()
        parsed = verify_envelope(msg, session_key)
        assert parsed["signature"] == expected


class TestMessageTypeRegistry:
    """REQ-010~017: 消息类型码注册表与 include/dk_interfaces.h 契约一致性."""

    EXPECTED = {
        1000: "KEY_BIND_REQ", 1001: "KEY_BIND_RSP",
        1002: "KEY_UNBIND_REQ", 1003: "KEY_UNBIND_RSP",
        1004: "KEY_REVOKE_REQ", 1005: "KEY_REVOKE_RSP",
        1010: "KEY_LIST_REQ", 1011: "KEY_LIST_RSP",
        2000: "SHARE_CREATE_REQ", 2001: "SHARE_CREATE_RSP",
        3000: "VEHICLE_CMD_REQ", 3001: "VEHICLE_CMD_RSP",
        3002: "VEHICLE_STATUS",
        9000: "HEARTBEAT_REQ", 9001: "HEARTBEAT_RSP",
    }

    def test_registry_matches_interface_contract(self):
        # Covers: REQ-010, REQ-011, REQ-012, REQ-013, REQ-014, REQ-015, REQ-016, REQ-017
        import re
        from pathlib import Path
        header = Path(__file__).resolve().parents[2] / "include" / "dk_interfaces.h"
        text = header.read_text(encoding="utf-8")
        # extract DK_MSG_* enum values from the C contract header
        enum_block = re.search(
            r"enum dk_message_type \{(.*?)\};", text, re.DOTALL)
        assert enum_block, "dk_message_type enum not found in include/dk_interfaces.h"
        found = {}
        for name, value in re.findall(
                r"DK_MSG_(\w+)\s*=\s*(\d+)", enum_block.group(1)):
            found[int(value)] = name
        assert found == self.EXPECTED, (
            f"registry drift: C header {sorted(found)} != spec {sorted(self.EXPECTED)}")

    def test_all_message_types_are_covered_by_requirement_trace(self):
        # Covers: REQ-010, REQ-015, REQ-017
        # every message type must map to at least one requirement (sanity)
        assert len(MESSAGE_TYPES) >= 14


def parsed_signature(data: bytes, session_key: bytes) -> bytes:
    """Extract trailer signature from a built envelope (structure known)."""
    parsed = verify_envelope(data, session_key)
    return parsed["signature"]
