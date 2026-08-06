#!/usr/bin/env python3
"""ASPICE 证据自检测试 — 校验 SRS/追溯/合格性证据产物的结构一致性.

本文件是证据链完整性守护测试（SWE.4.BP1/BP3、SWE.3.BP2 的单元测试证据之一），
校验:
  1. SRS (docs/software-requirements.md) 每条 REQ-xxx 唯一且含 SHALL 语句
  2. 机器可读表 (specs/requirements-shall-table.md) 可被追溯工具解析且覆盖全部 REQ
  3. 每条 REQ 至少被一个测试文件 Covers（需求→测试 单向追溯完整性）
  4. 合格性策略 (docs/qualification-strategy.md) 覆盖全部 REQ

Covers: REQ-001, REQ-021, REQ-024
"""

import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]

REQ_PATTERN = re.compile(r"\bREQ-(\d{3})\b")


def _load_srs() -> str:
    return (PROJECT_ROOT / "docs" / "software-requirements.md").read_text(encoding="utf-8")


def _load_shall_table() -> str:
    return (PROJECT_ROOT / "specs" / "requirements-shall-table.md").read_text(encoding="utf-8")


def _all_req_ids() -> set[str]:
    """REQ-xxx IDs from the machine-readable SHALL table (authoritative)."""
    text = _load_shall_table()
    ids = set()
    for m in REQ_PATTERN.finditer(text):
        num = int(m.group(1))
        if 1 <= num <= 40:
            ids.add(f"REQ-{num:03d}")
    return ids


class TestSRSStructure:
    """SRS 结构完整性: 唯一标识 + SHALL 语句 (SWE.1.BP1)."""

    def test_srs_has_unique_req_ids(self):
        # Covers: REQ-001
        srs = _load_srs()
        ids = REQ_PATTERN.findall(srs)
        # SRS contains each REQ-xxx as a section header; headers must be unique
        headers = re.findall(r"^#### REQ-(\d{3}):", srs, re.MULTILINE)
        assert len(headers) == 40, f"expected 40 REQ headers in SRS, got {len(headers)}"
        assert len(set(headers)) == 40, "duplicate REQ headers found"

    def test_each_requirement_has_shall_statements(self):
        # Covers: REQ-001
        srs = _load_srs()
        sections = re.split(r"^#### REQ-\d{3}:", srs, flags=re.MULTILINE)[1:]
        assert len(sections) == 40
        missing = [
            idx for idx, sec in enumerate(sections, start=1)
            if "SHALL" not in sec
        ]
        assert not missing, f"requirements without SHALL statements: {missing}"

    def test_srs_matches_shall_table(self):
        # Covers: REQ-001, REQ-024
        srs_ids = set(re.findall(r"^#### (REQ-\d{3}):", _load_srs(), re.MULTILINE))
        table_ids = _all_req_ids()
        assert srs_ids == table_ids, (
            f"SRS/table drift: only-in-SRS={srs_ids - table_ids}, "
            f"only-in-table={table_ids - srs_ids}")


class TestTraceabilityCompleteness:
    """需求→测试 追溯完整性 (SWE.4.BP2 / SWE.6.BP3)."""

    def test_every_requirement_has_covers_in_tests(self):
        # Covers: REQ-021
        req_ids = _all_req_ids()
        # collect Covers: markers from all python tests
        covered: set[str] = set()
        for tf in (PROJECT_ROOT / "tests").rglob("test_*.py"):
            text = tf.read_text(encoding="utf-8", errors="replace")
            for m in REQ_PATTERN.finditer(text):
                covered.add(m.group(0))
        missing = sorted(req_ids - covered)
        assert not missing, (
            f"requirements without any Covers marker in tests: {missing}\n"
            f"covered={len(covered)}/{len(req_ids)}")

    def test_traceability_matrix_generated(self):
        # Covers: REQ-021
        ev = PROJECT_ROOT / ".osh" / "evidence"
        matrix = ev / "traceability-matrix.md"
        assert matrix.exists(), "traceability-matrix.md missing — run yuleosh traceability matrix"
        assert matrix.stat().st_size > 1000


class TestQualificationCoverage:
    """合格性策略覆盖全部需求 (SWE.6.BP1)."""

    def test_qualification_strategy_covers_all_reqs(self):
        # Covers: REQ-024
        qs = (PROJECT_ROOT / "docs" / "qualification-strategy.md").read_text(
            encoding="utf-8", errors="replace")
        req_ids = _all_req_ids()
        missing = [r for r in sorted(req_ids) if r not in qs]
        assert not missing, f"qualification-strategy.md missing reqs: {missing}"

    def test_acceptance_matrix_exists(self):
        # Covers: REQ-024
        ev = PROJECT_ROOT / ".osh" / "evidence"
        assert (ev / "acceptance-matrix.md").exists()
        assert (ev / "requirement-coverage.md").exists()
