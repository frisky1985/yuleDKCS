#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
# migrate-dkcs.sh — dkcs 服务 PostgreSQL schema 迁移脚本 (psql 方式)
#
# 用途: 手动 / CI 环境执行 db/migrations/*.up.sql (按文件名升序)。
#       dkcs 服务启动时 internal/migrate 也会自动执行同样的迁移,
#       本脚本用于无法自动迁移的场景 (如初始化裸库、离线部署)。
#
# 用法:
#   ./scripts/migrate-dkcs.sh [PGHOST] [PGPORT] [PGUSER] [PGDATABASE]
#   或通过环境变量 PGHOST/PGPORT/PGUSER/PGPASSWORD/PGDATABASE 提供
#   (与 lib/pq DSN 对应: DB_HOST/DB_PORT/DB_USER/DB_PASSWORD/DB_NAME 亦可)
#
# 依赖: psql (PostgreSQL 客户端)
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MIGRATIONS_DIR="${DB_MIGRATIONS_DIR:-$SCRIPT_DIR/../backend/db/migrations}"

# 连接参数: 兼容 lib/pq 风格环境变量 (DKCS 服务同款)
PGHOST="${PGHOST:-${DB_HOST:-localhost}}"
PGPORT="${PGPORT:-${DB_PORT:-5432}}"
PGUSER="${PGUSER:-${DB_USER:-digitalkey}}"
PGDATABASE="${PGDATABASE:-${DB_NAME:-digitalkey_db}}"
export PGPASSWORD="${PGPASSWORD:-${DB_PASSWORD:-}}"

command -v psql >/dev/null 2>&1 || { echo "错误: 未找到 psql, 请安装 PostgreSQL 客户端" >&2; exit 1; }
[[ -d "$MIGRATIONS_DIR" ]] || { echo "错误: 迁移目录不存在: $MIGRATIONS_DIR" >&2; exit 1; }

echo "==> 迁移目录: $MIGRATIONS_DIR"
echo "==> 目标数据库: $PGHOST:$PGPORT/$PGDATABASE (user=$PGUSER)"

# 确保版本表存在
psql -v ON_ERROR_STOP=1 -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDATABASE" \
  -c "CREATE TABLE IF NOT EXISTS schema_migrations (version TEXT PRIMARY KEY, applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW());"

applied=0
skipped=0
for file in "$MIGRATIONS_DIR"/*.up.sql; do
  [[ -e "$file" ]] || continue
  version="$(basename "$file" .up.sql)"
  exists="$(psql -At -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDATABASE" \
    -c "SELECT EXISTS (SELECT 1 FROM schema_migrations WHERE version = '$version');")"
  if [[ "$exists" == "t" ]]; then
    echo "==> 跳过 (已应用): $version"
    skipped=$((skipped + 1))
    continue
  fi
  echo "==> 应用迁移: $version"
  psql -v ON_ERROR_STOP=1 -1 -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDATABASE" -f "$file"
  psql -v ON_ERROR_STOP=1 -h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDATABASE" \
    -c "INSERT INTO schema_migrations (version) VALUES ('$version');"
  applied=$((applied + 1))
done

echo "==> 完成: 新应用 $applied, 跳过 $skipped"
