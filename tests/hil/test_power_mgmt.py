"""
电源管理 — HIL 测试模块

覆盖测试:
    HIL-PM-01 休眠电流测量
    HIL-PM-02 BLE 唤醒延迟
    HIL-PM-03 低电量模式
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["PM"])
