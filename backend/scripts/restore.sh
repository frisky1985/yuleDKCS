#!/bin/bash
# 数据库恢复脚本

set -e

# 配置
DB_HOST=${DB_HOST:-localhost}
DB_PORT=${DB_PORT:-5432}
DB_USER=${DB_USER:-yuledkcs}
DB_NAME=${DB_NAME:-yuledkcs}

# 检查参数
if [ $# -eq 0 ]; then
    echo "Usage: $0 <backup_file>"
    echo ""
    echo "Available backups:"
    ls -1t /var/backups/yuledkcs/*.sql.gz 2>/dev/null || echo "  No backups found"
    exit 1
fi

BACKUP_FILE="$1"

# 检查文件是否存在
if [ ! -f "$BACKUP_FILE" ]; then
    echo "ERROR: Backup file not found: $BACKUP_FILE"
    exit 1
fi

echo "==> WARNING: This will restore database '$DB_NAME' from backup!"
echo "    Backup file: $BACKUP_FILE"
read -p "Are you sure? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# 设置密码
if [ -n "$DB_PASSWORD" ]; then
    export PGPASSWORD="$DB_PASSWORD"
fi

# 先创建数据库 (如果不存在)
echo "==> Creating database if not exists..."
psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d postgres <<EOF
DROP DATABASE IF EXISTS ${DB_NAME}_temp;
CREATE DATABASE ${DB_NAME}_temp;
EOF

# 恢复到临时数据库
echo "==> Restoring to temporary database..."
gunzip -c "$BACKUP_FILE" | psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "${DB_NAME}_temp"

# 检查恢复是否成功
echo "==> Verifying restore..."
TABLE_COUNT=$(psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "${DB_NAME}_temp" -t -c "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='public';")

if [ -z "$TABLE_COUNT" ] || [ "$TABLE_COUNT" -eq 0 ]; then
    echo "ERROR: Restore verification failed!"
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d postgres -c "DROP DATABASE ${DB_NAME}_temp;"
    exit 1
fi

echo "    Tables restored: $TABLE_COUNT"

# 切换数据库
echo "==> Switching databases..."
psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d postgres <<EOF
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname='$DB_NAME';
DROP DATABASE IF EXISTS ${DB_NAME}_old;
ALTER DATABASE $DB_NAME RENAME TO ${DB_NAME}_old;
ALTER DATABASE ${DB_NAME}_temp RENAME TO $DB_NAME;
EOF

echo "==> Database restored successfully!"
echo "    Old database saved as: ${DB_NAME}_old"
echo ""
echo "To rollback: ALTER DATABASE ${DB_NAME}_old RENAME TO $DB_NAME;"
