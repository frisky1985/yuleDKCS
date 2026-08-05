"""
NFC 通信 — HIL 测试模块

覆盖测试:
    HIL-NFC-01 NFC 标准刷卡解锁
    HIL-NFC-02 NFC 多卡共存
    HIL-NFC-03 NFC 超时处理
    HIL-NFC-04 NFC 场强与距离
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["NFC"])
