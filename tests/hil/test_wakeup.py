"""
Covers: REQ-034
唤醒源 — HIL 测试模块

覆盖测试:
    HIL-WK-01 BLE 唤醒
    HIL-WK-02 NFC 唤醒
    HIL-WK-03 定时唤醒
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["WAKEUP"])
