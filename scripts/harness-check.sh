#!/bin/bash
#
# Harness Compliance Check Script
# Validates that code meets harness requirements before PR
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ERRORS=0
WARNINGS=0

log_pass() { echo -e "${GREEN}✓${NC} $1"; }
log_fail() { echo -e "${RED}✗${NC} $1"; ((ERRORS++)); }
log_warn() { echo -e "${YELLOW}!${NC} $1"; ((WARNINGS++)); }

echo "=================================="
echo "yuleDKCS Harness Compliance Check"
echo "=================================="
echo ""

# 1. Check required files exist
echo "Checking required files..."
[ -f "AGENTS.md" ] && log_pass "AGENTS.md exists" || log_fail "AGENTS.md missing"
[ -f "CLAUDE.md" ] && log_pass "CLAUDE.md exists" || log_warn "CLAUDE.md missing"
[ -f ".cursorrules" ] && log_pass ".cursorrules exists" || log_warn ".cursorrules missing"
[ -f "init.sh" ] && log_pass "init.sh exists" || log_warn "init.sh missing"

echo ""
echo "Checking subdirectory AGENTS.md..."
[ -f "backend/AGENTS.md" ] && log_pass "backend/AGENTS.md" || log_warn "backend/AGENTS.md missing"
[ -f "frontend/AGENTS.md" ] && log_pass "frontend/AGENTS.md" || log_warn "frontend/AGENTS.md missing"
[ -f "embedded/AGENTS.md" ] && log_pass "embedded/AGENTS.md" || log_warn "embedded/AGENTS.md missing"

echo ""

# 2. Backend checks
echo "Checking Backend (Go)..."
if [ -d "backend" ]; then
    cd backend
    
    # Check go.mod
    [ -f "go.mod" ] && log_pass "go.mod exists" || log_fail "go.mod missing"
    
    # Check linter config
    [ -f ".golangci.yml" ] && log_pass ".golangci.yml exists" || log_warn ".golangci.yml missing"
    
    # Run go mod verify
    go mod verify > /dev/null 2>&1 && log_pass "go modules verified" || log_warn "go modules verification failed"
    
    # Run tests
    if go test -short ./... > /dev/null 2>&1; then
        log_pass "unit tests pass"
    else
        log_warn "some unit tests failed"
    fi
    
    cd ..
else
    log_warn "backend directory not found"
fi

echo ""

# 3. Frontend checks
echo "Checking Frontend (TypeScript)..."
if [ -d "frontend" ]; then
    cd frontend
    
    # Check package.json
    [ -f "package.json" ] && log_pass "package.json exists" || log_fail "package.json missing"
    
    # Check lock file
    [ -f "package-lock.json" ] && log_pass "package-lock.json exists" || log_warn "package-lock.json missing (run npm install)"
    
    cd ..
else
    log_warn "frontend directory not found"
fi

echo ""

# 4. CI/CD checks
echo "Checking CI/CD configuration..."
[ -d ".github/workflows" ] && log_pass ".github/workflows exists" || log_warn "GitHub Actions not configured"
[ -f ".github/pull_request_template.md" ] && log_pass "PR template exists" || log_warn "PR template missing"

echo ""

# 5. Documentation checks
echo "Checking Documentation..."
[ -d "docs" ] && log_pass "docs/ directory exists" || log_warn "docs/ directory missing"
[ -f "docs/architecture.md" ] && log_pass "architecture.md exists" || log_warn "architecture.md missing"

echo ""

# 6. Security checks
echo "Running security checks..."

# Check for common secrets patterns
if grep -r "password.*=" --include="*.go" --include="*.ts" --include="*.js" . 2>/dev/null | grep -v "_test\." | grep -v "example" | head -5; then
    log_warn "Possible hardcoded passwords found (check above)"
else
    log_pass "no obvious hardcoded passwords"
fi

# Check for .env files not in .gitignore
if [ -f ".env" ] && ! grep -q "\.env" .gitignore 2>/dev/null; then
    log_warn ".env file exists but not in .gitignore"
else
    log_pass ".env handling looks correct"
fi

echo ""
echo "=================================="
echo "Harness Check Complete"
echo "=================================="
echo ""
echo "Results:"
echo "  Errors: $ERRORS"
echo "  Warnings: $WARNINGS"
echo ""

if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}All critical checks passed!${NC}"
    exit 0
else
    echo -e "${RED}Some checks failed. Please fix before PR.${NC}"
    exit 1
fi
