---
name: agent-security-harness
description: >-
  Security test AI agent systems against protocol-level attacks.
  Use when: (1) testing MCP servers for tool poisoning, capability escalation, or protocol downgrade,
  (2) testing A2A agents for card spoofing, task hijacking, or context leakage,
  (3) testing L402/x402 payment flows for authorization bypass or receipt manipulation,
  (4) running pre-deployment adversarial testing for AIUC-1 certification readiness (B001, C010, D004),
  (5) enterprise platform security testing across 20 platforms,
  (6) full APT simulation with GTG-1002,
  (7) false positive rate testing (over-refusal),
  (8) supply chain provenance and attestation testing (CVE-2026-25253),
  (9) jailbreak resistance testing (DAN, token smuggling, authority impersonation).
  NOT for: model-layer testing (use Garak), identity/access policy enforcement (use MS Agent Governance),
  or static code scanning.
homepage: https://github.com/msaleme/red-team-blue-team-agent-fabric
metadata: {"openclaw":{"emoji":"🛡️","requires":{"bins":["python3","pip","agent-security"],"python":">=3.10"},"tags":["security","testing","mcp","a2a","l402","x402","agent-security","nist","owasp","red-team","aiuc-1","pre-certification","provenance","jailbreak","over-refusal"]}}
---

# Agent Security Harness

**332 security tests** across **24 modules** for AI agent systems. 4 wire protocols (MCP, A2A, L402, x402), 20+ enterprise platforms, GTG-1002 APT simulation, false positive rate testing, supply chain provenance, jailbreak resistance, AIUC-1 certification prep. Zero external dependencies for core protocol modules.

**Current version: v3.8.1** | [PyPI](https://pypi.org/project/agent-security-harness/) | [GitHub](https://github.com/msaleme/red-team-blue-team-agent-fabric) | Apache 2.0

**New in v3.8.1:** MCP Server (expose harness as MCP tools for any AI agent), Attestation Registry (opt-in, Ed25519 signed), Telemetry (opt-in, GDPR compliant), GitHub Action for CI/CD, Free MCP Security Scan, AIUC-1 Certification Prep, Monthly Security Report pipeline, Discord Scan Bot. Validated at 97.9% pass rate (HRAO-E, 146 tests, Wilson 95% CI [0.943, 0.994]). 22 rounds of critical evaluation, 10/10 final score.

## Safety

**Non-destructive by default.** All 332 tests send crafted inputs and analyze responses. No tests modify target state, delete data, or execute write operations.

**Do NOT run against production systems without explicit authorization.** Use isolated staging/test environments and test accounts, especially for payment endpoints (L402, x402).

**Payment tests (L402/x402):** Send crafted payment challenges and analyze responses. They do NOT execute real transactions, transfer funds, or interact with live payment networks.

## Required Environment

**Python 3.10+** and **pip** are required.

**Environment variables:**

| Variable | Required | Purpose |
|---|---|---|
| (none by default) | - | Most tests only need a target URL passed via CLI `--url` flag |
| `PLATFORM_API_KEY` | Only for enterprise adapter tests | Platform-specific API key (SAP, Salesforce, Workday, etc.) - use scoped test credentials only |
| `ALPACA_PAPER_API_KEY` | No | Only for trading-related integration tests |

**No environment variables are required for standard protocol testing (MCP, A2A, L402, x402, over-refusal, provenance, jailbreak).** The target URL is passed as a CLI argument, not an environment variable.

**Credential guidance:** If you use enterprise adapter tests that require API keys, store credentials securely using environment variables or `.env` files. Never commit API keys to version control. Only provide scoped test credentials, never production keys.

## Install

```bash
# Install from PyPI (pinned version recommended)
pip install agent-security-harness==3.8.1

# Verify installation
agent-security version
# Expected output: 3.8.1
```

**Source verification:**
- GitHub: [msaleme/red-team-blue-team-agent-fabric](https://github.com/msaleme/red-team-blue-team-agent-fabric) (Apache 2.0 license, git-tagged releases)
- PyPI: [agent-security-harness](https://pypi.org/project/agent-security-harness/) (v3.8.1)
- Verify release provenance: `git log --tags --oneline` against the repo

**Dependencies:** Core protocol modules (MCP, A2A, L402, x402, over-refusal, provenance, jailbreak) use Python stdlib only (zero external dependencies). Application-layer suite requires `requests` and `geopy`.

## Quick Reference

```bash
# List all harnesses and tests
agent-security list
agent-security list mcp

# Test an MCP server (requires only a URL)
agent-security test mcp --transport http --url http://localhost:8080/mcp

# Test an A2A agent
agent-security test a2a --url https://agent.example.com

# Test L402 payment endpoint (Lightning) - non-destructive
agent-security test l402 --url https://l402-endpoint.com

# Test x402 payment endpoint (Coinbase/USDC) - non-destructive
agent-security test x402 --url https://x402-endpoint.com

# Test x402 with specific paid endpoint path
agent-security test x402 --url https://apibase.pro --paid-path /api/v1/tools/geo.geocode/call

# Test false positive rate (over-refusal)
agent-security test over-refusal --url http://localhost:8080/mcp

# Test supply chain provenance and attestation
agent-security test provenance --url http://localhost:8080/mcp

# Test jailbreak resistance
agent-security test jailbreak --url http://localhost:8080/mcp

# Test capability profile boundaries
agent-security test capability-profile --url https://agent.example.com

# Test harmful output safeguards
agent-security test harmful-output --url https://agent.example.com

# Test CBRN content prevention
agent-security test cbrn --url https://agent.example.com

# Test incident response readiness
agent-security test incident-response --url https://agent.example.com

# Statistical confidence intervals (NIST AI 800-2 aligned)
agent-security test mcp --url http://localhost:8080/mcp --trials 10

# Rate-limit for production endpoints (milliseconds between tests)
agent-security test a2a --url https://agent.example.com --delay 1000

# Try without a server (bundled mock MCP server)
python -m testing.mock_mcp_server  # Terminal 1: starts on port 8402
agent-security test mcp --transport http --url http://localhost:8402/mcp  # Terminal 2
```

## MCP Server Mode

Use the harness as an MCP tool that any AI agent can call:

```bash
# stdio (for Cursor, Claude Desktop)
python -m mcp_server

# HTTP
python -m mcp_server --transport http --port 8400
```

Tools: `scan_mcp_server` (quick scan), `full_security_audit` (332 tests), `aiuc1_readiness`, `get_test_catalog`, `validate_attestation`.

## Harness Modules (24 modules, 332 tests)

| Command | Tests | What It Tests |
|---|---|---|
| `test mcp` | 13 | MCP wire-protocol (JSON-RPC 2.0): tool poisoning, capability escalation, protocol downgrade, resource traversal, sampling hijack, context displacement |
| `test a2a` | 12 | A2A protocol: Agent Card spoofing, task injection, push notification redirect, skill injection, context isolation |
| `test l402` | 14 | L402 payments: macaroon tampering, preimage replay, caveat escalation, invoice validation |
| `test x402` | 25 | x402 payments: recipient manipulation, session theft, facilitator trust, cross-chain confusion, spending limits, health checks. Includes Agent Autonomy Risk Score (0-100) |
| `test enterprise` | 31 | Tier 1 enterprise: SAP, Salesforce, Workday, Oracle, ServiceNow, Microsoft, Google, Amazon, OpenClaw |
| `test extended-enterprise` | 27 | Tier 2 enterprise: IBM Maximo, Snowflake, Databricks, Pega, UiPath, Atlassian, Zendesk, IFS, Infor, HubSpot, Appian |
| `test framework` | 11 | Framework adapters: LangChain, CrewAI, AutoGen, OpenAI Agents SDK, Bedrock |
| `test identity` | 18 | NIST NCCoE Agent Identity: identification, authentication, authorization, auditing, data flow, standards compliance |
| `test gtg1002` | 17 | GTG-1002 APT simulation: 6 campaign phases + hallucination detection |
| `test advanced` | 10 | Advanced patterns: polymorphic injection, stateful escalation, multi-domain chains, jailbreak persistence |
| `test over-refusal` | 25 | False positive rate: legitimate requests across all protocols that should NOT be blocked. Measures FPR with Wilson CI |
| `test provenance` | 15 | Supply chain: fake provenance, spoofed attestation, marketplace integrity, CVE-2026-25253 attack patterns |
| `test jailbreak` | 25 | Jailbreak resistance: DAN variants, token smuggling, authority impersonation, context manipulation, persistence |
| `test return-channel` | 8 | Return channel poisoning: output injection, ANSI escape, context overflow, encoded smuggling, structured data poisoning |
| `test capability-profile` | 10 | Executor capability boundary validation, profile escalation prevention |
| `test harmful-output` | 10 | Toxicity, bias, scope violations, deception (AIUC-1 C003/C004) |
| `test cbrn` | 8 | Chemical/biological/radiological/nuclear content safeguards (AIUC-1 F002) |
| `test incident-response` | 8 | Alert triggering, kill switch, log completeness, recovery (AIUC-1 E001-E003) |
| `test aiuc1` | 12 | AIUC-1 compliance: all 24 certification requirements mapped |
| `test cloud` | 25 | Cloud agent platforms: AWS Bedrock, Azure AI, GCP Vertex, Anthropic, OpenAI |
| `test cve-2026` | 8 | CVE-2026-25253 reproduction: supply chain tool poisoning at scale |

## CI/CD Integration (v3.8+)

```yaml
# GitHub Action - drop into any workflow
- uses: msaleme/red-team-blue-team-agent-fabric@v3.8
  with:
    target_url: http://localhost:8080/mcp
```

```bash
# Free quick scan (5 tests, A-F grade)
python scripts/free_scan.py --url http://server:port/mcp --format markdown

# AIUC-1 certification readiness report
python scripts/aiuc1_prep.py --url http://server:port --simulate

# Monthly security report across multiple targets
python scripts/monthly_security_report.py
```

## Output Format

All harnesses produce JSON reports with:
- Pass/fail per test with test ID and OWASP ASI mapping
- Full request/response transcripts for audit
- Elapsed time per test
- Wilson score confidence intervals (with `--trials N`)
- x402 harness adds: CSG mapping, financial impact estimation, Agent Autonomy Risk Score

## When to Use Each Harness

- **Building an MCP server?** Run `test mcp` before deploying
- **Exposing an A2A agent?** Run `test a2a` to check Agent Card and task security
- **Adding agent payments?** Run `test l402` (Lightning) or `test x402` (USDC) before going live
- **Deploying on enterprise platforms?** Run `test enterprise` with your platform name
- **Red-teaming an agent system?** Run `test gtg1002` for full APT campaign simulation
- **Need compliance evidence?** Use `--trials 10` for NIST AI 800-2 aligned statistical reports
- **Preparing for AIUC-1 certification?** Run all harnesses for B001/C010/D004 evidence
- **Checking false positive rate?** Run `test over-refusal` to verify security controls don't break legitimate use
- **Validating supply chain integrity?** Run `test provenance` (especially relevant after CVE-2026-25253)
- **Testing jailbreak resistance?** Run `test jailbreak` for DAN variants and encoding evasion
- **Checking agent capability boundaries?** Run `test capability-profile` to verify escalation prevention
- **Validating safety controls?** Run `test harmful-output` and `test cbrn` for content safeguards
- **Testing incident response?** Run `test incident-response` for kill switch and recovery validation

## Research

This harness is part of a published research program on autonomous AI agent governance:

- [Detecting Normalization of Deviance in Multi-Agent Systems](https://doi.org/10.5281/zenodo.19195516) (DOI: 10.5281/zenodo.19195516) - Empirical evidence that gateway defenses provide no measurable mitigation for agent protocol attacks
- [Constitutional Self-Governance for Autonomous AI Agents](https://doi.org/10.5281/zenodo.19162104) (DOI: 10.5281/zenodo.19162104) - Governance framework for agent decisions, 77 days production data
- [Decision Load Index](https://doi.org/10.5281/zenodo.18217577) (DOI: 10.5281/zenodo.18217577) - Measuring cognitive burden of AI agent oversight

## Source & Provenance

- **Repo:** [github.com/msaleme/red-team-blue-team-agent-fabric](https://github.com/msaleme/red-team-blue-team-agent-fabric) (Apache 2.0)
- **PyPI:** [pypi.org/project/agent-security-harness](https://pypi.org/project/agent-security-harness/) (v3.8.1)
- **Author:** Michael K. Saleme ([Signal Lab](https://msaleme.github.io/aiuc1-readiness/))
- **CI:** Tested on Python 3.10/3.11/3.12, self-tests validating harness correctness
- **Security policy:** [SECURITY_POLICY.md](https://github.com/msaleme/red-team-blue-team-agent-fabric/blob/main/SECURITY_POLICY.md) - threat model for contributions to security testing frameworks
