# Hub Merge Report

**Date**: 2026-07-16  
**Task**: yuleDKCS P1-1 — Merge dual hub directories  
**Merged into**: `backend/cloud/hub/`  

## Background

`backend/hub/` and `backend/cloud/hub/` had ~40% code duplication. The `cloud/hub` version was the newer iteration with refactored `internal/` structure and cloud-native deployment patterns.

## What Was Done

### 1. Duplicate code resolution

| backend/hub/pkg/ → | backend/cloud/hub/internal/ | Action |
|---|---|---|
| `pkg/adapter/` | `internal/adapter/` | Kept `internal/` version |
| `pkg/codec/bertlv/` | `internal/codec/bertlv/` | Kept `internal/` version |
| `pkg/errors/` | `internal/error/` | Kept `internal/` version |
| `pkg/gateway/` | `internal/gateway/` | Kept `internal/` version |
| `pkg/logger/` | `internal/logger/` | Kept `internal/` version |
| `pkg/service/` | `internal/service/` | Kept `internal/` version |
| `pkg/telemetry/` | `internal/telemetry/` | Kept `internal/` version |
| `pkg/token/` | `internal/token/` | Kept `internal/` version |
| `pkg/unified/` | `internal/unified/` | Kept `internal/` version |
| `security/` | `internal/security/` | Merged (see below) |
| `pin/` | `internal/diagnostics/` | Merged (see below) |

### 2. Unique code from `backend/hub/` moved into `backend/cloud/hub/`

| Directory | Files | Notes |
|---|---|---|
| `device/` | registry.go, store.go, types.go, device_test.go | Device management layer |
| `migrations/` | 001_init.sql, 002_seed_data.sql, run_migrations.go | DB migration runner |
| `oem/` | tenant.go, types.go, oem_test.go | OEM multi-tenant interface |
| `oms/` | deployment.go, lifecycle.go, monitoring.go, provisioning.go, store.go, types.go, doc.go, oms_test.go | Key lifecycle management |
| `protocol/` | adapter.go, bridge.go, ccc_adapter.go, icce_adapter.go, iccoa_adapter.go, doc.go, protocol_test.go | Protocol bridge layer |
| `run/` | executor.go, runner.go, scenario.go, store.go, types.go, report.go, doc.go, e2e_test.go, phase3_integration_test.go | E2E test runner |
| `tune/` | calibrator.go, optimizer.go, profile.go, store.go, types.go, doc.go, tune_test.go | Performance tuning |
| `api/v1/handler_coverage_test.go` | → `api/v1/` | Additional coverage tests |
| `security/store_inmemory.go` | → `internal/security/` | In-memory event store |
| `security/security_test.go` | → `internal/security/` | Security monitor tests |
| `pin/pin_test.go` | → `internal/diagnostics/` | PIN diagnostics tests (renamed `package pin` → `package diagnostics`) |
| `tests/integration/hub_integration_test.go` | → `tests/integration/` | Integration tests |

### 3. Import path updates

All `backend/hub/...` import paths updated to `backend/cloud/hub/...` in:
- `tests/integration/hub_integration_test.go` — import path + hardcoded directory path
- `migrations/run_migrations.go` — hardcoded path `backend/hub/` → `backend/cloud/hub/`

### 4. Package declaration fixes

- `pin/pin_test.go` — changed `package pin` to `package diagnostics` to match target directory

### 5. Configuration updates

| File | Change |
|---|---|
| `go.work` | Removed `backend/hub` from `use ()` |
| `backend/cloud/hub/go.mod` | Added `github.com/lib/pq v1.12.3` (required by migrations) |
| `backend/cloud/hub/tests/stress/go.mod` | Removed stale `replace` directive pointing to `../../../../hub` |
| `backend/cloud/hub/tests/integration/go.mod` | Updated `replace` to point `../` (parent module) |
| `scripts/traceability_report.go` | Updated 11 file path references from `backend/hub/run/` to `backend/cloud/hub/run/` |

### 6. Removed

- Entire `backend/hub/` directory (all files + go.mod + go.sum)

## Verification

### ✅ `go build ./...`
All packages compile without errors.

### ✅ `go vet ./...`
All packages pass vet checks.

### ✅ `go test ./...` — 24 packages, all passing

| Package | Status |
|---|---|
| `api/v1` | OK |
| `device` | OK |
| `internal/adapter` | OK |
| `internal/codec/bertlv` | OK |
| `internal/diagnostics` | OK |
| `internal/gateway` | OK |
| `internal/security` | OK |
| `internal/service` | OK |
| `internal/token` | OK |
| `internal/unified` | OK |
| `oem` | OK |
| `oms` | OK |
| `protocol` | OK |
| `run` | OK |
| `tests/compliance/ccc` | OK |
| `tests/compliance/icce` | OK |
| `tests/compliance/iccoa` | OK |
| `tune` | OK |

## Final Structure

```
backend/cloud/hub/
├── api/
│   ├── dk/v1/
│   └── v1/               # ← handler_coverage_test.go added
├── cmd/
│   ├── hub/
│   └── yuledkcs/
├── device/                # ← moved from backend/hub
├── internal/
│   ├── adapter/
│   ├── codec/bertlv/
│   ├── diagnostics/       # ← pin_test.go added from hub pin/
│   ├── error/
│   ├── gateway/
│   ├── logger/
│   ├── security/          # ← store_inmemory.go + security_test.go added
│   ├── service/
│   ├── telemetry/
│   ├── token/
│   └── unified/
├── migrations/            # ← moved from backend/hub
├── oem/                   # ← moved from backend/hub
├── oms/                   # ← moved from backend/hub
├── protocol/              # ← moved from backend/hub
├── run/                   # ← moved from backend/hub
├── tests/
│   ├── compliance/
│   ├── integration/
│   └── stress/
└── tune/                  # ← moved from backend/hub
```

## Notes

- The `cmd/hub/main.go` from `backend/hub/` was **not** merged into the cloud version. The cloud version's `cmd/hub/main.go` has a different startup architecture (mode-based: `all-in-one`, `hub-only`, `server-only`). The old hub main's TLS and configurable port features are a potential future enhancement.
- All `pkg/` content from `backend/hub/` was superseded by `internal/` in cloud/hub.
- The `pin/` package was superseded by `internal/diagnostics/` (functionally identical, renamed package).
