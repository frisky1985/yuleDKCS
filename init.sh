#!/bin/bash
#
# yuleDKCS Environment Initialization Script
# Usage: ./init.sh [backend|frontend|mobile|embedded|all]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "$1 is not installed. Please install it first."
        return 1
    fi
    log_info "$1 is installed: $(command -v "$1")"
}

init_backend() {
    log_info "Initializing Backend (Go)..."
    cd "$SCRIPT_DIR/backend"
    
    check_command go
    check_command golangci-lint
    
    log_info "Downloading Go dependencies..."
    go mod download
    
    log_info "Running lint check..."
    golangci-lint run --timeout 5m || log_warn "Linting found issues"
    
    log_info "Running tests..."
    go test -short ./... || log_warn "Some tests failed"
    
    log_info "Backend initialized successfully!"
    echo "  Run: cd backend && go run ./cmd/api"
}

init_frontend() {
    log_info "Initializing Frontend (TypeScript)..."
    cd "$SCRIPT_DIR/frontend"
    
    check_command node
    check_command npm
    
    log_info "Installing npm dependencies..."
    npm install
    
    log_info "Running type check..."
    npm run type-check || log_warn "Type check found issues"
    
    log_info "Running linter..."
    npm run lint || log_warn "Linting found issues"
    
    log_info "Frontend initialized successfully!"
    echo "  Run: cd frontend && npm run dev"
}

init_mobile() {
    log_info "Initializing Mobile SDKs..."
    
    # iOS
    if command -v swift &> /dev/null; then
        log_info "Initializing iOS SDK..."
        cd "$SCRIPT_DIR/mobile/ios"
        swift build || log_warn "iOS build had issues"
        log_info "iOS SDK initialized"
    else
        log_warn "Swift not installed, skipping iOS"
    fi
    
    # Android
    if command -v java &> /dev/null; then
        log_info "Initializing Android SDK..."
        cd "$SCRIPT_DIR/mobile/android"
        if [ -f "./gradlew" ]; then
            ./gradlew build || log_warn "Android build had issues"
            log_info "Android SDK initialized"
        else
            log_warn "Gradle wrapper not found, skipping Android"
        fi
    else
        log_warn "Java not installed, skipping Android"
    fi
}

init_embedded() {
    log_info "Initializing Embedded (KW47)..."
    cd "$SCRIPT_DIR/embedded"
    
    check_command cmake
    check_command make
    
    if command -v arm-none-eabi-gcc &> /dev/null; then
        log_info "ARM toolchain found"
    else
        log_warn "ARM toolchain not found. Install from: https://developer.arm.com/downloads/-/gnu-rm"
    fi
    
    log_info "Creating build directory..."
    mkdir -p build && cd build
    cmake .. || log_warn "CMake configuration had issues"
    make || log_warn "Build had issues"
    
    log_info "Embedded initialized successfully!"
}

health_check() {
    log_info "Running health checks..."
    
    # Check required files exist
    [ -f "$SCRIPT_DIR/AGENTS.md" ] || log_warn "AGENTS.md not found"
    [ -f "$SCRIPT_DIR/backend/go.mod" ] || log_warn "Backend go.mod not found"
    [ -f "$SCRIPT_DIR/frontend/package.json" ] || log_warn "Frontend package.json not found"
    
    # Check Docker (optional)
    if command -v docker &> /dev/null; then
        if docker info &> /dev/null; then
            log_info "Docker is available"
        else
            log_warn "Docker installed but not running"
        fi
    fi
    
    log_info "Health checks complete"
}

print_usage() {
    cat << EOF
Usage: ./init.sh [COMMAND]

Commands:
  backend    Initialize Go backend only
  frontend   Initialize TypeScript frontend only
  mobile     Initialize Mobile SDKs (iOS + Android)
  embedded   Initialize Embedded firmware only
  all        Initialize all components (default)
  health     Run health checks only
  help       Show this help message

Examples:
  ./init.sh              # Initialize all components
  ./init.sh backend      # Initialize backend only
  ./init.sh health       # Run health checks

Environment Setup:
  - Go 1.21+ (backend)
  - Node.js 20+ (frontend)
  - Xcode 15+ (iOS)
  - Android Studio (Android)
  - ARM GCC toolchain (embedded)
  - Docker (optional, for local services)

EOF
}

# Main
main() {
    log_info "yuleDKCS Environment Initialization"
    echo "======================================"
    
    case "${1:-all}" in
        backend)
            init_backend
            ;;
        frontend)
            init_frontend
            ;;
        mobile)
            init_mobile
            ;;
        embedded)
            init_embedded
            ;;
        all)
            health_check
            init_backend
            init_frontend
            init_mobile
            init_embedded
            log_info "All components initialized!"
            ;;
        health)
            health_check
            ;;
        help|--help|-h)
            print_usage
            ;;
        *)
            log_error "Unknown command: $1"
            print_usage
            exit 1
            ;;
    esac
    
    echo ""
    log_info "Initialization complete!"
}

main "$@"
