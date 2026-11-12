#!/usr/bin/env python3
"""G1 台架串口日志分析工具。

按《G1 硬件 Bring-up 作业指导书》约定的日志规则单遍流式解析
`serial_capture.py` 采集的日志，逐行给出结论：

    🟢 正常    🟡 警告/跳过    🔴 问题

并输出按步骤汇总、问题清单、自检统计与退出码，便于人工判读和 CI 门禁。

日志规则（与固件输出保持一致）：
  1. 采集格式: `<ISO8601> | <原始文本>`；无时间戳的原始日志同样支持。
  2. 步骤标记: `===== MARKER <步骤号> <说明> =====` 划分步骤上下文。
  3. Bring-up: `[BRINGUP] NN <名称> PASS|FAIL|SKIP (detail)`；
                `[BRINGUP] RESULT: N passed, M failed, K skipped`。
  4. 级别: `FATAL:`/`ERROR:`/`WARN:`；关键字 FAIL/PANIC/HardFault/ASSERT。
  5. 安全事件: `SECURE_BOOT: FAIL|OK|...`、`VERIFY_*` 失败码、`STATE_LOCKED`。

退出码：0=通过（无问题），1=发现问题（或 --fail-on-warn 且有警告），
        2=参数/读取错误。

示例：
    python3 analyze_serial_log.py bench-evidence/G1-20261112-104500/serial.log
    python3 analyze_serial_log.py serial.log --json > report.json
    python3 analyze_serial_log.py serial.log --fail-on-warn --expect "BRINGUP] RESULT"
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

VERSION = "1.0.0"

SEV_GREEN, SEV_YELLOW, SEV_RED = 0, 1, 2
EMOJI = {SEV_GREEN: "🟢", SEV_YELLOW: "🟡", SEV_RED: "🔴"}
LABEL = {SEV_GREEN: "正常", SEV_YELLOW: "警告", SEV_RED: "问题"}

ANSI = {
    SEV_GREEN: "\033[32m",
    SEV_YELLOW: "\033[33m",
    SEV_RED: "\033[31m",
}
DIM = "\033[2m"
BOLD = "\033[1m"
RESET = "\033[0m"

# ── 规则表：id, 严重级别, 正则 ────────────────────────────────────────────
# 同一行命中多条时取最高级别；`--ignore` 命中的行强制视为正常。
RULES: list[tuple[str, int, re.Pattern[str]]] = [
    ("fatal",         SEV_RED,    re.compile(r"\b(FATAL|PANIC)\b")),
    ("error-level",   SEV_RED,    re.compile(r"\bERROR\b|\[E\]")),
    ("fail-word",     SEV_RED,    re.compile(r"(?i)(?<!no )\bfail(?:ed|ure)?\b")),
    ("hardfault",     SEV_RED,    re.compile(r"(?i)\b(hardfault|assert(?:ion)?(?:\s+failed)?)\b")),
    ("secureboot",    SEV_RED,    re.compile(r"SECURE_BOOT:\s*(FAIL|incomplete)|secure boot verification failed")),
    ("verify-fail",   SEV_RED,    re.compile(r"VERIFY_(?:SIGN_INVALID|CERT_INVALID|FW_TAMPERED|INVALID|FAILED)")),
    ("auth-fail",     SEV_RED,    re.compile(r"(?i)\b(?:authentication|auth)\s+failed\b")),
    ("warn-level",    SEV_YELLOW, re.compile(r"\bWARN(?:ING)?\b|\[W\]")),
    ("skip",          SEV_YELLOW, re.compile(r"\bSKIPPED?\b")),
    ("timeout",       SEV_YELLOW, re.compile(r"(?i)\btime[\s-]?out\b|\btimed out\b")),
    ("locked",        SEV_YELLOW, re.compile(r"\bSTATE_LOCKED\b")),
    ("degraded",      SEV_YELLOW, re.compile(r"(?i)\b(degraded|fallback|not ready)\b")),
]

SUCCESS_RE = re.compile(r"\bPASS\b|:\s*OK\b|\bready\b|\bUNLOCKED\b")

TS_RE = re.compile(r"^(\d{4}-\d{2}-\d{2}T[0-9:.+\-]+)\s*\|\s?(.*)$")
MARKER_RE = re.compile(r"^=+\s*MARKER\s+(\S+)\s*(.*?)\s*=+$")
BRINGUP_RE = re.compile(
    r"\[BRINGUP\]\s+(\d{1,2})\s+(\S+)\s+[.\s]*(PASS|FAIL|SKIP)(?:\s*\(([^)]*)\))?"
)
RESULT_RE = re.compile(
    r"\[BRINGUP\]\s+RESULT:\s*(\d+)\s+passed,\s*(\d+)\s+failed,\s*(\d+)\s+skipped"
)
START_RE = re.compile(r"^=+\s*CAPTURE START\b")


@dataclass
class Entry:
    lineno: int
    ts: str
    body: str
    severity: int
    rules: list[str]
    step: str


@dataclass
class Step:
    label: str
    red: int = 0
    yellow: int = 0
    green: int = 0
    entries: list[Entry] = field(default_factory=list)


@dataclass
class Check:
    ident: str
    name: str
    status: str
    detail: str


def classify(body: str, ignore: list[re.Pattern[str]]) -> tuple[int, list[str]]:
    """返回 (严重级别, 命中规则列表)。"""
    for pat in ignore:
        if pat.search(body):
            return SEV_GREEN, ["ignored"]

    m = RESULT_RE.search(body)
    if m:
        failed, skipped = int(m.group(2)), int(m.group(3))
        if failed:
            return SEV_RED, ["bringup-result"]
        if skipped:
            return SEV_YELLOW, ["bringup-result"]
        return SEV_GREEN, ["bringup-result"]

    m = BRINGUP_RE.search(body)
    if m:
        status = m.group(3)
        if status == "FAIL":
            return SEV_RED, ["bringup-check"]
        if status == "SKIP":
            return SEV_YELLOW, ["bringup-check"]
        return SEV_GREEN, ["bringup-check"]

    severity = SEV_GREEN
    hits: list[str] = []
    for rule_id, sev, pat in RULES:
        if pat.search(body):
            hits.append(rule_id)
            severity = max(severity, sev)
    if not hits and SUCCESS_RE.search(body):
        hits.append("success")
    return severity, hits


def analyze(path: Path, ignore: list[re.Pattern[str]], expects: list[str],
            fail_on_warn: bool) -> dict:
    if not path.is_file():
        raise FileNotFoundError(path)

    steps: dict[str, Step] = {"前置": Step("前置")}
    order = ["前置"]
    current = "前置"

    checks: list[Check] = []
    seen: set[str] = set()
    total = {"red": 0, "yellow": 0, "green": 0}
    line_total = 0
    has_start = has_end = False
    first_ts = last_ts = ""

    with path.open("r", encoding="utf-8", errors="replace") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\r\n")
            m = TS_RE.match(line)
            if m:
                ts, body = m.group(1), m.group(2)
            else:
                ts, body = "", line

            if line_total == 0 and ts:
                first_ts = ts
            if ts:
                last_ts = ts
            line_total += 1

            marker = MARKER_RE.match(body)
            if marker:
                label = f"{marker.group(1)} {marker.group(2)}".strip()
                if label not in steps:
                    steps[label] = Step(label)
                    order.append(label)
                current = label
                entry = Entry(lineno, ts, body, SEV_GREEN, ["marker"], current)
                steps[current].green += 1
                steps[current].entries.append(entry)
                total["green"] += 1
                continue

            if START_RE.search(body):
                has_start = True
            elif "CAPTURE END" in body:
                has_end = True

            if not body.strip():
                continue

            severity, hits = classify(body, ignore)

            bm = BRINGUP_RE.search(body)
            if bm:
                key = f"{bm.group(1)}:{bm.group(2)}"
                if key not in seen:
                    seen.add(key)
                    checks.append(Check(bm.group(1), bm.group(2),
                                        bm.group(3), bm.group(4) or ""))

            entry = Entry(lineno, ts, body, severity, hits, current)
            step = steps[current]
            step.entries.append(entry)
            if severity == SEV_RED:
                step.red += 1
                total["red"] += 1
            elif severity == SEV_YELLOW:
                step.yellow += 1
                total["yellow"] += 1
            else:
                step.green += 1
                total["green"] += 1

    problems: list[Entry] = [
        e for s in steps.values() for e in s.entries if e.severity == SEV_RED
    ]
    warnings: list[Entry] = [
        e for s in steps.values() for e in s.entries if e.severity == SEV_YELLOW
    ]

    # 预期日志检查
    expect_results = []
    all_text = "\n".join(e.body for s in steps.values() for e in s.entries)
    for pat in expects:
        found = re.search(pat, all_text) is not None
        expect_results.append({"pattern": pat, "found": found})
        if not found:
            problems.append(Entry(0, "", f"缺少预期日志: {pat}", SEV_RED,
                                  ["expect"], "全局"))
            total["red"] += 1

    # 采集完整性
    capture_note = None
    capture_incomplete = False
    if has_start and not has_end:
        capture_note = "日志缺少 CAPTURE END，采集可能被中断（内容可能不完整）"
        capture_incomplete = True
    elif has_start and has_end:
        capture_note = "采集完整（START/END 成对）"

    meta = None
    meta_path = path.parent / "meta.json"
    if meta_path.is_file():
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            meta = None

    passed = sum(1 for e in steps.values() for x in e.entries
                 if BRINGUP_RE.search(x.body) and " PASS" in x.body)
    failed = sum(1 for e in steps.values() for x in e.entries
                 if BRINGUP_RE.search(x.body) and " FAIL" in x.body)
    skipped = sum(1 for e in steps.values() for x in e.entries
                  if BRINGUP_RE.search(x.body) and " SKIP" in x.body)

    result = "fail" if problems else (
        "warn" if (warnings or capture_incomplete or fail_on_warn) else "pass"
    )

    return {
        "file": str(path.resolve()),
        "meta": meta,
        "summary": {
            "lines": line_total,
            "red": total["red"],
            "yellow": total["yellow"],
            "green": total["green"],
            "steps": len([s for s in order if steps[s].entries]),
            "checks": {"pass": passed, "fail": failed, "skip": skipped},
            "first_timestamp": first_ts,
            "last_timestamp": last_ts,
            "capture_complete": None if not has_start else has_end,
            "capture_note": capture_note,
        },
        "steps": [
            {
                "label": label,
                "severity": max((e.severity for e in steps[label].entries),
                                default=SEV_GREEN),
                "red": steps[label].red,
                "yellow": steps[label].yellow,
                "green": steps[label].green,
                "entries": steps[label].entries,
            }
            for label in order if steps[label].entries
        ],
        "problems": problems,
        "warnings": warnings,
        "expectations": expect_results,
        "result": result,
    }


def _short_hash(meta: dict | None, key: str) -> str:
    if not meta:
        return ""
    if key == "firmware":
        fw = meta.get("firmware") or {}
        sha = fw.get("sha256", "")
        return f"{sha[:16]}…" if sha else ""
    value = meta.get(key, "")
    return f"{value[:12]}…" if value else ""


def render(report: dict, color: bool, only_problems: bool, context: int) -> None:
    s = report["summary"]
    meta = report.get("meta") or {}
    c = (lambda sev: ANSI[sev] if color else "")
    dim = DIM if color else ""
    bold = BOLD if color else ""
    reset = RESET if color else ""

    print(f"{bold}🔎 串口日志分析{reset}  v{VERSION}")
    print(f"📄 {report['file']}")

    if meta:
        bits = []
        if meta.get("label"):
            bits.append(f"台架 {meta['label']}")
        if meta.get("git_commit"):
            bits.append(f"提交 {meta['git_commit'][:12]}")
        fw = _short_hash(meta, "firmware")
        if fw:
            bits.append(f"固件 {fw}")
        if meta.get("operator"):
            bits.append(f"操作者 {meta['operator']}")
        if bits:
            print("🔖 " + " | ".join(bits))
        if meta.get("start"):
            print(f"🕒 {meta['start']} → {meta.get('end', '?')}"
                  f"（{meta.get('duration_s', '?')}s）")

    print(
        f"📊 行数 {s['lines']} | {EMOJI[2]} {s['red']} | "
        f"{EMOJI[1]} {s['yellow']} | {EMOJI[0]} {s['green']}"
    )
    if s["checks"]["pass"] or s["checks"]["fail"] or s["checks"]["skip"]:
        print(
            f"🧪 Bring-up 自检: {s['checks']['pass']} PASS / "
            f"{s['checks']['fail']} FAIL / {s['checks']['skip']} SKIP"
        )
    if s["capture_note"]:
        sev = SEV_GREEN if s["capture_complete"] else SEV_YELLOW
        print(f"{EMOJI[sev]} {s['capture_note']}")

    for step in report["steps"]:
        marker = EMOJI[step["severity"]]
        print(f"\n{bold}{marker} 步骤 {step['label']}{reset} "
              f"{dim}(🟢{step['green']} 🟡{step['yellow']} 🔴{step['red']}){reset}")
        for e in step["entries"]:
            if only_problems and e.severity == SEV_GREEN:
                continue
            ts = f"{dim}{e.ts}{reset} " if e.ts else ""
            print(f"  {EMOJI[e.severity]} {ts}{c(e.severity)}{e.body}{reset}")

    if report["problems"]:
        print(f"\n{bold}{ANSI[SEV_RED] if color else ''}🔴 问题清单 "
              f"({len(report['problems'])}){reset}")
        for idx, e in enumerate(report["problems"], 1):
            where = f"第 {e.lineno} 行" if e.lineno else "全局"
            ts = f" {e.ts}" if e.ts else ""
            print(f"  {idx}) [{e.step}] {where}{ts}")
            print(f"     {e.body}")

            if context > 0 and e.lineno:
                print(f"     {dim}上下文:{reset}")
                for step in report["steps"]:
                    for ctx in step["entries"]:
                        if abs(ctx.lineno - e.lineno) <= context and ctx is not e:
                            print(f"       {EMOJI[ctx.severity]} {ctx.body}")

    if report["warnings"] and not report["problems"]:
        print(f"\n🟡 警告 ({len(report['warnings'])})")
        for e in report["warnings"][:20]:
            where = f"第 {e.lineno} 行" if e.lineno else "全局"
            print(f"  - [{e.step}] {where} {e.body}")
        if len(report["warnings"]) > 20:
            print(f"  … 其余 {len(report['warnings']) - 20} 条见 --json")

    print()
    if report["result"] == "fail":
        print(f"{c(SEV_RED)}{EMOJI[SEV_RED]} 结论：发现 {len(report['problems'])} 个问题，"
              f"验证未通过{reset}")
    elif report["result"] == "warn":
        print(f"{c(SEV_YELLOW)}{EMOJI[SEV_YELLOW]} 结论：未发现错误，"
              f"但有 {len(report['warnings'])} 条警告{reset}")
    else:
        print(f"{c(SEV_GREEN)}{EMOJI[SEV_GREEN]} 结论：未发现问题，日志检查通过{reset}")


def _json_default(obj):
    if isinstance(obj, Entry):
        return {
            "line": obj.lineno,
            "timestamp": obj.ts,
            "step": obj.step,
            "severity": LABEL[obj.severity],
            "rules": obj.rules,
            "text": obj.body,
        }
    if isinstance(obj, Path):
        return str(obj)
    raise TypeError(f"not serializable: {type(obj)}")


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        description="G1 台架串口日志分析（🟢 正常 / 🟡 警告 / 🔴 问题）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("日志规则", 1)[1] if "日志规则" in __doc__ else "",
    )
    p.add_argument("log", help="serial.log 路径（或任意纯文本串口日志）")
    p.add_argument("--json", action="store_true", help="输出机器可读 JSON")
    p.add_argument("--no-color", action="store_true", help="禁用 ANSI 颜色")
    p.add_argument("--only-problems", action="store_true",
                   help="仅显示 🟡/🔴 行")
    p.add_argument("--context", type=int, default=0,
                   help="问题清单附带的上下文行数")
    p.add_argument("--ignore", action="append", default=[],
                   help="忽略匹配该正则的行（可重复）")
    p.add_argument("--expect", action="append", default=[],
                   help="必须出现的日志正则，缺失记为问题（可重复）")
    p.add_argument("--fail-on-warn", action="store_true",
                   help="存在警告时也返回非 0")
    p.add_argument("--list-rules", action="store_true", help="打印内置规则表")
    p.add_argument("--version", action="version", version=VERSION)
    args = p.parse_args(argv)

    if args.list_rules:
        print(f"{'规则':<14}{'级别':<6}正则")
        for rule_id, sev, pat in RULES:
            print(f"{rule_id:<14}{LABEL[sev]:<6}{pat.pattern}")
        return 0

    try:
        ignore = [re.compile(x) for x in args.ignore]
    except re.error as exc:
        print(f"错误：--ignore 正则无效: {exc}", file=sys.stderr)
        return 2

    try:
        report = analyze(Path(args.log), ignore, args.expect, args.fail_on_warn)
    except FileNotFoundError:
        print(f"错误：日志不存在: {args.log}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"错误：读取日志失败: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2,
                         default=_json_default))
    else:
        color = (not args.no_color) and sys.stdout.isatty()
        render(report, color, args.only_problems, args.context)

    if report["result"] == "fail":
        return 1
    if report["result"] == "warn" and args.fail_on_warn:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
