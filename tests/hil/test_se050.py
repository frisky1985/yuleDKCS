"""
Covers: REQ-033
SE050 SCP03 安全通道 — HIL 测试模块

覆盖测试:
    HIL-SE-01 SCP03 标准建链
    HIL-SE-02 SCP03 建链失败(错误密钥)
    HIL-SE-03 密钥注入与签名
    HIL-SE-04 密钥更新
    HIL-SE-05 密钥删除
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["SE050"])
