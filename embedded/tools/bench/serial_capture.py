#!/usr/bin/env python3
"""G1 台架串口日志采集工具。

用途：在真机 Bring-up 验证时，把串口输出按时间戳落盘，并允许操作者
在关键步骤插入标记，最终生成可直接归档的证据包：

    <out>/serial.log    带时间戳的串口原始输出
    <out>/steps.tsv     步骤标记（时间、步骤号、说明、操作者）
    <out>/meta.json     环境元数据、固件哈希、统计与日志 SHA-256

仅依赖 Python 3 标准库（POSIX termios），适用于 Linux/macOS。
Windows 请在 WSL 中运行，或使用支持 COMx 的采集工具。

示例：
    python3 serial_capture.py --port /dev/ttyUSB0 --baud 115200 \\
        --firmware embedded/firmware/build-arm/src/yuleDKCS_fw.elf \\
        --label G1-EVB01

运行中在终端输入 `3.5.2 新密钥重建 SCP03` 回车即可插入标记；
输入 `q` 回车、Ctrl-C 或到达 --duration 后结束并完成归档。
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import platform
import select
import signal
import stat
import sys
import termios
import time
from pathlib import Path

VERSION = "1.0.0"

_STEP_FILE_HEADER = "timestamp\tstep\tnote\toperator\n"


def _now_iso() -> str:
    return _dt.datetime.now().astimezone().isoformat(timespec="milliseconds")


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _git_commit(repo: Path) -> str:
    """读取仓库提交号；失败时返回 unknown，不阻断采集。"""
    head = repo / ".git" / "HEAD"
    try:
        text = head.read_text(encoding="ascii").strip()
        if text.startswith("ref: "):
            ref = repo / ".git" / text[5:]
            return ref.read_text(encoding="ascii").strip()
        return text
    except OSError:
        return "unknown"


def _baud_constant(baud: int) -> int:
    name = f"B{baud}"
    try:
        return getattr(termios, name)
    except AttributeError:
        supported = sorted(
            int(k[1:]) for k in dir(termios)
            if k.startswith("B") and k[1:].isdigit()
        )
        raise SystemExit(
            f"不支持的波特率 {baud}；可用: {supported}"
        )


def _configure_tty(fd: int, baud: int) -> None:
    """8N1、无流控、原始模式。"""
    speed = _baud_constant(baud)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, _ispeed, _ospeed, cc = attrs

    iflag &= ~(
        termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP
        | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON
        | termios.IXOFF | termios.IXANY
    )
    oflag &= ~termios.OPOST
    lflag &= ~(
        termios.ECHO | termios.ECHONL | termios.ICANON
        | termios.ISIG | termios.IEXTEN
    )
    cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0

    attrs[0] = iflag
    attrs[1] = oflag
    attrs[2] = cflag
    attrs[3] = lflag
    attrs[4] = speed  # ispeed
    attrs[5] = speed  # ospeed
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def _stdin_monitored() -> bool:
    """仅当 stdin 是终端或管道时监听标记输入，避免 /dev/null 忙轮询。"""
    try:
        mode = os.fstat(sys.stdin.fileno()).st_mode
    except (OSError, ValueError):
        return False
    return sys.stdin.isatty() or stat.S_ISFIFO(mode)


def _timestamped(text: str) -> str:
    return f"{_now_iso()} | {text}"


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="G1 台架串口日志采集（带时间戳、步骤标记与元数据归档）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--port", required=True, help="串口设备，如 /dev/ttyUSB0")
    p.add_argument("--baud", type=int, default=115200, help="波特率")
    p.add_argument("--out", default=None,
                   help="证据目录（默认 bench-evidence/G1-<本地时间>）")
    p.add_argument("--duration", type=float, default=0.0,
                   help="采集时长（秒）；0 表示一直采集到 q/Ctrl-C")
    p.add_argument("--label", default="", help="台架/样机标签，如 G1-EVB01")
    p.add_argument("--operator", default=os.environ.get("USER", "unknown"),
                   help="操作者")
    p.add_argument("--firmware", default=None,
                   help="被测固件文件，用于记录 SHA-256")
    p.add_argument("--echo", action="store_true", help="同时把串口输出回显到终端")
    p.add_argument("--quiet", action="store_true", help="不打印启动横幅")
    p.add_argument("--version", action="version", version=VERSION)
    return p


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)

    if args.duration < 0:
        print("错误：--duration 不能为负", file=sys.stderr)
        return 2

    repo = Path(__file__).resolve().parents[3]
    out_dir = Path(
        args.out or f"bench-evidence/G1-{_dt.datetime.now():%Y%m%d-%H%M%S}"
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    serial_log = out_dir / "serial.log"
    steps_file = out_dir / "steps.tsv"
    meta_file = out_dir / "meta.json"

    try:
        fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    except OSError as exc:
        print(
            f"错误：无法打开 {args.port}: {exc}\n"
            "提示：确认设备存在，并将当前用户加入 dialout 组（或使用 sudo）。",
            file=sys.stderr,
        )
        return 2

    try:
        _configure_tty(fd, args.baud)
    except (termios.error, SystemExit) as exc:
        os.close(fd)
        print(f"错误：配置串口失败: {exc}", file=sys.stderr)
        return 2

    if not args.quiet:
        print(f"[serial_capture {VERSION}] port={args.port} baud={args.baud} 8N1")
        print(f"[serial_capture] 证据目录: {out_dir.resolve()}")
        if _stdin_monitored() and args.duration == 0.0:
            print("[serial_capture] 输入 `步骤号 说明` 回车插入标记；`q` 回车结束。")
        elif args.duration > 0:
            print(f"[serial_capture] 采集 {args.duration:g}s 后自动结束。")
        elif not _stdin_monitored():
            print("[serial_capture] 非交互环境且未指定 --duration，拒绝无限采集。",
                  file=sys.stderr)
            os.close(fd)
            return 2

    start_wall = _dt.datetime.now().astimezone()
    start_mono = time.monotonic()
    stop = False
    exit_code = 0

    def _handle_signal(_signum, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    byte_count = 0
    line_count = 0
    steps: list[dict] = []
    partial = ""
    stdin_buf = ""
    monitor_stdin = _stdin_monitored()

    try:
        with serial_log.open("a", encoding="utf-8", buffering=1) as log, \
                steps_file.open("a", encoding="utf-8", buffering=1) as step_out:
            if steps_file.stat().st_size == 0:
                step_out.write(_STEP_FILE_HEADER)

            log.write(_timestamped(
                f"===== CAPTURE START port={args.port} baud={args.baud} "
                f"label={args.label or '-'} operator={args.operator} ====="
            ) + "\n")

            while not stop:
                elapsed = time.monotonic() - start_mono
                if args.duration > 0.0 and elapsed >= args.duration:
                    break

                watch = [fd]
                if monitor_stdin:
                    watch.append(sys.stdin.fileno())
                timeout = 0.2
                if args.duration > 0.0:
                    timeout = min(timeout, max(0.0, args.duration - elapsed))

                try:
                    readable, _, _ = select.select(watch, [], [], timeout)
                except InterruptedError:
                    continue

                if fd in readable:
                    try:
                        data = os.read(fd, 4096)
                    except BlockingIOError:
                        data = b""
                    except OSError as exc:
                        log.write(_timestamped(f"===== SERIAL ERROR: {exc} =====") + "\n")
                        exit_code = 1
                        break

                    if data:
                        byte_count += len(data)
                        text = partial + data.decode("utf-8", errors="replace")
                        lines = text.split("\n")
                        partial = lines.pop()
                        for line in lines:
                            line = line.rstrip("\r")
                            log.write(_timestamped(line) + "\n")
                            line_count += 1
                            if args.echo:
                                print(line)

                if monitor_stdin and sys.stdin.fileno() in readable:
                    try:
                        chunk = os.read(sys.stdin.fileno(), 4096)
                    except OSError:
                        chunk = b""
                    if not chunk:
                        monitor_stdin = False
                    else:
                        stdin_buf += chunk.decode("utf-8", errors="replace")
                        while "\n" in stdin_buf:
                            raw, stdin_buf = stdin_buf.split("\n", 1)
                            cmd = raw.strip()
                            if not cmd:
                                continue
                            if cmd.lower() in ("q", "quit", "exit"):
                                stop = True
                                break
                            parts = cmd.split(None, 1)
                            step_id = parts[0]
                            note = parts[1] if len(parts) > 1 else ""
                            ts = _now_iso()
                            steps.append({
                                "timestamp": ts,
                                "step": step_id,
                                "note": note,
                                "operator": args.operator,
                            })
                            step_out.write(
                                f"{ts}\t{step_id}\t{note}\t{args.operator}\n"
                            )
                            log.write(_timestamped(
                                f"===== MARKER {step_id} {note} ====="
                            ) + "\n")
                            print(f"[marker] {step_id} {note}")

            if partial:
                log.write(_timestamped(partial.rstrip("\r")) + "\n")
                line_count += 1
                partial = ""

            end_wall = None

            fw_meta: dict | None = None
            if args.firmware:
                fw_path = Path(args.firmware)
                if fw_path.is_file():
                    fw_meta = {
                        "path": str(fw_path.resolve()),
                        "size": fw_path.stat().st_size,
                        "sha256": _sha256_file(fw_path),
                    }
                else:
                    fw_meta = {"path": str(fw_path), "error": "not found"}

            log.write(_timestamped(
                f"===== CAPTURE END bytes={byte_count} lines={line_count} "
                f"steps={len(steps)} ====="
            ) + "\n")
            log.flush()
            log_sha = _sha256_file(serial_log)
            (out_dir / "serial.log.sha256").write_text(
                f"{log_sha}  {serial_log.name}\n", encoding="utf-8"
            )
            end_wall = _dt.datetime.now().astimezone()

            meta = {
                "tool": "serial_capture.py",
                "version": VERSION,
                "start": start_wall.isoformat(timespec="seconds"),
                "end": end_wall.isoformat(timespec="seconds"),
                "duration_s": round(time.monotonic() - start_mono, 3),
                "host": platform.node(),
                "platform": platform.platform(),
                "operator": args.operator,
                "label": args.label,
                "port": args.port,
                "baud": args.baud,
                "framing": "8N1",
                "git_commit": _git_commit(repo),
                "firmware": fw_meta,
                "bytes": byte_count,
                "lines": line_count,
                "steps": steps,
                "serial_log": {
                    "file": serial_log.name,
                    "sha256": log_sha,
                },
            }
            meta_file.write_text(
                json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )

        if not args.quiet:
            print(f"[serial_capture] 完成：{serial_log} ({line_count} 行)")
            print(f"[serial_capture] 元数据：{meta_file}")
            print(f"[serial_capture] 日志 SHA-256：{log_sha}")
    finally:
        os.close(fd)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
