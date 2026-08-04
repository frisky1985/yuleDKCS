#!/bin/bash
# ─────────────────────────────────────────────────────────
# yuleDKCS Production Startup Script
# 启动所有生产服务：Hub gRPC + REST + PostgreSQL + Redis + Kafka
# ─────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPOSE_FILE="$PROJECT_DIR/docker-compose.prod.yml"

# ── Colors ──
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

info()  { echo -e "${CYAN}[INFO]${NC}  $1"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
err()   { echo -e "${RED}[ERROR]${NC} $1"; }

# ── Help ──
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Start yuleDKCS production services.

Options:
  --build           Build images before starting
  --no-kafka        Skip Kafka/Zookeeper (for dev/test with external Kafka)
  --skip-infra      Skip infrastructure (DB, Redis, Kafka) — start hub only
  --tail            Tail logs after starting
  -h, --help        Show this help
EOF
    exit 0
}

# ── Parse args ──
BUILD=false
NO_KAFKA=false
SKIP_INFRA=false
TAIL=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)    BUILD=true ;;
        --no-kafka) NO_KAFKA=true ;;
        --skip-infra) SKIP_INFRA=true ;;
        --tail)     TAIL=true ;;
        -h|--help)  usage ;;
        *)          err "Unknown option: $1"; usage ;;
    esac
    shift
done

# ── Main ──
echo -e "\n${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  🚀  yuleDKCS Production Startup${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}\n"

cd "$PROJECT_DIR"

# ── Check compose file ──
if [[ ! -f "$COMPOSE_FILE" ]]; then
    err "Compose file not found: $COMPOSE_FILE"
    exit 1
fi

# ── Build (optional) ──
if $BUILD; then
    info "Building Hub image..."
    docker compose -f "$COMPOSE_FILE" build hub
    ok "Build complete"
fi

# ── Start infrastructure ──
if ! $SKIP_INFRA; then
    info "Starting PostgreSQL..."
    docker compose -f "$COMPOSE_FILE" up -d postgres
    ok "PostgreSQL started"

    info "Starting Redis..."
    docker compose -f "$COMPOSE_FILE" up -d redis
    ok "Redis started"

    if ! $NO_KAFKA; then
        info "Starting Zookeeper..."
        docker compose -f "$COMPOSE_FILE" up -d zookeeper
        ok "Zookeeper started"

        info "Starting Kafka..."
        docker compose -f "$COMPOSE_FILE" up -d kafka
        ok "Kafka started"
    fi
fi

# ── Wait for dependencies ──
info "Waiting for PostgreSQL to become healthy..."
MAX_RETRIES=30
RETRY=0
while [[ $RETRY -lt $MAX_RETRIES ]]; do
    if docker compose -f "$COMPOSE_FILE" exec -T postgres pg_isready -U postgres -d yuledkcs 2>/dev/null; then
        ok "PostgreSQL is ready"
        break
    fi
    RETRY=$((RETRY + 1))
    sleep 2
done
if [[ $RETRY -eq $MAX_RETRIES ]]; then
    err "PostgreSQL failed to become ready within $((MAX_RETRIES * 2)) seconds"
    exit 1
fi

# ── Start Hub ──
info "Starting yuleHUB (gRPC :9090 + REST :8080)..."
docker compose -f "$COMPOSE_FILE" up -d hub
ok "yuleHUB started"

# ── Verify ──
sleep 3
info "Verifying services..."
docker compose -f "$COMPOSE_FILE" ps

# ── Health check ──
info "Probing /health endpoint..."
if docker compose -f "$COMPOSE_FILE" exec -T hub wget -q -O- http://localhost:8080/health 2>/dev/null; then
    ok "Health check passed"
else
    warn "Health check not yet responsive — service may still be starting"
fi

# ── Summary ──
echo -e "\n${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  ✅  yuleDKCS Production is running${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo ""
echo "   REST API:   http://localhost:8080/api/v1"
echo "   gRPC:       localhost:9090"
echo "   Health:     http://localhost:8080/health"
echo "   Healthz:    http://localhost:8080/healthz"
echo "   PostgreSQL: localhost:5432"
echo "   Redis:      localhost:6379"
echo "   Kafka:      localhost:9092"
echo ""

# ── Tail logs ──
if $TAIL; then
    docker compose -f "$COMPOSE_FILE" logs -f
fi
