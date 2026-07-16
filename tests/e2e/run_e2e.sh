#!/bin/bash
set -e

# yuleDKCS E2E Verification Suite Runner
# Usage: ./run_e2e.sh [--no-cloud]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

CAR_SIM_PID=""
CLEANUP_ON_EXIT=true

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

cleanup() {
    if [ "$CLEANUP_ON_EXIT" = true ] && [ -n "$CAR_SIM_PID" ]; then
        echo -e "\n${YELLOW}🧹 Cleaning up...${NC}"
        kill "$CAR_SIM_PID" 2>/dev/null || true
        wait "$CAR_SIM_PID" 2>/dev/null || true
        echo -e "${GREEN}✅ Car simulator stopped${NC}"
    fi
}

trap cleanup EXIT

echo -e "${BLUE}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     🔥 yuleDKCS E2E Verification Suite        ║${NC}"
echo -e "${BLUE}║     8 Scenarios · Full Protocol Stack          ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════╝${NC}"
echo ""

# Optionally start cloud stack
if [ "$1" != "--no-cloud" ]; then
    echo -e "${YELLOW}🌐 Starting cloud backend (PostgreSQL, Redis)...${NC}"
    docker compose -f docker-compose.e2e.yml up -d db redis 2>/dev/null || \
        echo -e "${YELLOW}   ⚠️  Docker not available, skipping cloud services${NC}"
fi

# Build car simulator
echo -e "${YELLOW}🔨 Building car simulator...${NC}"
GOWORK=off go build -o /tmp/car-simulator ./carsim/
echo -e "${GREEN}✅ Car simulator built${NC}"

# Start car simulator
echo -e "${YELLOW}🚗 Starting car simulator on :18001...${NC}"
/tmp/car-simulator &
CAR_SIM_PID=$!
sleep 1

# Verify it's running
if ! nc -z localhost 18001 2>/dev/null; then
    echo -e "${RED}❌ Car simulator failed to start${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Car simulator running (PID: $CAR_SIM_PID)${NC}"
echo ""

# Run all E2E tests
echo -e "${YELLOW}🏃 Running 8 E2E scenarios...${NC}"
echo ""

cd scenarios
GOWORK=off go test -v -count=1 -timeout 120s ./...
TEST_EXIT=$?
cd ..

echo ""
if [ $TEST_EXIT -eq 0 ]; then
    echo -e "${GREEN}╔══════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  🎉 ALL 8 E2E SCENARIOS PASSED!               ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════╝${NC}"
else
    echo -e "${RED}╔══════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ❌ SOME SCENARIOS FAILED (exit: $TEST_EXIT)${NC}"
    echo -e "${RED}╚══════════════════════════════════════════════════╝${NC}"
fi

exit $TEST_EXIT
