#!/usr/bin/env python3
"""
test_flash_generator.py — 烧录脚本生成器单元测试

覆盖:
  - J-Link 脚本生成 (内容/设备/接口/烧录命令)
  - 批次 manifest (固件信息/哈希/设备列表/密钥)
  - 烧录日志 CSV (追加/字段/DRY_RUN 记录)
  - prepare CLI 端到端 (验签失败拒绝 / 正常生成)
  - flash CLI dry-run
"""

import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import flash_generator
import sign_firmware
from fw_header import FirmwareHeader


def make_bundle(tmpdir):
    """生成 dev 密钥 + 固件包, 返回 (sk, ek, pk, pkg_path)."""
    key_dir = os.path.join(tmpdir, "keys")
    os.makedirs(key_dir, exist_ok=True)
    sk_path = os.path.join(key_dir, "sign_key.pem")
    sign_firmware.generate_signing_key(sk_path)
    pk_path = sk_path.replace("_key.pem", "_pub.pem")
    ek_path = os.path.join(key_dir, "enc_key.bin")
    sign_firmware.generate_enc_key(ek_path)
    sk = sign_firmware.load_signing_key(sk_path)
    ek = sign_firmware.load_enc_key(ek_path)
    pk = flash_generator.load_pub(pk_path)

    fw = os.path.join(tmpdir, "fw.bin")
    with open(fw, "wb") as f:
        f.write(os.urandom(2048))
    pkg = sign_firmware.build_package(open(fw, "rb").read(), sk, ek, 2, 1)
    pkg_path = os.path.join(tmpdir, "fw.ydk")
    with open(pkg_path, "wb") as f:
        f.write(pkg)
    return sk, ek, pk, pkg_path


class TestJLinkScript(unittest.TestCase):
    def test_script_content(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "flash.jlink")
            flash_generator.generate_jlink_script(
                "/path/to/fw_plain.bin", out,
                device="S32K312", interface="SWD", speed=4000,
                base_addr="0x00400000")
            content = open(out).read()
            self.assertIn("device S32K312", content)
            self.assertIn("if SWD", content)
            self.assertIn("speed 4000", content)
            self.assertIn("erase", content)
            self.assertIn("loadfile /path/to/fw_plain.bin", content)
            self.assertIn("verifybin /path/to/fw_plain.bin 0x00400000", content)
            self.assertIn("r", content)
            self.assertIn("g", content)
            self.assertIn("exit", content)

    def test_script_custom_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "flash.jlink")
            flash_generator.generate_jlink_script(
                "fw.bin", out, device="STM32L552", interface="JTAG",
                speed=1000, base_addr="0x08000000")
            content = open(out).read()
            self.assertIn("device STM32L552", content)
            self.assertIn("if JTAG", content)
            self.assertIn("verifybin fw.bin 0x08000000",
                          content.replace(os.path.abspath("fw.bin"), "fw.bin"))


class TestManifest(unittest.TestCase):
    def test_manifest_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, _, _, pkg_path = make_bundle(tmp)
            header = FirmwareHeader.parse(open(pkg_path, "rb").read())
            plain = os.path.join(tmp, "fw_plain.bin")
            with open(plain, "wb") as f:
                f.write(b"\xAB" * 512)
            out = os.path.join(tmp, "manifest.json")
            m = flash_generator.generate_manifest(
                pkg_path, plain, header, "dev", "B20260810-01",
                ["DK-0001", "DK-0002"], out)
            self.assertEqual(m["batch_id"], "B20260810-01")
            self.assertEqual(m["firmware"]["version"], "2.1")
            self.assertEqual(m["devices"], ["DK-0001", "DK-0002"])
            self.assertEqual(m["keys"]["enc_key_id"], "dev")
            self.assertEqual(len(m["firmware"]["package_sha256"]), 64)
            # 重新加载验证 JSON 有效性
            loaded = json.load(open(out))
            self.assertEqual(loaded["firmware"]["plain_size"], 512)


class TestFlashLog(unittest.TestCase):
    def test_log_append(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = os.path.join(tmp, "flash_log.csv")
            flash_generator.log_flash_entry(
                log, "B1", "DK-0001", "2.1", "a" * 64, "DRY_RUN", "no hw")
            flash_generator.log_flash_entry(
                log, "B1", "DK-0002", "2.1", "a" * 64, "PASSED", "ok")
            rows = list(csv.reader(open(log)))
            self.assertEqual(rows[0], flash_generator.LOG_HEADER)
            self.assertEqual(len(rows), 3)
            self.assertEqual(rows[1][2], "DK-0001")
            self.assertEqual(rows[1][5], "DRY_RUN")
            self.assertEqual(rows[2][5], "PASSED")

    def test_flash_dry_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            script = os.path.join(tmp, "flash.jlink")
            with open(script, "w") as f:
                f.write("device S32K312\n")
            log = os.path.join(tmp, "flash_log.csv")
            ok, detail = flash_generator.flash(
                script, "DK-0001", log, "B1", "2.1", "a" * 64, dry_run=True)
            self.assertTrue(ok)
            self.assertEqual(detail, "DRY_RUN")
            rows = list(csv.reader(open(log)))
            self.assertEqual(rows[1][2], "DK-0001")
            self.assertEqual(rows[1][5], "DRY_RUN")

    def test_flash_missing_script(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = os.path.join(tmp, "flash_log.csv")
            ok, detail = flash_generator.flash(
                "/nonexistent/script.jlink", "DK-0001", log,
                "B1", "2.1", "a" * 64, dry_run=True)
            self.assertFalse(ok)
            self.assertIn("不存在", detail)
            rows = list(csv.reader(open(log)))
            self.assertEqual(rows[1][5], "ERROR")


class TestCLI(unittest.TestCase):
    def test_prepare_end_to_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, _, pk, pkg_path = make_bundle(tmp)
            key_dir = os.path.join(tmp, "keys")
            out = os.path.join(tmp, "out")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__),
                                              "flash_generator.py"),
                 "prepare",
                 "--package", pkg_path,
                 "--pub-key", os.path.join(key_dir, "sign_pub.pem"),
                 "--enc-key", os.path.join(key_dir, "enc_key.bin"),
                 "--out-dir", out, "--batch", "B20260810-01",
                 "--device-ids", "DK-0001,DK-0002"],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertTrue(os.path.exists(os.path.join(out, "fw_plain.bin")))
            self.assertTrue(os.path.exists(os.path.join(out, "fw.jlink")))
            m = json.load(open(os.path.join(out, "manifest.json")))
            self.assertEqual(m["batch_id"], "B20260810-01")
            self.assertEqual(m["devices"], ["DK-0001", "DK-0002"])

    def test_prepare_rejects_tampered(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, _, pk, pkg_path = make_bundle(tmp)
            # 篡改 payload
            data = bytearray(open(pkg_path, "rb").read())
            data[128] ^= 0xFF
            bad = os.path.join(tmp, "bad.ydk")
            open(bad, "wb").write(bytes(data))
            key_dir = os.path.join(tmp, "keys")
            out = os.path.join(tmp, "out")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__),
                                              "flash_generator.py"),
                 "prepare",
                 "--package", bad,
                 "--pub-key", os.path.join(key_dir, "sign_pub.pem"),
                 "--enc-key", os.path.join(key_dir, "enc_key.bin"),
                 "--out-dir", out, "--batch", "B-bad"],
                capture_output=True, text=True)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("验签", r.stderr)

    def test_flash_cli_dry_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "out")
            os.makedirs(out)
            script = os.path.join(out, "fw.jlink")
            with open(script, "w") as f:
                f.write("device S32K312\n")
            # manifest 供 flash 读取批次/版本
            json.dump({"batch_id": "B1",
                       "firmware": {"version": "2.1",
                                    "package_sha256": "b" * 64}},
                      open(os.path.join(out, "manifest.json"), "w"))
            log = os.path.join(tmp, "flash_log.csv")
            r = subprocess.run(
                [sys.executable, os.path.join(os.path.dirname(__file__),
                                              "flash_generator.py"),
                 "flash", "--script", script, "--device-id", "DK-0001",
                 "--log", log, "--dry-run"],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            rows = list(csv.reader(open(log)))
            self.assertEqual(rows[1][5], "DRY_RUN")
            self.assertEqual(rows[1][2], "DK-0001")
            self.assertEqual(rows[1][1], "B1")


if __name__ == "__main__":
    unittest.main()
