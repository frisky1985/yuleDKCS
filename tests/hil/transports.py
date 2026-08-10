#!/usr/bin/env python3
"""
transports.py — HIL HardwareInterface 传输层抽象

三态 transport:
  - QemuTransport:    QEMU 软件在环 (SIL) — 通过 stdio 管道与固件 UART 通信
  - SerialTransport:  真实硬件 — pyserial 连 debug UART
  - JLinkTransport:   SEGGER J-Link — 烧录/复位/内存读写 (JLinkExe CLI)

用法:
    from transports import QemuTransport, SerialTransport, JLinkTransport
    hw = HardwareInterface(transport=QemuTransport(kernel="qemu_m33.elf"))
    hw.send_command("HIL:GET_VERSION")
"""

import os
import re
import shutil
import subprocess
import time

# ---------------------------------------------------------------------------
#  Base
# ---------------------------------------------------------------------------
class BaseTransport:
    """传输层接口: 发送命令 / 读取行 / 关闭."""

    name = "base"

    def open(self):
        raise NotImplementedError

    def send_command(self, cmd):
        raise NotImplementedError

    def read_line(self, timeout=2.0):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError


# ---------------------------------------------------------------------------
#  QEMU (SIL)
# ---------------------------------------------------------------------------
class QemuTransport(BaseTransport):
    """通过 QEMU stdio 管道与固件 UART 通信 (软件在环)."""

    name = "qemu"

    def __init__(self, kernel=None, machine="mps2-an521", cpu="cortex-m33",
                 boot_timeout=10.0):
        self.kernel = kernel
        self.machine = machine
        self.cpu = cpu
        self.boot_timeout = boot_timeout
        self._proc = None

    def open(self):
        qemu = shutil.which("qemu-system-arm")
        if not qemu:
            raise RuntimeError("qemu-system-arm 未安装")
        if not self.kernel or not os.path.exists(self.kernel):
            raise RuntimeError(f"QEMU kernel 不存在: {self.kernel}")
        cmd = [
            qemu, "-machine", self.machine, "-cpu", self.cpu,
            "-kernel", self.kernel, "-nographic",
            "-monitor", "none", "-serial", "stdio",
        ]
        self._proc = subprocess.Popen(
            cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=1, text=False,
        )
        # 等待固件启动标记
        deadline = time.time() + self.boot_timeout
        booted = False
        while time.time() < deadline:
            line = self.read_line(timeout=1.0)
            if line and "START" in line:
                booted = True
                break
        if not booted:
            raise RuntimeError("QEMU 固件未在超时内输出启动标记")
        return self

    def send_command(self, cmd):
        if self._proc is None or self._proc.poll() is not None:
            raise RuntimeError("QEMU 进程未运行")
        data = (cmd + "\n").encode()
        self._proc.stdin.write(data)
        self._proc.stdin.flush()

    def read_line(self, timeout=2.0):
        if self._proc is None or self._proc.poll() is not None:
            return None
        import select
        r, _, _ = select.select([self._proc.stdout], [], [], timeout)
        if not r:
            return None
        line = self._proc.stdout.readline()
        if not line:
            return None
        return line.decode(errors="replace").strip()

    def close(self):
        if self._proc is not None:
            try:
                self._proc.terminate()
                self._proc.wait(timeout=3)
            except Exception:
                self._proc.kill()
            self._proc = None


# ---------------------------------------------------------------------------
#  Serial (真实硬件)
# ---------------------------------------------------------------------------
class SerialTransport(BaseTransport):
    """pyserial 连真实硬件 debug UART."""

    name = "serial"

    def __init__(self, device="/dev/ttyUSB0", baud=115200, timeout=2.0):
        self.device = device
        self.baud = baud
        self.timeout = timeout
        self._serial = None

    def open(self):
        try:
            import serial
        except ImportError:
            raise RuntimeError("pyserial 未安装 (pip install pyserial)")
        self._serial = serial.Serial(self.device, self.baud,
                                     timeout=self.timeout)
        self._serial.reset_input_buffer()
        return self

    def send_command(self, cmd):
        if self._serial is None:
            raise RuntimeError("串口未打开")
        self._serial.write((cmd + "\n").encode())

    def read_line(self, timeout=2.0):
        if self._serial is None:
            return None
        line = self._serial.readline()
        if not line:
            return None
        return line.decode(errors="replace").strip()

    def close(self):
        if self._serial is not None:
            self._serial.close()
            self._serial = None


# ---------------------------------------------------------------------------
#  J-Link (烧录/复位)
# ---------------------------------------------------------------------------
class JLinkTransport(BaseTransport):
    """JLinkExe CLI 封装: 烧录 / 复位 / 执行命令."""

    name = "jlink"

    def __init__(self, device="S32K312", interface="SWD", speed=4000,
                 commander_script=None):
        self.device = device
        self.interface = interface
        self.speed = speed
        self.commander_script = commander_script
        self._jlink = shutil.which("JLinkExe")

    def open(self):
        if not self._jlink:
            raise RuntimeError("JLinkExe 未安装 (SEGGER J-Link software)")
        return self

    def run_commander(self, commands):
        """执行 J-Link Commander 命令序列, 返回输出."""
        script = "\n".join(commands) + "\nexit\n"
        result = subprocess.run(
            [self._jlink, "-device", self.device, "-if", self.interface,
             "-speed", str(self.speed), "-autoconnect", "1"],
            input=script, capture_output=True, text=True, timeout=120)
        return result.stdout + result.stderr

    def flash(self, binary, base_addr="0x00400000"):
        """烧录固件 (ELF 或 bin)."""
        out = self.run_commander([
            "h",
            "erase",
            f"loadfile {binary}",
            f"verifybin {binary} {base_addr}",
            "r", "g",
        ])
        return "Verified OK" in out or "O.K." in out or "Download" in out

    def reset(self):
        self.run_commander(["r"])

    def send_command(self, cmd):
        raise NotImplementedError("J-Link transport 不支持交互命令; "
                                  "用 flash()/reset()")

    def read_line(self, timeout=2.0):
        return None

    def close(self):
        pass


# ---------------------------------------------------------------------------
#  工厂
# ---------------------------------------------------------------------------
def create_transport(kind, **kwargs):
    """按名称创建 transport."""
    kinds = {
        "qemu": QemuTransport,
        "serial": SerialTransport,
        "jlink": JLinkTransport,
    }
    if kind not in kinds:
        raise ValueError(f"未知 transport: {kind} (可选: {list(kinds)})")
    return kinds[kind](**kwargs)
