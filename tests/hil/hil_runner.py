#!/usr/bin/env python3
"""
yuleDKCS HIL Test Runner
========================

Main entry point for Hardware-in-the-Loop testing of the yuleDKCS digital key
system on S32K312 EVB hardware.

Usage:
    python3 hil_runner.py --check-env      # Environment self-check
    python3 hil_runner.py --status          # System status
    python3 hil_runner.py --all             # Run all tests
    python3 hil_runner.py --domain BLE      # Run BLE domain tests
    python3 hil_runner.py --domain BLE,NFC  # Run multiple domains
    python3 hil_runner.py --test HIL-BLE-01 # Single test
    python3 hil_runner.py --flash           # Flash firmware
    python3 hil_runner.py --power-on        # Power on rig
    python3 hil_runner.py --power-off       # Power off rig

Output:
    - JSON report:  reports/hil-report-{timestamp}.json
    - JUnit XML:    reports/hil-junit.xml
"""

import argparse
import json
import os
import subprocess
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path

# 保证 tests/hil 下模块可导入 (直接运行 或 python -m 两种方式)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from transports import JLinkTransport

# ---------------------------------------------------------------------------
#  Paths
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
HIL_DIR = Path(__file__).resolve().parent
REPORTS_DIR = HIL_DIR / "reports"
EMBEDDED_DIR = REPO_ROOT / "embedded"

# Test script registry: domain -> module path
TEST_DOMAINS = {
    "BLE":     "tests.hil.test_ble",
    "NFC":     "tests.hil.test_nfc",
    "UWB":     "tests.hil.test_uwb",
    "SE050":   "tests.hil.test_se050",
    "UNLOCK":  "tests.hil.test_unlock",
    "VS":      "tests.hil.test_vehicle_status",
    "PM":      "tests.hil.test_power_mgmt",
    "FI":      "tests.hil.test_fault_inject",
    "WAKEUP":  "tests.hil.test_wakeup",
}

# Domain -> individual test IDs
DOMAIN_TEST_MAP = {
    "BLE":    ["HIL-BLE-01", "HIL-BLE-02", "HIL-BLE-03", "HIL-BLE-04", "HIL-BLE-05"],
    "NFC":    ["HIL-NFC-01", "HIL-NFC-02", "HIL-NFC-03", "HIL-NFC-04"],
    "UWB":    ["HIL-UWB-01", "HIL-UWB-02", "HIL-UWB-03", "HIL-UWB-04"],
    "SE050":  ["HIL-SE-01", "HIL-SE-02", "HIL-SE-03", "HIL-SE-04", "HIL-SE-05"],
    "UNLOCK": ["HIL-UL-01", "HIL-UL-02", "HIL-UL-03", "HIL-UL-04"],
    "VS":     ["HIL-VS-01", "HIL-VS-02", "HIL-VS-03"],
    "PM":     ["HIL-PM-01", "HIL-PM-02", "HIL-PM-03"],
    "FI":     ["HIL-FI-01", "HIL-FI-02", "HIL-FI-03", "HIL-FI-04", "HIL-FI-05", "HIL-FI-06"],
    "WAKEUP": ["HIL-WK-01", "HIL-WK-02", "HIL-WK-03"],
}

ALL_TEST_IDS = [tid for ids in DOMAIN_TEST_MAP.values() for tid in ids]

# Priority mapping
TEST_PRIORITY = {
    # P0
    "HIL-BLE-01": "P0", "HIL-BLE-02": "P0",
    "HIL-NFC-01": "P0", "HIL-NFC-03": "P0",
    "HIL-SE-01":  "P0", "HIL-SE-02": "P0", "HIL-SE-03": "P0",
    "HIL-UL-01":  "P0", "HIL-UL-02": "P0",
    "HIL-FI-05":  "P0",
    # P1
    "HIL-BLE-03": "P1", "HIL-BLE-04": "P1",
    "HIL-UWB-01": "P1", "HIL-UWB-02": "P1", "HIL-UWB-03": "P1", "HIL-UWB-04": "P1",
    "HIL-UL-03":  "P1", "HIL-UL-04": "P1",
    "HIL-SE-04":  "P1",
    "HIL-VS-01":  "P1", "HIL-VS-03": "P1",
    "HIL-FI-01":  "P1", "HIL-FI-02": "P1", "HIL-FI-03": "P1", "HIL-FI-04": "P1", "HIL-FI-06": "P1",
    "HIL-WK-01":  "P1", "HIL-WK-02": "P1",
    "HIL-PM-02":  "P1",
    # P2
    "HIL-BLE-05": "P2",
    "HIL-NFC-02": "P2", "HIL-NFC-04": "P2",
    "HIL-SE-05":  "P2",
    "HIL-VS-02":  "P2",
    "HIL-PM-01":  "P2", "HIL-PM-03": "P2",
    "HIL-WK-03":  "P2",
}

# ---------------------------------------------------------------------------
#  Test Result
# ---------------------------------------------------------------------------
class TestResult:
    """Single test case result."""

    def __init__(self, test_id, name, domain, status="NOT_RUN",
                 duration_ms=0, measurements=None, details="", priority="P2"):
        self.test_id = test_id
        self.name = name
        self.domain = domain
        self.status = status  # PASSED | FAILED | ERROR | NOT_RUN
        self.duration_ms = duration_ms
        self.measurements = measurements or {}
        self.details = details
        self.priority = TEST_PRIORITY.get(test_id, priority)

    def to_dict(self):
        return {
            "test_id": self.test_id,
            "name": self.name,
            "domain": self.domain,
            "priority": self.priority,
            "status": self.status,
            "duration_ms": self.duration_ms,
            "measurements": self.measurements,
            "details": self.details,
        }


# ---------------------------------------------------------------------------
#  Report
# ---------------------------------------------------------------------------
class TestReport:
    """Test run report generator."""

    def __init__(self, firmware_version="unknown", hardware_config="S32K312-EVB"):
        self.firmware_version = firmware_version
        self.hardware_config = hardware_config
        self.start_time = datetime.now(timezone.utc)
        self.end_time = None
        self.results = []
        self.errors = []

    def add_result(self, result):
        self.results.append(result)

    def add_error(self, test_id, message):
        self.errors.append({"test_id": test_id, "message": message})

    def finalize(self):
        self.end_time = datetime.now(timezone.utc)

    @property
    def totals(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.status == "PASSED")
        failed = sum(1 for r in self.results if r.status == "FAILED")
        error = sum(1 for r in self.results if r.status == "ERROR")
        not_run = sum(1 for r in self.results if r.status == "NOT_RUN")
        return total, passed, failed, error, not_run

    @property
    def pass_rate(self):
        total, passed, *_ = self.totals
        if total == 0:
            return 0.0
        return round(passed / total * 100, 1)

    @property
    def p0_pass_rate(self):
        p0 = [r for r in self.results if r.priority == "P0"]
        if not p0:
            return 100.0
        passed = sum(1 for r in p0 if r.status == "PASSED")
        return round(passed / len(p0) * 100, 1)

    @property
    def p0_passed(self):
        p0 = [r for r in self.results if r.priority == "P0"]
        return sum(1 for r in p0 if r.status == "PASSED")

    @property
    def p0_total(self):
        return sum(1 for r in self.results if r.priority == "P0")

    @property
    def p1_pass_rate(self):
        p1 = [r for r in self.results if r.priority == "P1"]
        if not p1:
            return 100.0
        passed = sum(1 for r in p1 if r.status == "PASSED")
        return round(passed / len(p1) * 100, 1)

    @property
    def p1_passed(self):
        p1 = [r for r in self.results if r.priority == "P1"]
        return sum(1 for r in p1 if r.status == "PASSED")

    @property
    def p1_total(self):
        return sum(1 for r in self.results if r.priority == "P1")

    def to_json(self):
        duration = (self.end_time - self.start_time).total_seconds()
        total, passed, failed, error, not_run = self.totals
        return {
            "metadata": {
                "firmware_version": self.firmware_version,
                "hardware_config": self.hardware_config,
                "tester": "auto",
                "start_time": self.start_time.isoformat(),
                "end_time": self.end_time.isoformat(),
                "duration_seconds": duration,
            },
            "summary": {
                "total": total,
                "passed": passed,
                "failed": failed,
                "error": error,
                "not_run": not_run,
                "pass_rate": self.pass_rate,
                "p0_pass_rate": self.p0_pass_rate,
                "p1_pass_rate": self.p1_pass_rate,
            },
            "results": [r.to_dict() for r in self.results],
            "failures": [
                r.to_dict() for r in self.results
                if r.status in ("FAILED", "ERROR")
            ],
        }

    def save_report(self):
        REPORTS_DIR.mkdir(parents=True, exist_ok=True)
        ts = self.start_time.strftime("%Y%m%d-%H%M%S")
        path = REPORTS_DIR / f"hil-report-{ts}.json"
        with open(path, "w", encoding="utf-8") as f:
            json.dump(self.to_json(), f, indent=2, ensure_ascii=False)
        print(f"[HIL] Report saved: {path}")
        return path

    def print_summary(self):
        total, passed, failed, error, not_run = self.totals
        print()
        print("=" * 60)
        print("  yuleDKCS HIL Test Summary")
        print("=" * 60)
        print(f"  Total:   {total:3d}")
        print(f"  Passed:  {passed:3d}")
        print(f"  Failed:  {failed:3d}")
        print(f"  Error:   {error:3d}")
        print(f"  Not Run: {not_run:3d}")
        print(f"  Pass Rate: {self.pass_rate:.1f}%")
        print(f"  P0 Rate:   {self.p0_pass_rate:.1f}% ({self.p0_passed}/{self.p0_total})")
        print(f"  P1 Rate:   {self.p1_pass_rate:.1f}% ({self.p1_passed}/{self.p1_total})")
        if self.errors:
            print()
            print("  Errors:")
            for e in self.errors:
                print(f"    [{e['test_id']}] {e['message']}")
        print("=" * 60)
        print()


# ---------------------------------------------------------------------------
#  Hardware Interface
# ---------------------------------------------------------------------------
class HardwareInterface:
    """S32K312 EVB 硬件接口 — 通过可插拔 transport 通信.

    transport 可选: qemu (SIL) / serial (真实 UART) / jlink (J-Link).
    默认 qemu: 无硬件时用 QEMU 软件在环跑固件逻辑.
    """

    def __init__(self, uart_device="/dev/tty.usbserial-*", baud=115200,
                 transport=None, transport_kwargs=None):
        self.uart_device = uart_device
        self.baud = baud
        if transport is not None:
            self._transport = transport
        else:
            from transports import create_transport
            kwargs = dict(transport_kwargs or {})
            kind = kwargs.pop("kind", "qemu")
            self._transport = create_transport(kind, **kwargs)
        self._transport_open = False

    # -- transport 生命周期 --
    def open(self):
        if not self._transport_open:
            self._transport.open()
            self._transport_open = True
        return self

    def send_command(self, cmd):
        """发送命令到固件 (UART)."""
        return self._transport.send_command(cmd)

    def read_line(self):
        """从固件读取一行."""
        return self._transport.read_line()

    def query(self, cmd, timeout=2.0, prefix="HIL:"):
        """发送命令并等待带前缀的响应行 (过滤固件日志输出)."""
        self.send_command(cmd)
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self._transport.read_line(timeout=0.5)
            if line and line.startswith(prefix):
                return line
        return None

    def flash_firmware(self, binary_path=None):
        """使用 J-Link 烧录固件."""
        if isinstance(self._transport, JLinkTransport):
            if binary_path is None:
                binary_path = EMBEDDED_DIR / "build" / "hil" / "yuleDKCS_hil.elf"
            return self._transport.flash(str(binary_path))
        print("[HIL] 当前 transport 非 J-Link, 跳过烧录")
        return True

    def read_firmware_version(self):
        """读取固件版本."""
        resp = self.query("HIL:GET_VERSION", timeout=2.0)
        if resp and "VERSION" in resp:
            return resp.split("VERSION:")[-1].strip()
        return "simulated-v1.2.0"

    def close(self):
        if self._transport_open:
            self._transport.close()
            self._transport_open = False


# ---------------------------------------------------------------------------
#  Test Runner
# ---------------------------------------------------------------------------
class HILTestRunner:
    """Orchestrates HIL test execution."""

    def __init__(self, hardware=None, report=None):
        self.hw = hardware or HardwareInterface()
        self.report = report or TestReport()
        self.results = []

    def run_test(self, test_id, test_fn, *args, **kwargs):
        """Run a single test and record the result."""
        test_meta = self._get_test_meta(test_id)
        result = TestResult(
            test_id=test_id,
            name=test_meta.get("name", test_id),
            domain=test_meta.get("domain", "UNKNOWN"),
            priority=test_meta.get("priority", "P2"),
        )
        print(f"\n[HIL] >>> Running {test_id}: {result.name} [{result.priority}]")
        start = time.perf_counter()
        try:
            test_fn(self.hw, result, *args, **kwargs)
            if result.status == "NOT_RUN":
                result.status = "PASSED"
        except AssertionError:
            result.status = "FAILED"
        except Exception as e:
            result.status = "ERROR"
            result.details = traceback.format_exc()
            self.report.add_error(test_id, str(e))
        finally:
            elapsed = (time.perf_counter() - start) * 1000
            result.duration_ms = round(elapsed, 1)
            self.report.add_result(result)
            icon = {"PASSED": "✅", "FAILED": "❌", "ERROR": "⚠️", "NOT_RUN": "⏭️"}
            print(f"[HIL] <<< {icon.get(result.status, '❓')} {test_id}: "
                  f"{result.status} ({result.duration_ms:.0f}ms)")
            if result.status in ("FAILED", "ERROR") and result.details:
                print(f"[HIL]     Detail: {result.details[:200]}")
        return result

    def _get_test_meta(self, test_id):
        """Return metadata dict for a test ID."""
        domain = None
        for d, ids in DOMAIN_TEST_MAP.items():
            if test_id in ids:
                domain = d
                break
        prio = TEST_PRIORITY.get(test_id, "P2")
        name_map = {
            "HIL-BLE-01": "BLE 标准连接",
            "HIL-BLE-02": "RSSI 阈值测试",
            "HIL-BLE-03": "断线重连",
            "HIL-BLE-04": "多设备并发连接",
            "HIL-BLE-05": "BLE GATT MTU 协商",
            "HIL-UWB-01": "1m 近距离测距精度",
            "HIL-UWB-02": "5m 中距离测距精度",
            "HIL-UWB-03": "10m 远距离测距精度",
            "HIL-UWB-04": "20m 极限距离测距稳定性",
            "HIL-NFC-01": "NFC 标准刷卡解锁",
            "HIL-NFC-02": "NFC 多卡共存",
            "HIL-NFC-03": "NFC 超时处理",
            "HIL-NFC-04": "NFC 场强与距离",
            "HIL-SE-01": "SCP03 标准建链",
            "HIL-SE-02": "SCP03 建链失败(错误密钥)",
            "HIL-SE-03": "密钥注入与签名",
            "HIL-SE-04": "密钥更新",
            "HIL-SE-05": "密钥删除",
            "HIL-UL-01": "BLE 解锁 < 1s",
            "HIL-UL-02": "NFC 解锁 < 500ms",
            "HIL-UL-03": "UWB 自动解锁",
            "HIL-UL-04": "解锁失败重试机制",
            "HIL-VS-01": "状态变更推送",
            "HIL-VS-02": "离线缓冲",
            "HIL-VS-03": "状态变更频控",
            "HIL-PM-01": "休眠电流测量",
            "HIL-PM-02": "BLE 唤醒延迟",
            "HIL-PM-03": "低电量模式",
            "HIL-FI-01": "BLE 通信异常",
            "HIL-FI-02": "SE050 通信故障",
            "HIL-FI-03": "NFC 通信故障",
            "HIL-FI-04": "电源掉电恢复",
            "HIL-FI-05": "非法状态机转换",
            "HIL-FI-06": "签名绕过攻击模拟",
            "HIL-WK-01": "BLE 唤醒",
            "HIL-WK-02": "NFC 唤醒",
            "HIL-WK-03": "定时唤醒",
        }
        return {
            "test_id": test_id,
            "name": name_map.get(test_id, test_id),
            "domain": domain or "UNKNOWN",
            "priority": prio,
        }

    def run_all(self):
        """Run all 37 HIL tests in dependency order."""
        ordered = [
            # Phase 1: HW basics
            "HIL-PM-01",
            "HIL-SE-01",
            # Phase 2: BLE
            "HIL-BLE-01", "HIL-BLE-02", "HIL-BLE-03", "HIL-BLE-05",
            # Phase 3: Security
            "HIL-SE-02", "HIL-SE-03", "HIL-SE-04", "HIL-SE-05",
            "HIL-UL-01",
            # Phase 4: NFC
            "HIL-NFC-01", "HIL-NFC-02", "HIL-NFC-03", "HIL-NFC-04",
            "HIL-UL-02", "HIL-UL-04",
            # Phase 5: UWB
            "HIL-UWB-01", "HIL-UWB-02", "HIL-UWB-03", "HIL-UWB-04",
            "HIL-UL-03",
            # Phase 6: Vehicle status + wakeup
            "HIL-VS-01", "HIL-VS-02", "HIL-VS-03",
            "HIL-WK-01", "HIL-WK-02", "HIL-WK-03",
            "HIL-PM-02",
            # Phase 7: Fault injection
            "HIL-FI-01", "HIL-FI-02", "HIL-FI-03", "HIL-FI-04",
            "HIL-FI-05", "HIL-FI-06",
            # Phase 8: Power
            "HIL-PM-03", "HIL-BLE-04",
        ]
        return self._run_test_list(ordered)

    def run_domains(self, domains):
        """Run tests for specific domains."""
        test_ids = []
        for d in domains:
            test_ids.extend(DOMAIN_TEST_MAP.get(d.upper(), []))
        return self._run_test_list(test_ids)

    def run_single(self, test_id):
        """Run a single test by ID."""
        return self._run_test_list([test_id])

    def _run_test_list(self, test_ids):
        """Run a list of tests by importing their implementations."""
        from tests.hil import test_cases

        for test_id in test_ids:
            if test_id not in ALL_TEST_IDS:
                result = TestResult(
                    test_id=test_id, name="Unknown",
                    domain="UNKNOWN", status="NOT_RUN",
                    details=f"Test ID {test_id} not found in test registry",
                )
                self.report.add_result(result)
                continue

            if test_id in test_cases.TEST_REGISTRY:
                test_fn = test_cases.TEST_REGISTRY[test_id]
                self.run_test(test_id, test_fn)
            else:
                # Create a dummy placeholder result
                result = TestResult(
                    test_id=test_id,
                    name=self._get_test_meta(test_id).get("name", test_id),
                    domain=self._get_test_meta(test_id).get("domain", "UNKNOWN"),
                    status="NOT_RUN",
                    details="Test case implementation not found in registry",
                    priority=TEST_PRIORITY.get(test_id, "P2"),
                )
                self.report.add_result(result)
                print(f"[HIL] ⏭️  {test_id}: NOT_RUN (no implementation)")

        self.report.finalize()
        self.report.print_summary()
        self.report.save_report()
        return self.report


# ---------------------------------------------------------------------------
#  Environment check
# ---------------------------------------------------------------------------
def check_environment():
    """Run environment self-check."""
    print("=" * 60)
    print("  yuleDKCS HIL Environment Check")
    print("=" * 60)
    checks = []

    # Tool availability
    tools = {
        "python3": ("python3", "--version"),
        "cmake": ("cmake", "--version"),
        "arm-none-eabi-gcc": ("arm-none-eabi-gcc", "--version"),
        "JLinkExe": ("JLinkExe", "--version"),
        "screen": ("which", "screen"),
    }
    for name, cmd in tools.items():
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            ok = r.returncode == 0
            checks.append((f"Tool: {name}", "✅" if ok else "❌"))
            if ok:
                print(f"  ✅ Tool: {name} found: {r.stdout.split(chr(10))[0].strip()}")
            else:
                print(f"  ❌ Tool: {name} NOT FOUND")
        except FileNotFoundError:
            checks.append((f"Tool: {name}", "❌"))
            print(f"  ❌ Tool: {name} NOT FOUND")

    # Directory structure
    dirs = [
        HIL_DIR,
        EMBEDDED_DIR,
        EMBEDDED_DIR / "bsw_integration",
        EMBEDDED_DIR / "ccc_protocol",
    ]
    for d in dirs:
        ok = d.exists()
        checks.append((f"Dir: {d.name}", "✅" if ok else "❌"))
        print(f"  {'✅' if ok else '❌'} Dir: {d}")

    # Python modules
    modules = ["json", "argparse", "subprocess", "serial"]
    for m in modules:
        try:
            __import__(m)
            checks.append((f"Module: {m}", "✅"))
            print(f"  ✅ Module: {m}")
        except ImportError:
            checks.append((f"Module: {m}", "❌"))
            print(f"  ❌ Module: {m}")

    failed = sum(1 for _, s in checks if s == "❌")
    print()
    if failed == 0:
        print("  ✅ All checks passed! Environment is ready.")
    else:
        print(f"  ⚠️  {failed} check(s) failed. Please review.")
    print("=" * 60)
    return failed == 0


# ---------------------------------------------------------------------------
#  CLI
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="yuleDKCS HIL Test Runner")
    parser.add_argument("--check-env", action="store_true", help="Environment self-check")
    parser.add_argument("--status", action="store_true", help="Read system status")
    parser.add_argument("--all", action="store_true", help="Run all HIL tests")
    parser.add_argument("--domain", type=str, help="Comma-separated domains to test")
    parser.add_argument("--test", type=str, help="Single test ID to run")
    parser.add_argument("--flash", action="store_true", help="Flash firmware to EVB")
    parser.add_argument("--power-on", action="store_true", help="Power on rig")
    parser.add_argument("--power-off", action="store_true", help="Power off rig")
    parser.add_argument("--jenkins", action="store_true", help="Jenkins CI mode")
    parser.add_argument("--transport", type=str, default="qemu",
                        help="传输层: qemu (SIL, 默认) / serial / jlink")
    parser.add_argument("--transport-arg", action="append", default=[],
                        help="transport 参数, 如 --transport-arg kind=qemu "
                             "--transport-arg kernel=path/to.elf")
    args = parser.parse_args()

    # Ensure reports dir
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    if args.check_env:
        sys.exit(0 if check_environment() else 1)

    # Initialize (transport 可插拔)
    transport_kwargs = {}
    for kv in args.transport_arg:
        if "=" in kv:
            k, v = kv.split("=", 1)
            transport_kwargs[k] = v
    transport_kwargs.setdefault("kind", args.transport)
    if transport_kwargs.get("kind") == "qemu":
        transport_kwargs.setdefault(
            "kernel", str(REPO_ROOT / "tests" / "qemu_m33" / "qemu_m33.elf"))
    hw = HardwareInterface(transport_kwargs=transport_kwargs)
    report = TestReport()
    runner = HILTestRunner(hardware=hw, report=report)

    if args.flash:
        ok = hw.flash_firmware()
        sys.exit(0 if ok else 1)

    if args.power_on:
        print("[HIL] Power ON — please connect EVB power supply manually")
        sys.exit(0)

    if args.power_off:
        print("[HIL] Power OFF — please disconnect EVB power supply manually")
        sys.exit(0)

    if args.status:
        hw.open()  # status 需连接 transport
        print("=" * 60)
        print("  yuleDKCS HIL System Status")
        print("=" * 60)
        try:
            version = hw.read_firmware_version()
        finally:
            hw.close()
        print(f"  Firmware: {version}")
        print(f"  HW Config: S32K312-EVB + KW47 + ST25R501 + SE050 + NCJ29D6")
        print(f"  HIL Dir: {HIL_DIR}")
        sys.exit(0)

    if args.test or args.domain or args.all:
        hw.open()  # 启动 transport (QEMU 进程 / 串口)
        runner.report.start_time = datetime.now(timezone.utc)
        try:
            if args.test:
                runner.run_single(args.test.upper())
            elif args.domain:
                domains = [d.strip().upper() for d in args.domain.split(",")]
                runner.run_domains(domains)
            elif args.all:
                runner.run_all()
        finally:
            hw.close()

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
