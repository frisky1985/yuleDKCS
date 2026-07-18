# OpenSpec: yuleDKCS P0 修复 Sprint

> **版本**: 1.0.0  
> **方法**: yuleOSH Pipeline (Superpowers → OpenSpec → Harness Engineering)  
> **日期**: 2026-07-18  
> **诊断依据**: `reports/yuleosh-full-diagnosis.md`

---

## 概述

基于 yuleOSH 全流程诊断发现的 6 个 P0 阻塞项，本 Sprint 将 yuleDKCS 从 CL1 提升到 CL2 基础水准。

### FIX-001: hub/service 补测试 → SWR-HUB-003

Reason: hub/internal/service 有 7 个源文件但零测试，是诊断发现的 P0 阻塞项

<!-- REQ-ID: SWR-HUB-003.1 -->
- The system SHALL add unit tests for `backend/cloud/hub/internal/service/` covering all 7 source files
<!-- REQ-ID: SWR-HUB-003.2 -->
- The system SHALL reach at least 80% test coverage for the service package
<!-- REQ-ID: SWR-HUB-003.5 -->
- The system SHALL use Go standard testing package
<!-- REQ-ID: SWR-HUB-003.6 -->
- The system SHALL NOT modify production code logic

Status: PROPOSED

### FIX-002: hub/logger 补测试 → SWR-HUB-003

Reason: hub/internal/logger 有 1 个源文件但零测试

<!-- REQ-ID: SWR-HUB-003.3 -->
- The system SHALL add unit tests for `backend/cloud/hub/internal/logger/`
<!-- REQ-ID: SWR-HUB-003.4 -->
- The system SHALL reach at least 85% test coverage for the logger package

Status: PROPOSED

### FIX-003: 覆盖率门禁 → SWR-HUB-004

Reason: CI 目前无覆盖率门禁，低覆盖率代码可以合入

<!-- REQ-ID: SWR-HUB-004.1 -->
- The system SHALL enforce a coverage gate at fail-under=60 in CI
<!-- REQ-ID: SWR-HUB-004.2 -->
- The system SHALL fail the CI run when coverage drops below 60%
<!-- REQ-ID: SWR-HUB-004.3 -->
- The system SHALL apply the gate to both backend/dkcs and backend/cloud/hub
<!-- REQ-ID: SWR-HUB-004.4 -->
- The system SHALL implement the gate via go test -coverprofile plus custom shell check

Status: PROPOSED

### FIX-004: 集成测试 CI 化 → SWR-HUB-005

Reason: 集成测试 tests/integration 有骨架但从未在 CI 中执行

<!-- REQ-ID: SWR-HUB-005.7 -->
- The system SHALL run integration tests in backend/cloud/hub/tests/integration as a CI step
<!-- REQ-ID: SWR-HUB-005.8 -->
- The system SHALL run integration tests separately from unit tests
<!-- REQ-ID: SWR-HUB-005.9 -->
- The system SHALL NOT block unit test results on integration test outcome

Status: PROPOSED

### FIX-005: SAST 安全扫描 → SWR-HUB-005

Reason: yuleDKCS 代码安全全靠手写审查，无自动化 SAST

<!-- REQ-ID: SWR-HUB-005.10 -->
- The system SHALL run gosec on all Go code in CI
<!-- REQ-ID: SWR-HUB-005.11 -->
- The system SHALL run security scan as a separate CI step
<!-- REQ-ID: SWR-HUB-005.12 -->
- The system SHALL report all findings in CI output

Status: PROPOSED

### FIX-006: CI 分层 L1/L2/L3 → SWR-HUB-005

Reason: CI 是单一流水线，无分层机制

<!-- REQ-ID: SWR-HUB-005.1 -->
- The system SHALL restructure CI into 3 layers
<!-- REQ-ID: SWR-HUB-005.2 -->
- The system SHALL include unit tests coverage gate and go vet in L1
<!-- REQ-ID: SWR-HUB-005.3 -->
- The system SHALL include integration tests and SAST scan in L2
<!-- REQ-ID: SWR-HUB-005.4 -->
- The system SHALL include full build and docker build in L3
<!-- REQ-ID: SWR-HUB-005.5 -->
- The system SHALL require L1 for merge
<!-- REQ-ID: SWR-HUB-005.6 -->
- The system SHALL run L2 and L3 only after L1 passes

Status: PROPOSED

---

## 场景

### Scenario: 覆盖率门禁生效

- GIVEN a PR is pushed with coverage below 60%
- WHEN CI runs L1
- THEN the CI run SHALL fail
- AND the error message SHALL state the coverage gap

### Scenario: 安全扫描告警

- GIVEN Go source contains a potential security vulnerability
- WHEN CI runs L2
- THEN gosec SHALL report the finding
- AND the CI step SHALL pass with warnings

### Scenario: 三层 CI 触发

- GIVEN a commit is pushed
- WHEN CI starts
- THEN L1 SHALL run first
- AND L2 SHALL run only after L1 passes
- AND L3 SHALL run only after L2 passes
