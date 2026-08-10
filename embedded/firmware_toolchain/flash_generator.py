#!/usr/bin/env python3
"""
flash_generator.py — yuleDKCS 生产烧录脚本生成器 (B2)

流程:
  1. 验签 + 解密 .ydk 固件包 (verify_firmware.verify_package)
  2. 生成 J-Link Commander 烧录脚本 (SWD/JTAG)
  3. 生成批次 manifest (批次号/固件版本/哈希/密钥 ID/设备列表)
  4. 记录烧录日志 (flash_log.csv, 追加式, 可追溯)

用法:
  # 生成烧录包 (验签+解密 → .jlink 脚本 + manifest)
  python3 flash_generator.py prepare --package fw.ydk \\
      --pub-key keys/dev_signing_pub.pem --enc-key keys/dev_enc_key.bin \\
      --out-dir out/ --batch B20260810-01 --device-id DK-0001 \\
      --jlink-device S32K312 --base-addr 0x00400000

  # 执行烧录 (需 J-Link; 无 JLinkExe 时 dry-run 并记录日志)
  python3 flash_generator.py flash --script out/flash_B20260810-01.jlink \\
      --device-id DK-0001 --log out/flash_log.csv
"""

import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone

from fw_header import FirmwareHeader, PACKAGE_EXT
from verify_firmware import verify_package, VerifyError

JLINK_DEFAULT_DEVICE = "S32K312"
JLINK_DEFAULT_IF = "SWD"
JLINK_DEFAULT_SPEED = 4000
JLINK_DEFAULT_BASE = "0x00400000"


# ---------------------------------------------------------------------------
#  烧录脚本生成
# ---------------------------------------------------------------------------
def generate_jlink_script(binary_path, out_path, device=JLINK_DEFAULT_DEVICE,
                          interface=JLINK_DEFAULT_IF, speed=JLINK_DEFAULT_SPEED,
                          base_addr=JLINK_DEFAULT_BASE, log_path=None):
    """生成 J-Link Commander 烧录脚本."""
    lines = [
        f"/* yuleDKCS 生产烧录脚本 — 由 flash_generator.py 自动生成 */",
        f"/* 固件: {os.path.basename(binary_path)} */",
        f"/* 生成时间: {datetime.now(timezone.utc).isoformat()} */",
        f"device {device}",
        f"if {interface}",
        f"speed {speed}",
        f"log {log_path or 'flash.log'}",
        f"connect",
        f"h",
        f"unlock kinetis",
        f"erase",
        f"loadfile {os.path.abspath(binary_path)}",
        f"verifybin {os.path.abspath(binary_path)} {base_addr}",
        f"r",
        f"g",
        f"exit",
        "",
    ]
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    return out_path


# ---------------------------------------------------------------------------
#  批次 manifest
# ---------------------------------------------------------------------------
def generate_manifest(package_path, plain_binary, header, enc_key_id,
                      batch, device_ids, out_path, jlink_cfg=None):
    """生成批次 manifest (JSON)."""
    with open(package_path, "rb") as f:
        pkg = f.read()
    with open(plain_binary, "rb") as f:
        plain = f.read()
    manifest = {
        "batch_id": batch,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "firmware": {
            "package": os.path.basename(package_path),
            "binary": os.path.basename(plain_binary),
            "version": f"{header.version_major}.{header.version_minor}",
            "algo": header.algo,
            "enc": header.enc,
            "package_sha256": hashlib.sha256(pkg).hexdigest(),
            "plain_sha256": hashlib.sha256(plain).hexdigest(),
            "plain_size": len(plain),
        },
        "keys": {"signing_pub": f"{enc_key_id}.pub", "enc_key_id": enc_key_id},
        "devices": device_ids,
        "jlink": jlink_cfg or {
            "device": JLINK_DEFAULT_DEVICE,
            "interface": JLINK_DEFAULT_IF,
            "speed": JLINK_DEFAULT_SPEED,
            "base_addr": JLINK_DEFAULT_BASE,
        },
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    return manifest


# ---------------------------------------------------------------------------
#  烧录日志
# ---------------------------------------------------------------------------
LOG_HEADER = ["timestamp", "batch", "device_id", "firmware_version",
              "package_sha256", "result", "detail"]


def log_flash_entry(log_path, batch, device_id, version, pkg_sha256,
                    result, detail=""):
    """追加一条烧录日志 (CSV)."""
    new_file = not os.path.exists(log_path)
    with open(log_path, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        if new_file:
            writer.writerow(LOG_HEADER)
        writer.writerow([
            datetime.now(timezone.utc).isoformat(),
            batch, device_id, version, pkg_sha256, result, detail,
        ])
    return log_path


# ---------------------------------------------------------------------------
#  执行烧录
# ---------------------------------------------------------------------------
def flash(script_path, device_id, log_path, batch, version, pkg_sha256,
          dry_run=True):
    """执行 J-Link 烧录; 无 JLinkExe 时 dry-run (记录日志)."""
    jlink = shutil_which("JLinkExe")
    if jlink is None or dry_run:
        # dry-run: 校验脚本存在, 记录日志 (无硬件环境可追溯)
        if not os.path.exists(script_path):
            log_flash_entry(log_path, batch, device_id, version, pkg_sha256,
                            "ERROR", f"脚本不存在: {script_path}")
            return False, "脚本不存在"
        log_flash_entry(log_path, batch, device_id, version, pkg_sha256,
                        "DRY_RUN", "无 JLinkExe, dry-run (需硬件 A2)")
        return True, "DRY_RUN"

    result = subprocess.run(
        [jlink, "-CommanderScript", script_path],
        capture_output=True, text=True, timeout=300)
    ok = result.returncode == 0 and ("Verified OK" in result.stdout
                                     or "O.K." in result.stdout)
    log_flash_entry(log_path, batch, device_id, version, pkg_sha256,
                    "PASSED" if ok else "FAILED",
                    result.stdout[-200:] if result.stdout else result.stderr[-200:])
    return ok, result.stdout[-200:] if result.stdout else ""


def shutil_which(cmd):
    import shutil
    return shutil.which(cmd)


# ---------------------------------------------------------------------------
#  CLI
# ---------------------------------------------------------------------------
def prepare(args):
    """验签+解密 → 生成 .jlink 脚本 + manifest."""
    with open(args.package, "rb") as f:
        pkg = f.read()
    public_key = load_pub(args.pub_key)
    with open(args.enc_key, "rb") as f:
        enc_key = f.read()
    try:
        plain = verify_package(pkg, public_key, enc_key)
    except VerifyError as e:
        print(f"[FAIL] 验签失败: {e}", file=sys.stderr)
        return 1

    header = FirmwareHeader.parse(pkg)
    os.makedirs(args.out_dir, exist_ok=True)
    base = os.path.join(args.out_dir,
                        f"{os.path.basename(args.package).replace(PACKAGE_EXT, '')}")
    bin_path = f"{base}_plain.bin"
    with open(bin_path, "wb") as f:
        f.write(plain)

    script_path = generate_jlink_script(
        bin_path, f"{base}.jlink",
        device=args.jlink_device, interface=args.jlink_if,
        speed=args.jlink_speed, base_addr=args.base_addr,
        log_path=os.path.join(args.out_dir, "jlink.log"))

    manifest = generate_manifest(
        args.package, bin_path, header, args.enc_key_id, args.batch,
        [d.strip() for d in args.device_ids.split(",") if d.strip()],
        os.path.join(args.out_dir, "manifest.json"),
        jlink_cfg={"device": args.jlink_device, "interface": args.jlink_if,
                   "speed": args.jlink_speed, "base_addr": args.base_addr})

    pkg_sha = hashlib.sha256(pkg).hexdigest()
    print(f"[OK] 验签通过 + 解密: {bin_path} ({len(plain)} B)")
    print(f"[OK] J-Link 脚本: {script_path}")
    print(f"[OK] manifest: {os.path.join(args.out_dir, 'manifest.json')}")
    print(f"[OK] 批次 {args.batch}, 固件 {header.version_major}.{header.version_minor}, "
          f"sha256={pkg_sha[:16]}...")
    return 0


def load_pub(path):
    from verify_firmware import load_public_key
    return load_public_key(path)


def do_flash(args):
    """执行烧录 + 记录日志."""
    # 从 manifest 取批次/版本/哈希 (脚本同目录)
    manifest_path = os.path.join(os.path.dirname(args.script),
                                 "manifest.json")
    batch = version = pkg_sha = "unknown"
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            m = json.load(f)
        batch = m.get("batch_id", "unknown")
        version = m.get("firmware", {}).get("version", "unknown")
        pkg_sha = m.get("firmware", {}).get("package_sha256", "unknown")
    ok, detail = flash(args.script, args.device_id, args.log,
                       batch, version, pkg_sha, dry_run=args.dry_run)
    print(f"[{'OK' if ok else 'FAIL'}] 烧录 {args.device_id}: {detail}")
    return 0 if ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description="yuleDKCS 生产烧录脚本生成器")
    sub = parser.add_subparsers(dest="cmd")

    p = sub.add_parser("prepare", help="验签+解密 → 生成烧录脚本 + manifest")
    p.add_argument("--package", required=True, help=".ydk 固件包")
    p.add_argument("--pub-key", required=True)
    p.add_argument("--enc-key", required=True)
    p.add_argument("--enc-key-id", default="dev")
    p.add_argument("--out-dir", required=True)
    p.add_argument("--batch", required=True, help="批次号, 如 B20260810-01")
    p.add_argument("--device-ids", default="DK-0001",
                   help="逗号分隔设备列表")
    p.add_argument("--jlink-device", default=JLINK_DEFAULT_DEVICE)
    p.add_argument("--jlink-if", default=JLINK_DEFAULT_IF)
    p.add_argument("--jlink-speed", type=int, default=JLINK_DEFAULT_SPEED)
    p.add_argument("--base-addr", default=JLINK_DEFAULT_BASE)

    f = sub.add_parser("flash", help="执行烧录 + 日志 (无 JLinkExe 时 dry-run)")
    f.add_argument("--script", required=True)
    f.add_argument("--device-id", required=True)
    f.add_argument("--log", required=True)
    f.add_argument("--dry-run", action="store_true",
                   help="强制 dry-run (不调用 JLinkExe)")

    args = parser.parse_args(argv)
    if args.cmd == "prepare":
        return prepare(args)
    if args.cmd == "flash":
        return do_flash(args)
    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
