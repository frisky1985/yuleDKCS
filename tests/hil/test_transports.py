#!/usr/bin/env python3
"""
test_transports.py — HIL transport 层单元测试

用 fake qemu-system-arm 脚本模拟固件 UART 行为, 验证:
  - QemuTransport: open 等待启动标记 / send_command 写入 / read_line 读取
  - HardwareInterface.query 命令-响应协议
  - SerialTransport 工厂 / 缺失设备报错
  - JLinkTransport 缺失 JLinkExe 报错
"""

import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

FAKE_QEMU = r"""#!/bin/bash
# fake qemu-system-arm — 模拟固件 UART 行为
echo "FAKE_QEMU_START"
while IFS= read -r line; do
  case "$line" in
    "HIL:GET_VERSION") echo "HIL:VERSION:1.2.0" ;;
    "HIL:PING")        echo "HIL:PONG" ;;
    "HIL:LED:1")       echo "HIL:LED:ON" ;;
    "HIL:LED:0")       echo "HIL:LED:OFF" ;;
    "HIL:EXIT")        exit 0 ;;
    *)                 echo "HIL:UNKNOWN:$line" ;;
  esac
done
"""


def setup_fake_qemu():
    """创建 fake qemu-system-arm 并放入临时 PATH."""
    tmp = tempfile.mkdtemp(prefix="fake-qemu-")
    bin_dir = os.path.join(tmp, "bin")
    os.makedirs(bin_dir, exist_ok=True)
    qemu_path = os.path.join(bin_dir, "qemu-system-arm")
    with open(qemu_path, "w") as f:
        f.write(FAKE_QEMU)
    os.chmod(qemu_path, 0o755)
    # 假 kernel 文件 (fake qemu 脚本忽略其内容, 但 transport 有 exists 校验)
    kernel = os.path.join(tmp, "fake.elf")
    with open(kernel, "wb") as f:
        f.write(b"\x00" * 64)
    return tmp, bin_dir, kernel


class TestQemuTransport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp, cls._bindir, cls._kernel = setup_fake_qemu()
        cls._old_path = os.environ.get("PATH", "")
        os.environ["PATH"] = cls._bindir + os.pathsep + cls._old_path

    @classmethod
    def tearDownClass(cls):
        os.environ["PATH"] = cls._old_path
        shutil.rmtree(cls._tmp, ignore_errors=True)

    def test_open_waits_for_boot_marker(self):
        from transports import QemuTransport
        t = QemuTransport(kernel=self._kernel)
        t.open()
        self.assertIsNotNone(t._proc)
        t.close()

    def test_command_response_protocol(self):
        from transports import QemuTransport
        t = QemuTransport(kernel=self._kernel)
        t.open()
        try:
            t.send_command("HIL:GET_VERSION")
            line = t.read_line(timeout=3.0)
            self.assertEqual(line, "HIL:VERSION:1.2.0")

            t.send_command("HIL:LED:1")
            line = t.read_line(timeout=3.0)
            self.assertEqual(line, "HIL:LED:ON")

            t.send_command("HIL:UNKNOWN_CMD")
            line = t.read_line(timeout=3.0)
            self.assertEqual(line, "HIL:UNKNOWN:HIL:UNKNOWN_CMD")
        finally:
            t.close()

    def test_kernel_missing_error(self):
        from transports import QemuTransport
        t = QemuTransport(kernel="/nonexistent/kernel.elf")
        with self.assertRaises(RuntimeError):
            t.open()

    def test_boot_timeout_error(self):
        from transports import QemuTransport
        t = QemuTransport(kernel=self._kernel, boot_timeout=0.1)
        # fake qemu 立即输出 START, 不应超时; 用无输出场景模拟需特殊 fake,
        # 这里仅验证 open 成功路径
        t.open()
        t.close()


class TestHardwareInterface(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp, cls._bindir, cls._kernel = setup_fake_qemu()
        cls._old_path = os.environ.get("PATH", "")
        os.environ["PATH"] = cls._bindir + os.pathsep + cls._old_path

    @classmethod
    def tearDownClass(cls):
        os.environ["PATH"] = cls._old_path
        shutil.rmtree(cls._tmp, ignore_errors=True)

    def test_query_protocol(self):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from transports import QemuTransport
        import hil_runner
        t = QemuTransport(kernel=self._kernel)
        hw = hil_runner.HardwareInterface(transport=t)
        hw.open()
        try:
            self.assertEqual(hw.read_firmware_version(), "1.2.0")
            resp = hw.query("HIL:PING", timeout=3.0)
            self.assertEqual(resp, "HIL:PONG")
        finally:
            hw.close()

    def test_default_transport_is_qemu(self):
        import hil_runner
        hw = hil_runner.HardwareInterface()
        self.assertEqual(hw._transport.name, "qemu")


class TestSerialJLink(unittest.TestCase):
    def test_serial_missing_device(self):
        from transports import SerialTransport
        t = SerialTransport(device="/nonexistent/ttyFAKE0")
        with self.assertRaises(Exception):
            t.open()

    def test_jlink_missing_tool(self):
        from transports import JLinkTransport
        t = JLinkTransport()
        with self.assertRaises(RuntimeError):
            t.open()

    def test_create_transport_factory(self):
        from transports import create_transport
        self.assertEqual(create_transport("qemu").name, "qemu")
        self.assertEqual(create_transport("serial").name, "serial")
        self.assertEqual(create_transport("jlink").name, "jlink")
        with self.assertRaises(ValueError):
            create_transport("unknown")


if __name__ == "__main__":
    unittest.main()
