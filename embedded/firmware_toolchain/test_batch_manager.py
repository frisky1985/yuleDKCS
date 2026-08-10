#!/usr/bin/env python3
"""
test_batch_manager.py — 批次管理器单元测试

覆盖:
  - 批次创建/列表/详情/关闭
  - 烧录记录 + 哈希链 (篡改检测)
  - 良率统计/失败设备
  - 设备状态机
  - 密钥使用审计
  - CSV 导入 / API 载荷导出
  - CLI 端到端
"""

import json
import os
import sqlite3
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from batch_manager import BatchDB, record_hash


def make_db(tmpdir):
    return BatchDB(os.path.join(tmpdir, "test.db"))


class TestBatchCRUD(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.db = make_db(self._tmp.name)

    def tearDown(self):
        self.db.close()
        self._tmp.cleanup()

    def test_create_and_get(self):
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev",
                             devices=["DK-0001", "DK-0002"])
        b = self.db.get_batch("B1")
        self.assertEqual(b["firmware_version"], "2.1")
        self.assertEqual(b["status"], "active")
        self.assertEqual(json.loads(b["planned_devices"]),
                         ["DK-0001", "DK-0002"])

    def test_duplicate_batch_rejected(self):
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev")
        with self.assertRaises(ValueError):
            self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev")

    def test_list_and_close(self):
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev")
        self.assertEqual(len(self.db.list_batches()), 1)
        self.assertTrue(self.db.close_batch("B1"))
        self.assertEqual(self.db.get_batch("B1")["status"], "closed")
        self.assertFalse(self.db.close_batch("NOPE"))


class TestFlashRecords(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.db = make_db(self._tmp.name)
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev",
                             devices=["DK-0001"])

    def tearDown(self):
        self.db.close()
        self._tmp.cleanup()

    def test_add_and_chain(self):
        h1 = self.db.add_record("B1", "DK-0001", "PASSED", "2.1", "a" * 64)
        h2 = self.db.add_record("B1", "DK-0002", "FAILED", "2.1", "a" * 64,
                                detail="verifybin mismatch")
        ok, broken = self.db.verify_chain("B1")
        self.assertTrue(ok)
        self.assertEqual(broken, 0)
        self.assertNotEqual(h1, h2)

    def test_tamper_detected(self):
        self.db.add_record("B1", "DK-0001", "PASSED", "2.1", "a" * 64)
        self.db.add_record("B1", "DK-0002", "FAILED", "2.1", "a" * 64)
        # 篡改第一条记录的结果
        self.db._conn.execute(
            "UPDATE flash_records SET result='PASSED' WHERE device_id='DK-0002'")
        self.db._conn.commit()
        ok, broken = self.db.verify_chain("B1")
        self.assertFalse(ok)
        self.assertGreaterEqual(broken, 1)

    def test_illegal_result_rejected(self):
        with self.assertRaises(ValueError):
            self.db.add_record("B1", "DK-0001", "BOGUS", "2.1", "a" * 64)

    def test_record_requires_batch(self):
        with self.assertRaises(ValueError):
            self.db.add_record("NOPE", "DK-0001", "PASSED", "2.1", "a" * 64)


class TestStats(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.db = make_db(self._tmp.name)
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev",
                             devices=[f"DK-{i:04d}" for i in range(5)])

    def tearDown(self):
        self.db.close()
        self._tmp.cleanup()

    def test_yield_stats(self):
        for i in range(5):
            result = "PASSED" if i < 4 else "FAILED"
            self.db.add_record("B1", f"DK-{i:04d}", result, "2.1", "a" * 64)
        stats = self.db.batch_stats("B1")
        self.assertEqual(stats["total"], 5)
        self.assertEqual(stats["passed"], 4)
        self.assertEqual(stats["yield_pct"], 80.0)
        self.assertEqual(stats["by_result"]["FAILED"], 1)
        self.assertEqual(stats["failed_devices"], ["DK-0004"])


class TestDevicesAndKeys(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.db = make_db(self._tmp.name)
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev")

    def tearDown(self):
        self.db.close()
        self._tmp.cleanup()

    def test_device_lifecycle(self):
        self.db.add_record("B1", "DK-0001", "PASSED", "2.1", "a" * 64)
        d = self.db.device_status("DK-0001")
        self.assertEqual(d["status"], "FLASHED")
        self.assertEqual(d["last_flash_result"], "PASSED")
        self.db.set_device_state("DK-0001", "SHIPPED")
        self.assertEqual(self.db.device_status("DK-0001")["status"], "SHIPPED")
        with self.assertRaises(ValueError):
            self.db.set_device_state("DK-0001", "BOGUS")

    def test_key_usage_audit(self):
        self.db.log_key_usage("dev", "B1", "signing")
        self.db.log_key_usage("dev", "B1", "signing")
        report = self.db.key_usage_report()
        self.assertEqual(len(report), 1)
        self.assertEqual(report[0][2], 2)


class TestIO(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.db = make_db(self._tmp.name)
        self.db.create_batch("B1", "2.1", "a" * 64, "dev", "dev",
                             devices=["DK-0001", "DK-0002"])

    def tearDown(self):
        self.db.close()
        self._tmp.cleanup()

    def test_import_csv(self):
        csv_path = os.path.join(self._tmp.name, "log.csv")
        with open(csv_path, "w", newline="") as f:
            f.write("timestamp,batch,device_id,firmware_version,"
                    "package_sha256,result,detail\n")
            f.write("2026-08-10T00:00:00+00:00,B1,DK-0001,2.1,"
                    "aaaaaaaa,DRY_RUN,no hw\n")
            f.write("2026-08-10T00:00:01+00:00,B1,DK-0002,2.1,"
                    "aaaaaaaa,PASSED,ok\n")
        n = self.db.import_csv(csv_path)
        self.assertEqual(n, 2)
        self.assertEqual(self.db.batch_stats("B1")["total"], 2)

    def test_export_api_payload(self):
        self.db.add_record("B1", "DK-0001", "PASSED", "2.1", "a" * 64)
        payload = self.db.export_api_payload("B1")
        self.assertEqual(payload["batch_id"], "B1")
        self.assertEqual(len(payload["records"]), 1)
        self.assertEqual(payload["records"][0]["result"], "PASSED")
        self.assertIn("record_hash", payload["records"][0])
        self.assertIn("stats", payload)


class TestCLI(unittest.TestCase):
    def test_end_to_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = os.path.join(tmp, "batch.db")
            script = os.path.join(os.path.dirname(__file__),
                                  "batch_manager.py")
            def run(*args):
                return subprocess.run(
                    [sys.executable, script, "--db", db, *args],
                    capture_output=True, text=True)

            r = run("init-db")
            self.assertEqual(r.returncode, 0, r.stderr)
            r = run("batch", "create", "--id", "B1", "--version", "2.1",
                    "--pkg-sha", "a" * 64, "--devices", "DK-0001,DK-0002")
            self.assertEqual(r.returncode, 0, r.stderr)
            r = run("record", "add", "--batch", "B1", "--device", "DK-0001",
                    "--result", "PASSED", "--version", "2.1", "--sha", "a" * 64)
            self.assertEqual(r.returncode, 0, r.stderr)
            r = run("record", "add", "--batch", "B1", "--device", "DK-0002",
                    "--result", "FAILED", "--version", "2.1", "--sha", "a" * 64)
            self.assertEqual(r.returncode, 0, r.stderr)
            r = run("stats", "--batch", "B1")
            self.assertEqual(r.returncode, 0, r.stderr)
            stats = json.loads(r.stdout)
            self.assertEqual(stats["total"], 2)
            self.assertEqual(stats["passed"], 1)
            r = run("verify-chain")
            self.assertEqual(r.returncode, 0, r.stdout)
            self.assertIn("完整", r.stdout)
            r = run("export-api-payload", "--batch", "B1")
            payload = json.loads(r.stdout)
            self.assertEqual(len(payload["records"]), 2)


if __name__ == "__main__":
    unittest.main()
