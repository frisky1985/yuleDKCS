// Package oms implements the OMS (生命周期管理系统) — Key Lifecycle Management.
//
// ASPICE Alignment:
//   SWE.1 — Software Requirements Analysis: OMS interfaces defined against 银基 OMS requirements.
//   SWE.4 — Software Unit Verification: state-machine transition logic with exhaustive checks.
//   SWE.5 — Software Integration and Integration Test: provisioning → deployment → monitoring pipeline.
//   SWE.6 — Software Qualification Test: lifecycle scenarios covering pre-sale → in-sale → post-sale.
//
// Architecture:
//   oms/  serves as the application-layer lifecycle coordinator. It orchestrates:
//     - Key lifecycle state machine (created → pre_paired → paired → active → suspended/revoked)
//     - Provisioning job management
//     - Deployment (OEM rollout) management
//     - Usage monitoring and statistics
//
// Dependencies:
//   - device pkg  (Device Management): used for device context lookup
//   - security pkg (Security Monitoring): triggered on lifecycle transitions
//
// Layering (outbound → inbound):
//   API handlers (api/) → OMSCoordinator (internal/) → oms/ interfaces + pkg/ backends
//
// Data flow:
//   [售前] CreateKey → [售中] PrePair → Pair → [售后] Activate → Suspend/Revoke/Delete
//
// Key design decisions:
//   - Interface-first: all implementations are swappable (in-memory, PostgreSQL, Redis, etc.)
//   - Every public method takes context.Context for tracing and cancellation
//   - State transitions are explicit (from → to) to prevent accidental mutations
//   - Metadata is typed as map[string]string for flexible extension
package oms
