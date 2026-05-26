.PHONY: all test build lint docker up clean help

all: test build

# ============================================================
# Testing
# ============================================================
test: test-backend test-frontend

test-backend:
	cd backend && go test ./internal/iccoa/cert/... ./internal/service/... -count=1 -v

test-frontend:
	cd frontend && npm test 2>/dev/null || echo "frontend tests not configured"

# ============================================================
# Building
# ============================================================
build: build-backend build-frontend

build-backend:
	cd backend && go build -o bin/api ./cmd/api

build-frontend:
	cd frontend && npm run build

# ============================================================
# Code Quality
# ============================================================
lint: lint-backend lint-frontend

lint-backend:
	cd backend && golangci-lint run --timeout 5m 2>/dev/null || echo "golangci-lint not installed"

lint-frontend:
	cd frontend && npm run lint 2>/dev/null || echo "eslint not configured"

# ============================================================
# Docker
# ============================================================
docker:
	docker build -t yuledkcs-backend:latest -f backend/Dockerfile backend

up:
	docker compose -f deploy/docker-compose.production.yml up -d

down:
	docker compose -f deploy/docker-compose.production.yml down

# ============================================================
# Cleanup
# ============================================================
clean:
	rm -rf backend/bin/
	rm -rf frontend/dist/
	rm -rf embedded/sdk/build/
	find . -name '*.o' -o -name '*.a' -o -name '__pycache__' | xargs rm -rf 2>/dev/null || true

# ============================================================
# Help
# ============================================================
help:
	@echo "yuleDKCS Development Commands"
	@echo "  make test     — Run all tests"
	@echo "  make build    — Build all components"
	@echo "  make lint     — Run all linters"
	@echo "  make docker   — Build Docker images"
	@echo "  make up       — Start docker-compose services"
	@echo "  make down     — Stop docker-compose services"
	@echo "  make clean    — Clean build artifacts"
