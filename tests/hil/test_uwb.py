"""
UWB 测距精度 — HIL 测试模块

覆盖测试:
    HIL-UWB-01 1m 近距离精度
    HIL-UWB-02 5m 中距离精度
    HIL-UWB-03 10m 远距离精度
    HIL-UWB-04 20m 极限距离
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["UWB"])
