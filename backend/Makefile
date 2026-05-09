# yuleDKCS Backend Makefile
.PHONY: all build test clean run lint docker migrate

# 变量定义
APP_NAME := yuledkcs-backend
VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME := $(shell date -u '+%Y-%m-%d_%H:%M:%S')
COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GO_VERSION := $(shell go version | cut -d ' ' -f 3)

# Go 构建参数
LDFLAGS := -X main.Version=$(VERSION) \
           -X main.BuildTime=$(BUILD_TIME) \
           -X main.Commit=$(COMMIT) \
           -X main.GoVersion=$(GO_VERSION)

# 默认目标
all: lint test build

# 构建应用
build:
	@echo "==> Building $(APP_NAME)..."
	go build -ldflags "$(LDFLAGS)" -o bin/$(APP_NAME) ./cmd/api

# 开发模式运行 (使用 Air 热重载)
dev:
	@echo "==> Running in dev mode with hot reload..."
	air -c .air.toml

# 运行应用
run:
	@echo "==> Running $(APP_NAME)..."
	go run ./cmd/api

# 运行所有测试
test:
	@echo "==> Running tests..."
	go test -v -race -coverprofile=coverage.out ./...
	@echo "==> Coverage report:"
	go tool cover -func=coverage.out | tail -n 1

# 运行单元测试
test-unit:
	@echo "==> Running unit tests..."
	go test -v -short ./...

# 运行集成测试
test-integration:
	@echo "==> Running integration tests..."
	go test -v -tags=integration ./tests/integration/...

# 生成覆盖率报告
test-coverage:
	@echo "==> Generating coverage report..."
	go test -coverprofile=coverage.out ./...
	go tool cover -html=coverage.out -o coverage.html
	@echo "==> Coverage report generated: coverage.html"

# 代码质量检查
lint:
	@echo "==> Running linter..."
	golangci-lint run ./...

# 格式化代码
fmt:
	@echo "==> Formatting code..."
	go fmt ./...

# 下载依赖
deps:
	@echo "==> Downloading dependencies..."
	go mod download
	go mod tidy

# 更新依赖
update-deps:
	@echo "==> Updating dependencies..."
	go get -u ./...
	go mod tidy

# 清理
 clean:
	@echo "==> Cleaning..."
	rm -rf bin/
	rm -f coverage.out coverage.html
	go clean -cache

# Docker 构建
docker-build:
	@echo "==> Building Docker image..."
	docker build -t $(APP_NAME):$(VERSION) .

# Docker 运行
docker-run:
	@echo "==> Running Docker container..."
	docker run -p 8080:8080 --env-file .env $(APP_NAME):$(VERSION)

# 数据库迁移
migrate-up:
	@echo "==> Running database migrations..."
	migrate -path migrations -database "$(DATABASE_URL)" up

migrate-down:
	@echo "==> Rolling back database migrations..."
	migrate -path migrations -database "$(DATABASE_URL)" down 1

migrate-create:
	@read -p "Enter migration name: " name; \
	migrate create -ext sql -dir migrations -seq $$name

migrate-status:
	@echo "==> Checking migration status..."
	migrate -path migrations -database "$(DATABASE_URL)" version

# 生成代码
generate:
	@echo "==> Generating code..."
	go generate ./...

# 安全扫描
security:
	@echo "==> Running security scan..."
	gosec -fmt sarif -out security.sarif ./...

# 性能分析
profile:
	@echo "==> Starting profile server..."
	go tool pprof -http=:8081 http://localhost:8080/debug/pprof/heap

# 构建二进制 (多平台)
build-all:
	@echo "==> Building for all platforms..."
	GOOS=linux GOARCH=amd64 go build -ldflags "$(LDFLAGS)" -o bin/$(APP_NAME)-linux-amd64 ./cmd/api
	GOOS=linux GOARCH=arm64 go build -ldflags "$(LDFLAGS)" -o bin/$(APP_NAME)-linux-arm64 ./cmd/api
	GOOS=darwin GOARCH=amd64 go build -ldflags "$(LDFLAGS)" -o bin/$(APP_NAME)-darwin-amd64 ./cmd/api
	GOOS=darwin GOARCH=arm64 go build -ldflags "$(LDFLAGS)" -o bin/$(APP_NAME)-darwin-arm64 ./cmd/api

# 帮助
help:
	@echo "Available targets:"
	@echo "  make build          - Build the application"
	@echo "  make dev            - Run with hot reload (Air)"
	@echo "  make run            - Run the application"
	@echo "  make test           - Run all tests"
	@echo "  make test-unit      - Run unit tests only"
	@echo "  make test-coverage  - Generate coverage report"
	@echo "  make lint           - Run linter"
	@echo "  make fmt            - Format code"
	@echo "  make deps           - Download dependencies"
	@echo "  make clean          - Clean build artifacts"
	@echo "  make docker-build   - Build Docker image"
	@echo "  make migrate-up     - Run database migrations"
	@echo "  make migrate-down   - Rollback migrations"
	@echo "  make security       - Run security scan"
	@echo "  make help           - Show this help"
