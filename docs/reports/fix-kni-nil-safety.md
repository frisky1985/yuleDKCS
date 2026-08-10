# Fix: ICCOACodec + ICCECodec Nil Safety

**Date**: 2026-07-18  
**Author**: 小克 (Claude)  
**Verdict**: 小明 🐉 — P1 today  

## Changes

### 1. `codec.go` — nil guards (core)

**ICCOACodec** — added `return nil, fmt.Errorf(...)` before every nil dereference:

| Method | Guard |
|---|---|
| `encodeKeyBind` | `if msg.KeyBind == nil → error` |
| `encodeKeyShare` | `if msg.KeyShare == nil → error` |
| `encodeRemoteControl` | `if msg.RemoteControl == nil → error` |
| `encodeVehicleStatus` | `if msg.VehicleStatus == nil → error` |

**ICCECodec** — same pattern in the `Encode` switch:

| Case | Guard |
|---|---|
| `MsgTypeKeyBind` | `if msg.KeyBind == nil → error` |
| `MsgTypeRemoteControl` | `if msg.RemoteControl == nil → error` |
| `MsgTypeVehicleStatus` | `if msg.VehicleStatus == nil → error` |

Previously these dereferenced `msg.X` directly (e.g. `kb := msg.KeyBind`) without checking if `msg.KeyBind` was nil. A misconfigured caller passing a `UnifiedMessage{Type: MsgTypeKeyBind}` without setting `KeyBind` would panic with nil pointer dereference.

`fmt` was already imported — no import changes needed.

Reference implementation: `CCCCodec.Encode()` already had `if msg.RemoteControl != nil` guarding its RemoteControl branch.

### 2. `unified_edge_test.go` — nil body safety tests

Added two new test functions:

**`TestICCOACodecNilBodySafety`** — 4 subtests (KeyBind/KeyShare/RemoteControl/VehicleStatus)
**`TestICCECodecNilBodySafety`** — 3 subtests (KeyBind/RemoteControl/VehicleStatus)

Each test calls `Encode(&UnifiedMessage{Type: MsgTypeX})` and expects an error (no panic).

### 3. `registry.go` — ToLower check

`registry.go` does **not exist** as a separate file. The `CodecRegistry` is defined inline in `codec.go`. It uses `ProtocolType` (an int-based enum, not strings) as map keys via `map[ProtocolType]MessageCodec`. Since `ProtocolType` is a concrete int type with no string case variance, `ToLower` normalization is **not applicable** — the comparison is purely numeric. No changes needed.

## Verification

```bash
go test -count=1 ./backend/cloud/hub/internal/unified/...  # PASS
go test -count=1 ./backend/cloud/hub/internal/adapter/...  # PASS
go test -count=1 ./backend/cloud/hub/...                   # all packages PASS
go vet ./backend/cloud/hub/...                              # no issues
```

## Summary

| Metric | Value |
|---|---|
| Files changed | 2 (codec.go, unified_edge_test.go) |
| Lines added (code) | 14 |
| Lines added (tests) | 62 |
| Tests added | 7 subtests (4 ICCOA + 3 ICCE) |
| Panic-fix coverage | 100% of Encode nil-deref paths |
