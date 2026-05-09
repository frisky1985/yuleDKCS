#!/bin/bash
# 数据库备份脚本

set -e

# 配置
DB_HOST=${DB_HOST:-localhost}
DB_PORT=${DB_PORT:-5432}
DB_USER=${DB_USER:-yuledkcs}
DB_NAME=${DB_NAME:-yuledkcs}
BACKUP_DIR=${BACKUP_DIR:-"/var/backups/yuledkcs"}
RETENTION_DAYS=${RETENTION_DAYS:-30}
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="$BACKUP_DIR/${DB_NAME}_${DATE}.sql.gz"

# 创建备份目录
mkdir -p "$BACKUP_DIR"

echo "==> Starting database backup..."
echo "    Database: $DB_NAME"
echo "    Backup file: $BACKUP_FILE"

# 执行备份
if [ -n "$DB_PASSWORD" ]; then
    export PGPASSWORD="$DB_PASSWORD"
fi

pg_dump -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" \
    --verbose \
    --no-owner \
    --no-privileges \
    --format=plain |
    gzip > "$BACKUP_FILE"

echo "==> Backup completed: $BACKUP_FILE"

# 验证备份
if [ -f "$BACKUP_FILE" ]; then
    FILE_SIZE=$(stat -f%z "$BACKUP_FILE" 2>/dev/null || stat -c%s "$BACKUP_FILE" 2>/dev/null)
    echo "    File size: $FILE_SIZE bytes"
else
    echo "ERROR: Backup file not found!"
    exit 1
fi

# 清理旧备份
echo "==> Cleaning up old backups (older than $RETENTION_DAYS days)..."
find "$BACKUP_DIR" -name "${DB_NAME}_*.sql.gz" -mtime +$RETENTION_DAYS -delete

# 上传到远程存储 (如果配置了)
if [ -n "$S3_BUCKET" ]; then
    echo "==> Uploading to S3..."
    aws s3 cp "$BACKUP_FILE" "s3://$S3_BUCKET/backups/"
    echo "==> Upload completed"
fi

echo "==> Backup process completed successfully!"
