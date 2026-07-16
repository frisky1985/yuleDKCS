"""
故障注入 — HIL 测试模块

覆盖测试:
    HIL-FI-01 BLE 通信异常
    HIL-FI-02 SE050 通信故障
    HIL-FI-03 NFC 通信故障
    HIL-FI-04 电源掉电恢复
    HIL-FI-05 非法状态机转换
    HIL-FI-06 签名绕过攻击模拟

前置条件:
    DK_FAULT_INJECT_ENABLE=1 的 HIL 测试固件
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["FI"])
