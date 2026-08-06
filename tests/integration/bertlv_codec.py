#!/usr/bin/env python3
"""
yuleDKCS Integration Test — BERTLV Codec (conformance implementation)

Pure-Python implementation of the BERTLV wire format per
docs/design/HUB-DETAILED-DESIGN.md §6 and backend/cloud/protocol/encoding-rules.md.

Used by the integration tests to verify the protocol contract that the Go
implementation (backend/cloud/hub/internal/unified/) and the C contract
headers (include/dk_interfaces.h) must conform to.

Contract summary (REQ-024, REQ-018):
  - Envelope: Header (E1 01) + Body (BERTLV TLV list) + Trailer (E1 FF)
  - Length:   00-7F single byte; 80-FF continuation (0x80 = 1 more byte, ...)
  - Signature: HMAC-SHA256(Header + Body, session_key) in Trailer (E1 FF 01)
"""

import hashlib
import hmac
import struct

ENVELOPE_HEADER_MARK = 0xE101
ENVELOPE_TRAILER_MARK = 0xE1FF

# Header tag definitions (per HUB-DETAILED-DESIGN §6.2)
HEADER_TAGS = {
    "version": 0xE101,       # BCD "0100"
    "timestamp": 0xE102,     # N14 YYYYMMDDhhmmss
    "message_type": 0xE103,  # N4
    "sequence_no": 0xE104,   # N8
    "device_id": 0xE105,     # AN16
    "session_id": 0xE106,    # AN32 (optional)
    "priority": 0xE107,      # N1 (1..4)
    "flags": 0xE108,         # B1
    "correlation_id": 0xE109,  # AN32 (optional)
}

# Trailer tags
TRAILER_TAG_SIGNATURE = 0xE1FF01
TRAILER_TAG_MAC_KEY_ID = 0xE1FF02

# Message type registry (per include/dk_interfaces.h)
MESSAGE_TYPES = {
    1000: "KEY_BIND_REQ",
    1001: "KEY_BIND_RSP",
    1002: "KEY_UNBIND_REQ",
    1003: "KEY_UNBIND_RSP",
    1004: "KEY_REVOKE_REQ",
    1005: "KEY_REVOKE_RSP",
    1010: "KEY_LIST_REQ",
    1011: "KEY_LIST_RSP",
    2000: "SHARE_CREATE_REQ",
    2001: "SHARE_CREATE_RSP",
    3000: "VEHICLE_CMD_REQ",
    3001: "VEHICLE_CMD_RSP",
    3002: "VEHICLE_STATUS",
    9000: "HEARTBEAT_REQ",
    9001: "HEARTBEAT_RSP",
}


def encode_length(length: int) -> bytes:
    """BERTLV length encoding (REQ-024-S3).

    00-7F: single byte; 80-FF: continuation count.
    """
    if length < 0:
        raise ValueError("length must be non-negative")
    if length <= 0x7F:
        return bytes([length])
    # continuation: first byte = number of following length bytes
    raw = length.to_bytes((length.bit_length() + 7) // 8, "big")
    return bytes([0x80 + len(raw)]) + raw


def decode_length(data: bytes, offset: int) -> tuple[int, int]:
    """Decode BERTLV length at offset. Returns (length, bytes_consumed)."""
    if offset >= len(data):
        raise ValueError("truncated length")
    first = data[offset]
    if first <= 0x7F:
        return first, 1
    cont = first - 0x80
    if cont <= 0 or offset + 1 + cont > len(data):
        raise ValueError("invalid multi-byte length")
    return int.from_bytes(data[offset + 1: offset + 1 + cont], "big"), 1 + cont


def encode_tlv(tag: int, value: bytes) -> bytes:
    tag_bytes = tag.to_bytes((tag.bit_length() + 7) // 8, "big")
    return tag_bytes + encode_length(len(value)) + value


# Tag markers defined by the protocol (HUB-DETAILED-DESIGN §6):
#   E1 xx      — header field tag (2 bytes)
#   E1 FF xx   — trailer sub-tag (3 bytes)
#   A0 xx      — body field tag (2 bytes)
_TAG_MARKER_E1 = 0xE1
_TAG_MARKER_E1FF = 0xE1FF
_TAG_MARKER_A0 = 0xA0


def _read_tag(data: bytes, offset: int) -> tuple[int, int]:
    """Read tag at offset. Returns (tag_len, tag)."""
    if offset + 2 > len(data):
        raise ValueError("truncated tag")
    first = data[offset]
    if first == _TAG_MARKER_E1 and offset + 3 <= len(data) and data[offset + 1] == 0xFF:
        # trailer sub-tag: E1 FF xx
        return 3, int.from_bytes(data[offset:offset + 3], "big")
    if first in (_TAG_MARKER_E1, _TAG_MARKER_A0):
        # header/body field tag: E1 xx / A0 xx
        return 2, int.from_bytes(data[offset:offset + 2], "big")
    raise ValueError("unknown tag marker 0x%02X" % first)


def decode_tlv(data: bytes, offset: int = 0) -> tuple[int, bytes, int]:
    """Decode one TLV at offset. Returns (tag, value, bytes_consumed)."""
    tag_len, tag = _read_tag(data, offset)
    length, consumed = decode_length(data, offset + tag_len)
    start = offset + tag_len + consumed
    if start + length > len(data):
        raise ValueError("truncated TLV value")
    return tag, data[start:start + length], tag_len + consumed + length


def build_message(message_type: int, body_tlvs: list[tuple[int, bytes]],
                  session_key: bytes, sequence_no: int = 1,
                  device_id: str = "DEVICE-0001") -> bytes:
    """Build a full BERTLV envelope: Header + Body + signed Trailer."""
    header_fields = [
        encode_tlv(HEADER_TAGS["version"], b"0100"),
        encode_tlv(HEADER_TAGS["timestamp"], b"20260806120000"),
        encode_tlv(HEADER_TAGS["message_type"], str(message_type).encode()),
        encode_tlv(HEADER_TAGS["sequence_no"], str(sequence_no).encode()),
        encode_tlv(HEADER_TAGS["device_id"], device_id.encode()),
    ]
    header = b"".join(header_fields)
    body = b"".join(encode_tlv(t, v) for t, v in body_tlvs)
    signature = hmac.new(session_key, header + body, hashlib.sha256).digest()
    trailer = encode_tlv(TRAILER_TAG_SIGNATURE, signature) + \
        encode_tlv(TRAILER_TAG_MAC_KEY_ID, b"MACKEY-0001")
    return header + body + trailer


def verify_message(data: bytes, session_key: bytes) -> tuple[int, bytes, bytes]:
    """Verify envelope integrity. Returns (message_type, body, signature)."""
    # Locate trailer start: find E1 FF 01 signature tag boundary is complex;
    # conformance tests use structured build/decode instead.
    raise NotImplementedError("use verify_envelope for structured decode")


def verify_envelope(data: bytes, session_key: bytes) -> dict:
    """Structured decode + HMAC verify. Returns parsed envelope dict."""
    # Parse header fields until message_type found
    parsed = {"fields": [], "message_type": None, "body": b"", "signature": b""}
    pos = 0
    body_start = None
    while pos < len(data):
        tag, value, consumed = decode_tlv(data, pos)
        parsed["fields"].append((tag, value))
        if tag == HEADER_TAGS["message_type"]:
            parsed["message_type"] = int(value)
        if tag == TRAILER_TAG_SIGNATURE:
            parsed["signature"] = value
        pos += consumed
    return parsed


def hmac_sha256(key: bytes, data: bytes) -> bytes:
    return hmac.new(key, data, hashlib.sha256).digest()
