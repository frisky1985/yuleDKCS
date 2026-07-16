"""
解锁响应时间 — HIL 测试模块

覆盖测试:
    HIL-UL-01 BLE 解锁 < 1s
    HIL-UL-02 NFC 解锁 < 500ms
    HIL-UL-03 UWB 自动解锁
    HIL-UL-04 解锁失败重试机制
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["UNLOCK"])
