"""
Covers: REQ-016
车况同步 — HIL 测试模块

覆盖测试:
    HIL-VS-01 状态变更推送
    HIL-VS-02 离线缓冲
    HIL-VS-03 状态变更频控
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["VS"])
