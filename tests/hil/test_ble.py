"""
BLE 连接稳定性 — HIL 测试模块

覆盖测试:
    HIL-BLE-01 BLE 标准连接
    HIL-BLE-02 RSSI 阈值测试
    HIL-BLE-03 断线重连
    HIL-BLE-04 多设备并发连接
    HIL-BLE-05 BLE GATT MTU 协商
"""

from tests.hil.test_cases import TEST_REGISTRY
from tests.hil.hil_runner import HILTestRunner, HardwareInterface


def run_all(hw=None, report=None):
    """Run all BLE tests and return the report."""
    runner = HILTestRunner(hardware=hw or HardwareInterface(), report=report)
    return runner.run_domains(["BLE"])
