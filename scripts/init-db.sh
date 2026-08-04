#!/bin/bash
# ─────────────────────────────────────────────────────────
# yuleDKCS Database Initialization Script
# 首次启动 PostgreSQL 时自动创建数据库并执行 migration
# ─────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Colors ──
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $1"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
err()   { echo -e "${RED}[ERROR]${NC} $1"; }

# ── Defaults ──
DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-5432}"
DB_USER="${DB_USER:-postgres}"
DB_PASSWORD="${DB_PASSWORD:-password}"
DB_NAME="${DB_NAME:-yuledkcs}"
DB_SSLMODE="${DB_SSLMODE:-disable}"
MAX_RETRIES="${MAX_RETRIES:-30}"

DSN="postgres://${DB_USER}:${DB_PASSWORD}@${DB_HOST}:${DB_PORT}/${DB_NAME}?sslmode=${DB_SSLMODE}"
ADMIN_DSN="postgres://${DB_USER}:${DB_PASSWORD}@${DB_HOST}:${DB_PORT}/postgres?sslmode=${DB_SSLMODE}"

# ── Help ──
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Initialize yuleDKCS database and run migrations.

Options:
  --host HOST           PostgreSQL host (default: localhost)
  --port PORT           PostgreSQL port (default: 5432)
  --user USER           PostgreSQL user (default: postgres)
  --password PASSWORD   PostgreSQL password (default: password)
  --dbname DBNAME       Database name (default: yuledkcs)
  --sslmode SSLMODE     SSL mode (default: disable)
  --build               Build migration tool from source
  -h, --help            Show this help
EOF
    exit 0
}

BUILD=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)     DB_HOST="$2"; shift 2 ;;
        --port)     DB_PORT="$2"; shift 2 ;;
        --user)     DB_USER="$2"; shift 2 ;;
        --password) DB_PASSWORD="$2"; shift 2 ;;
        --dbname)   DB_NAME="$2"; shift 2 ;;
        --sslmode)  DB_SSLMODE="$2"; shift 2 ;;
        --build)    BUILD=true ;;
        -h|--help)  usage ;;
        *)          err "Unknown option: $1"; usage ;;
    esac
done

echo -e "\n${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  🗄️   yuleDKCS Database Initialization${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}\n"

# ── Step 1: Wait for PostgreSQL ──
info "Waiting for PostgreSQL at ${DB_HOST}:${DB_PORT}..."
RETRY=0
while [[ $RETRY -lt $MAX_RETRIES ]]; do
    if pg_isready -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" 2>/dev/null; then
        ok "PostgreSQL is ready"
        break
    fi
    RETRY=$((RETRY + 1))
    sleep 2
done
if [[ $RETRY -eq $MAX_RETRIES ]]; then
    err "PostgreSQL not reachable after $((MAX_RETRIES * 2)) seconds"
    exit 1
fi

# ── Step 2: Create database if it doesn't exist ──
info "Ensuring database '${DB_NAME}' exists..."
DB_EXISTS=$(psql "$ADMIN_DSN" -t -c "SELECT 1 FROM pg_database WHERE datname='${DB_NAME}'" 2>/dev/null || echo "")
if [[ -z "$DB_EXISTS" ]]; then
    psql "$ADMIN_DSN" -c "CREATE DATABASE ${DB_NAME}" 2>/dev/null
    ok "Database '${DB_NAME}' created"
else
    ok "Database '${DB_NAME}' already exists"
fi

# ── Step 3: Build migration tool (optional) ──
MIGRATION_DIR="$PROJECT_DIR/backend/hub/migrations"
MIGRATE_BIN="/tmp/yule-migrate"

if $BUILD; then
    info "Building migration tool..."
    cd "$MIGRATION_DIR"
    go build -o "$MIGRATE_BIN" .
    ok "Migration tool built: $MIGRATE_BIN"
    cd "$PROJECT_DIR"
    MIGRATE_CMD="$MIGRATE_BIN"
else
    MIGRATE_CMD="go run $MIGRATION_DIR/run_migrations.go"
fi

# ── Step 4: Run migrations ──
info "Running database migrations..."
if $BUILD; then
    "$MIGRATE_CMD" -dsn "$DSN" -dir "$MIGRATION_DIR"
else
    cd "$PROJECT_DIR"
    $MIGRATE_CMD -dsn "$DSN" -dir "$MIGRATION_DIR"
fi
ok "Database migrations applied successfully"

# ── Summary ──
echo -e "\n${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✅  yuleDKCS Database Initialization Complete${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo ""
echo "   Host:     ${DB_HOST}:${DB_PORT}"
echo "   Database: ${DB_NAME}"
echo "   User:     ${DB_USER}"
echo "   DSN:      ${DSN}"
echo ""
