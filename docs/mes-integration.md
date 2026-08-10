# yuleDKCS 生产批次管理 — MES 对接方案 (B3)

> 版本: v1.0 | 日期: 2026-08-10 | 状态: 接口就绪 (对接域名即可)

本文档定义 yuleDKCS 生产烧录批次管理与 MES (制造执行系统) 的对接方案。
工厂侧本地管理 (A) 与云端 API (B) 数据模型对齐, 哈希链算法一致。

---

## 1. 架构总览

```
┌─────────────────────┐    ┌──────────────────────┐    ┌──────────────┐
│ 产线工位 (工厂侧)    │    │  batch-api (云端)     │    │  MES         │
│                     │    │  backend/cloud/      │    │              │
│ flash_generator.py  │───▶│  batch-api           │───▶│ 生产任务下发  │
│   .ydk → 烧录+日志   │    │  REST API + 文件持久化│    │ 良率查询      │
│ batch_manager.py    │    │  域名: batch-api.xxx  │    │ 批次追溯      │
│   SQLite + 哈希链    │    │  (DNS/TLS/反向代理)   │    │              │
└─────────────────────┘    └──────────────────────┘    └──────────────┘
```

- **工厂侧 (A)**: `embedded/firmware_toolchain/` — 本地 SQLite, 离线可用,
  产线断网也能记录; 联网后通过 API 上报。
- **云端 (B)**: `backend/cloud/batch-api/` — REST API, 工位上报 + MES 拉取。
- **一致性**: 两端数据模型/哈希链算法相同, A 导出的载荷 B 直接接收。

---

## 2. 云端 API 契约 (batch-api)

### 2.1 通用

- Base URL: `https://<domain>/` (示例: `https://batch-api.yuletech.com`)
- 鉴权: HTTP Header `X-API-Key: <key>` (生产建议 mTLS/OAuth2, 见 §4)
- 内容类型: `application/json`
- 错误响应: `{"error": "<message>"}`
- 错误码: `400` 参数错误 / `401` 鉴权失败 / `404` 不存在 / `409` 冲突 / `500` 服务错误

### 2.2 端点

| 方法 | 路径 | 说明 | 请求体关键字段 |
|------|------|------|----------------|
| GET | `/healthz` | 健康检查 (免鉴权) | — |
| POST | `/api/v1/batches` | 创建批次 | `batch_id`, `firmware_version`, `package_sha256`, `signing_key_id`, `enc_key_id`, `planned_devices[]` |
| GET | `/api/v1/batches` | 批次列表 | — |
| GET | `/api/v1/batches/{id}` | 批次详情 | — |
| GET | `/api/v1/batches/{id}/stats` | 良率统计 | — |
| POST | `/api/v1/batches/{id}/records` | 烧录结果上报 | `device_id`, `result`(PASSED/FAILED/DRY_RUN/ERROR), `detail`, `firmware_version`, `package_sha256`, `flashed_at` |
| GET | `/api/v1/batches/{id}/records` | 烧录记录列表 | — |
| GET | `/api/v1/devices/{id}` | 设备状态查询 | — |

### 2.3 示例

```bash
# 创建批次 (MES 下发生产任务)
curl -X POST https://batch-api.yuletech.com/api/v1/batches \
  -H "X-API-Key: <key>" -H "Content-Type: application/json" \
  -d '{
    "batch_id": "B20260810-01",
    "firmware_version": "2.1.0",
    "package_sha256": "<64-hex>",
    "signing_key_id": "prod-sign-01",
    "enc_key_id": "prod-enc-01",
    "planned_devices": ["DK-0001", "DK-0002"]
  }'

# 工位上报烧录结果
curl -X POST https://batch-api.yuletech.com/api/v1/batches/B20260810-01/records \
  -H "X-API-Key: <key>" -H "Content-Type: application/json" \
  -d '{"device_id":"DK-0001","result":"PASSED","firmware_version":"2.1.0"}'

# MES 查询良率
curl https://batch-api.yuletech.com/api/v1/batches/B20260810-01/stats \
  -H "X-API-Key: <key>"
# → {"batch":"B20260810-01","total":1,"passed":1,"yield_pct":100,
#    "by_result":{"PASSED":1},"failed_devices":[]}
```

---

## 3. 工厂侧 (A) 使用

```bash
# 1. 初始化本地库
python3 batch_manager.py --db batch.db init-db

# 2. 导入 B2 烧录日志 (flash_generator 产生的 CSV)
python3 batch_manager.py --db batch.db import-csv --csv flash_log.csv

# 3. 生成上报云端载荷 (联网后)
python3 batch_manager.py --db batch.db export-api-payload --batch B20260810-01

# 4. 上报云端 (POST 到 batch-api)
curl -X POST https://batch-api.yuletech.com/api/v1/batches \
  -H "X-API-Key: <key>" -d @payload.json

# 5. 本地哈希链防篡改校验
python3 batch_manager.py --db batch.db verify-chain
```

---

## 4. 域名对接配置 (上线清单)

| # | 项 | 说明 |
|:-:|----|------|
| 1 | DNS | `batch-api.<company>.com` A/CNAME 指向 API 服务器 |
| 2 | TLS | 证书 (Let's Encrypt / 企业 CA), 强制 HTTPS |
| 3 | 反向代理 | Nginx/Caddy 转发 `/api/*` → `localhost:8080` |
| 4 | 环境变量 | `BATCH_API_PORT`(默认 8080), `BATCH_API_DATA_DIR`(默认 ./data), `BATCH_API_KEY`(生产必改) |
| 5 | 密钥管理 | 生产签名密钥入 HSM, API key 按工位/系统独立签发 |
| 6 | 数据备份 | data/ 目录每日快照 (批次 JSON 可完整重建) |
| 7 | 存储升级 | 文件 JSON → PostgreSQL/SQLite 时, schema 字段不变 (见 §5) |

---

## 5. 数据模型 (两端对齐)

```
batches        id / firmware_version / package_sha256 / signing_key_id /
               enc_key_id / planned_devices[] / status(active|closed) / created_at

flash_records  device_id / firmware_version / package_sha256 / result /
               detail / flashed_at / prev_hash / record_hash

devices        device_id / batch_id / status(PENDING→FLASHED→VERIFIED→
               SHIPPED|SCRAPPED) / last_flash_result / se_key_injected

key_usage      key_id / batch_id / purpose(signing|encryption) / used_at
```

### 哈希链防篡改 (审计要求)

```
record_hash = sha256(prev_hash | batch_id | device_id | result |
                     flashed_at | firmware_version | package_sha256)
首条 prev_hash = "GENESIS"
```

篡改任一条记录 → 后续全部断裂, `verify-chain` 检测。工厂本地与云端
实现一致 (`batch_manager.py` / `batch-api main.go`), 可交叉校验。

---

## 6. 数据流时序

1. **MES → batch-api**: 下发生产任务 (创建批次 + 设备清单)
2. **产线工位**: B2 验签解密 → J-Link 烧录 → 本地 batch_manager 记录
3. **工位 → batch-api**: 烧录结果上报 (逐台或批量, 幂等可重试)
4. **MES → batch-api**: 按批次查良率/按设备查状态 (追溯/放行)
5. **质量工程师**: 报表导出 (batch-api stats / 工厂本地 report)

---

## 7. 安全与合规

- 签名密钥永不落盘产线 (B1: 产线只持有公钥验签)
- 烧录日志哈希链防篡改 (审计可追溯)
- API 鉴权: X-API-Key 起步, 生产 mTLS/OAuth2
- 密钥使用审计: key_usage 表记录每次签名/加密批次
