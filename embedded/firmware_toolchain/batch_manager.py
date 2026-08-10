#!/usr/bin/env python3
"""
batch_manager.py — yuleDKCS 生产批次管理 (B3-A, 工厂侧本地)

SQLite 数据模型 (与云端 batch-api 对齐, 迁移一致):

  batches       批次注册表: 批次号/固件版本/包哈希/密钥ID/设备清单/状态
  flash_records 烧录记录:   设备/版本/哈希/结果/哈希链防篡改
  devices       设备状态:   生命周期 PENDING→FLASHED→VERIFIED→SHIPPED/SCRAPPED
  key_usage     密钥审计:   签名/加密密钥使用记录

防篡改: 每条烧录记录 record_hash = sha256(prev_hash|batch|device|result|time),
        verify-chain 重算校验, 篡改即断裂。

CLI:
  init-db / batch create|list|show / record add / batch stats
  device status / verify-chain / report / import-csv / export-api-payload
"""

import argparse
import csv
import hashlib
import json
import os
import sqlite3
import sys
from datetime import datetime, timezone

SCHEMA = """
CREATE TABLE IF NOT EXISTS batches (
    id               TEXT PRIMARY KEY,
    firmware_version TEXT NOT NULL,
    package_sha256   TEXT NOT NULL,
    signing_key_id   TEXT NOT NULL,
    enc_key_id       TEXT NOT NULL,
    jlink_cfg        TEXT,
    planned_devices  TEXT,
    status           TEXT NOT NULL DEFAULT 'active',
    created_at       TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS flash_records (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_id         TEXT NOT NULL REFERENCES batches(id),
    device_id        TEXT NOT NULL,
    firmware_version TEXT NOT NULL,
    package_sha256   TEXT NOT NULL,
    result           TEXT NOT NULL,
    detail           TEXT,
    flashed_at       TEXT NOT NULL,
    prev_hash        TEXT NOT NULL,
    record_hash      TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS devices (
    device_id          TEXT PRIMARY KEY,
    batch_id           TEXT,
    status             TEXT NOT NULL DEFAULT 'PENDING',
    last_flash_result  TEXT,
    se_key_injected    INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS key_usage (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    key_id   TEXT NOT NULL,
    batch_id TEXT NOT NULL,
    purpose  TEXT NOT NULL,
    used_at  TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_records_batch ON flash_records(batch_id);
CREATE INDEX IF NOT EXISTS idx_records_device ON flash_records(device_id);
"""

DEVICE_STATUS = ("PENDING", "FLASHED", "VERIFIED", "SHIPPED", "SCRAPPED")


def now_iso():
    return datetime.now(timezone.utc).isoformat()


def record_hash(prev_hash, batch_id, device_id, result, flashed_at,
                version, sha):
    """哈希链: 记录哈希覆盖前序哈希 + 本记录全字段."""
    payload = "|".join([prev_hash, batch_id, device_id, result,
                        flashed_at, version, sha])
    return hashlib.sha256(payload.encode()).hexdigest()


class BatchDB:
    def __init__(self, db_path):
        self.db_path = db_path
        self._conn = sqlite3.connect(db_path)
        self._conn.executescript(SCHEMA)
        self._conn.commit()

    def close(self):
        self._conn.close()

    # ------------------------------------------------------------------
    #  批次
    # ------------------------------------------------------------------
    def create_batch(self, batch_id, version, pkg_sha, sign_key, enc_key,
                     devices=None, jlink_cfg=None):
        if self._conn.execute(
                "SELECT 1 FROM batches WHERE id=?", (batch_id,)).fetchone():
            raise ValueError(f"批次已存在: {batch_id}")
        self._conn.execute(
            "INSERT INTO batches (id, firmware_version, package_sha256, "
            "signing_key_id, enc_key_id, jlink_cfg, planned_devices, "
            "status, created_at) VALUES (?,?,?,?,?,?,?,?,?)",
            (batch_id, version, pkg_sha, sign_key, enc_key,
             json.dumps(jlink_cfg) if jlink_cfg else None,
             json.dumps(devices) if devices else None,
             "active", now_iso()))
        for d in devices or []:
            self._conn.execute(
                "INSERT OR IGNORE INTO devices (device_id, batch_id, status) "
                "VALUES (?,?,?)", (d, batch_id, "PENDING"))
        self._conn.commit()
        return batch_id

    def list_batches(self):
        return self._conn.execute(
            "SELECT id, firmware_version, status, created_at, "
            "(SELECT COUNT(*) FROM flash_records r WHERE r.batch_id=b.id) "
            "FROM batches b ORDER BY created_at DESC").fetchall()

    def get_batch(self, batch_id):
        row = self._conn.execute(
            "SELECT * FROM batches WHERE id=?", (batch_id,)).fetchone()
        if not row:
            return None
        cols = [d[0] for d in self._conn.execute(
            "SELECT * FROM batches WHERE id=?", (batch_id,)).description]
        return dict(zip(cols, row))

    def close_batch(self, batch_id):
        cur = self._conn.execute(
            "UPDATE batches SET status='closed' WHERE id=?", (batch_id,))
        self._conn.commit()
        return cur.rowcount > 0

    # ------------------------------------------------------------------
    #  烧录记录 (哈希链)
    # ------------------------------------------------------------------
    def add_record(self, batch_id, device_id, result, version, sha,
                   detail=""):
        if result not in ("PASSED", "FAILED", "DRY_RUN", "ERROR"):
            raise ValueError(f"非法结果: {result}")
        batch = self.get_batch(batch_id)
        if batch is None:
            raise ValueError(f"批次不存在: {batch_id}")
        # 前序哈希 (批次内最后一条)
        prev = self._conn.execute(
            "SELECT record_hash FROM flash_records WHERE batch_id=? "
            "ORDER BY id DESC LIMIT 1", (batch_id,)).fetchone()
        prev_hash = prev[0] if prev else "GENESIS"
        ts = now_iso()
        rh = record_hash(prev_hash, batch_id, device_id, result, ts,
                         version, sha)
        self._conn.execute(
            "INSERT INTO flash_records (batch_id, device_id, "
            "firmware_version, package_sha256, result, detail, "
            "flashed_at, prev_hash, record_hash) VALUES (?,?,?,?,?,?,?,?,?)",
            (batch_id, device_id, version, sha, result, detail, ts,
             prev_hash, rh))
        # 设备状态联动
        if result == "PASSED":
            status = "VERIFIED" if detail.startswith("verify") else "FLASHED"
            self._conn.execute(
                "INSERT INTO devices (device_id, batch_id, status, "
                "last_flash_result) VALUES (?,?,?,?) "
                "ON CONFLICT(device_id) DO UPDATE SET "
                "batch_id=excluded.batch_id, status=excluded.status, "
                "last_flash_result=excluded.last_flash_result",
                (device_id, batch_id, status, result))
        else:
            self._conn.execute(
                "INSERT INTO devices (device_id, batch_id, status, "
                "last_flash_result) VALUES (?,?,?,?) "
                "ON CONFLICT(device_id) DO UPDATE SET "
                "last_flash_result=excluded.last_flash_result",
                (device_id, batch_id, "PENDING", result))
        self._conn.commit()
        return rh

    def batch_records(self, batch_id):
        return self._conn.execute(
            "SELECT device_id, firmware_version, package_sha256, result, "
            "detail, flashed_at, record_hash FROM flash_records "
            "WHERE batch_id=? ORDER BY id", (batch_id,)).fetchall()

    # ------------------------------------------------------------------
    #  哈希链校验
    # ------------------------------------------------------------------
    def verify_chain(self, batch_id=None):
        """重算整条哈希链, 返回 (ok, 断点数)."""
        if batch_id:
            rows = self._conn.execute(
                "SELECT id, batch_id, device_id, result, flashed_at, "
                "firmware_version, package_sha256, prev_hash, record_hash "
                "FROM flash_records WHERE batch_id=? ORDER BY id",
                (batch_id,)).fetchall()
        else:
            rows = self._conn.execute(
                "SELECT id, batch_id, device_id, result, flashed_at, "
                "firmware_version, package_sha256, prev_hash, record_hash "
                "FROM flash_records ORDER BY id").fetchall()
        broken = 0
        prev_hash = "GENESIS"
        for r in rows:
            _, bid, dev, res, ts, ver, sha, ph, rh = r
            if ph != prev_hash:
                broken += 1
            expect = record_hash(prev_hash, bid, dev, res, ts, ver, sha)
            if expect != rh:
                broken += 1
            prev_hash = rh
        return broken == 0, broken

    # ------------------------------------------------------------------
    #  良率统计
    # ------------------------------------------------------------------
    def batch_stats(self, batch_id):
        rows = self.batch_records(batch_id)
        total = len(rows)
        if total == 0:
            return {"batch": batch_id, "total": 0, "yield": 0.0,
                    "by_result": {}, "failed_devices": []}
        by_result = {}
        for r in rows:
            by_result[r[3]] = by_result.get(r[3], 0) + 1
        passed = by_result.get("PASSED", 0)
        failed_devices = sorted({r[0] for r in rows if r[3] != "PASSED"})
        return {
            "batch": batch_id,
            "total": total,
            "passed": passed,
            "yield_pct": round(passed / total * 100, 1),
            "by_result": by_result,
            "failed_devices": failed_devices,
        }

    # ------------------------------------------------------------------
    #  设备
    # ------------------------------------------------------------------
    def device_status(self, device_id):
        row = self._conn.execute(
            "SELECT * FROM devices WHERE device_id=?", (device_id,)).fetchone()
        if not row:
            return None
        cols = [d[0] for d in self._conn.execute(
            "SELECT * FROM devices WHERE device_id=?", (device_id,)).description]
        return dict(zip(cols, row))

    def set_device_state(self, device_id, status):
        if status not in DEVICE_STATUS:
            raise ValueError(f"非法状态: {status}")
        self._conn.execute(
            "UPDATE devices SET status=? WHERE device_id=?", (status, device_id))
        self._conn.commit()

    # ------------------------------------------------------------------
    #  密钥审计
    # ------------------------------------------------------------------
    def log_key_usage(self, key_id, batch_id, purpose):
        self._conn.execute(
            "INSERT INTO key_usage (key_id, batch_id, purpose, used_at) "
            "VALUES (?,?,?,?)", (key_id, batch_id, purpose, now_iso()))
        self._conn.commit()

    def key_usage_report(self):
        return self._conn.execute(
            "SELECT key_id, purpose, COUNT(*) as cnt, "
            "GROUP_CONCAT(DISTINCT batch_id) FROM key_usage "
            "GROUP BY key_id, purpose").fetchall()

    # ------------------------------------------------------------------
    #  导入/导出
    # ------------------------------------------------------------------
    def import_csv(self, csv_path):
        """导入 flash_generator 的 flash_log.csv 到批次记录."""
        imported = 0
        with open(csv_path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                batch = row.get("batch", "")
                device = row.get("device_id", "")
                if not batch or not device:
                    continue
                try:
                    self.add_record(
                        batch, device,
                        row.get("result", "ERROR"),
                        row.get("firmware_version", "unknown"),
                        row.get("package_sha256", "unknown"),
                        detail=row.get("detail", ""))
                    imported += 1
                except ValueError:
                    continue
        return imported

    def export_api_payload(self, batch_id):
        """生成上传云端 batch-api 的 JSON 载荷."""
        batch = self.get_batch(batch_id)
        if not batch:
            return None
        return {
            "batch_id": batch["id"],
            "firmware_version": batch["firmware_version"],
            "package_sha256": batch["package_sha256"],
            "signing_key_id": batch["signing_key_id"],
            "enc_key_id": batch["enc_key_id"],
            "records": [
                {"device_id": r[0], "result": r[3], "detail": r[4],
                 "flashed_at": r[5], "record_hash": r[6]}
                for r in self.batch_records(batch_id)
            ],
            "stats": self.batch_stats(batch_id),
        }


# ---------------------------------------------------------------------------
#  CLI
# ---------------------------------------------------------------------------
def main(argv=None):
    parser = argparse.ArgumentParser(description="yuleDKCS 生产批次管理 (B3-A)")
    parser.add_argument("--db", default="batch.db")
    sub = parser.add_subparsers(dest="cmd")

    sub.add_parser("init-db", help="初始化数据库")

    b = sub.add_parser("batch", help="批次操作")
    b.add_argument("action", choices=["create", "list", "show", "close"])
    b.add_argument("--id", dest="bid")
    b.add_argument("--version", default="1.0.0")
    b.add_argument("--pkg-sha", default="")
    b.add_argument("--sign-key-id", default="dev")
    b.add_argument("--enc-key-id", default="dev")
    b.add_argument("--devices", default="", help="逗号分隔")

    r = sub.add_parser("record", help="烧录记录")
    r.add_argument("action", choices=["add"])
    r.add_argument("--batch", required=True)
    r.add_argument("--device", required=True)
    r.add_argument("--result", required=True,
                   choices=["PASSED", "FAILED", "DRY_RUN", "ERROR"])
    r.add_argument("--version", default="1.0.0")
    r.add_argument("--sha", default="")
    r.add_argument("--detail", default="")

    s = sub.add_parser("stats", help="批次良率统计")
    s.add_argument("--batch", required=True)

    d = sub.add_parser("device", help="设备状态")
    d.add_argument("action", choices=["status", "set"])
    d.add_argument("--device", required=True)
    d.add_argument("--state", choices=list(DEVICE_STATUS))

    sub.add_parser("verify-chain", help="校验烧录记录哈希链")

    rep = sub.add_parser("report", help="导出报表")
    rep.add_argument("--batch", required=True)
    rep.add_argument("--format", choices=["json", "csv"], default="json")

    imp = sub.add_parser("import-csv", help="导入 flash_log.csv")
    imp.add_argument("--csv", required=True)

    exp = sub.add_parser("export-api-payload", help="生成云端 API 载荷")
    exp.add_argument("--batch", required=True)

    args = parser.parse_args(argv)
    if not args.cmd:
        parser.print_help()
        return 1

    db = BatchDB(args.db)
    try:
        if args.cmd == "init-db":
            print(f"[OK] 数据库已初始化: {args.db}")
        elif args.cmd == "batch":
            if args.action == "create":
                devices = [x.strip() for x in args.devices.split(",") if x.strip()]
                db.create_batch(args.bid, args.version, args.pkg_sha,
                                args.sign_key_id, args.enc_key_id, devices)
                print(f"[OK] 批次 {args.bid} 创建 (设备 {len(devices)} 台)")
            elif args.action == "list":
                for row in db.list_batches():
                    print(f"  {row[0]:<16} v{row[1]:<8} {row[2]:<8} "
                          f"{row[4]:>3} 条  {row[3][:19]}")
            elif args.action == "show":
                b = db.get_batch(args.bid)
                if not b:
                    print(f"[FAIL] 批次不存在: {args.bid}", file=sys.stderr)
                    return 1
                print(json.dumps(b, indent=2, ensure_ascii=False))
            elif args.action == "close":
                print(f"[OK] 批次关闭: {db.close_batch(args.bid)}")
        elif args.cmd == "record":
            rh = db.add_record(args.batch, args.device, args.result,
                               args.version, args.sha, args.detail)
            print(f"[OK] 记录已写入 {args.batch}/{args.device}: "
                  f"{args.result} (hash={rh[:16]}...)")
        elif args.cmd == "stats":
            print(json.dumps(db.batch_stats(args.batch),
                             indent=2, ensure_ascii=False))
        elif args.cmd == "device":
            if args.action == "status":
                s = db.device_status(args.device)
                print(json.dumps(s, indent=2, ensure_ascii=False)
                      if s else f"[FAIL] 设备不存在: {args.device}")
            elif args.action == "set":
                db.set_device_state(args.device, args.state)
                print(f"[OK] 设备 {args.device} → {args.state}")
        elif args.cmd == "verify-chain":
            ok, broken = db.verify_chain()
            print(f"[{'OK' if ok else 'FAIL'}] 哈希链校验: "
                  f"{'完整' if ok else f'{broken} 处断裂'}")
            return 0 if ok else 1
        elif args.cmd == "report":
            stats = db.batch_stats(args.batch)
            if args.format == "json":
                print(json.dumps(stats, indent=2, ensure_ascii=False))
            else:
                print("device_id,result,flashed_at")
                for r in db.batch_records(args.batch):
                    print(f"{r[0]},{r[3]},{r[5]}")
        elif args.cmd == "import-csv":
            n = db.import_csv(args.csv)
            print(f"[OK] 导入 {n} 条记录")
        elif args.cmd == "export-api-payload":
            payload = db.export_api_payload(args.batch)
            if not payload:
                print(f"[FAIL] 批次不存在: {args.batch}", file=sys.stderr)
                return 1
            print(json.dumps(payload, indent=2, ensure_ascii=False))
        return 0
    finally:
        db.close()


if __name__ == "__main__":
    sys.exit(main())
