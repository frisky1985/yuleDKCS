# yuleDKCS Backend Makefile
.PHONY: build run test migrate seed clean dev

# 默认配置
DB_URL=postgres://yuledkcs:yuledkcs_dev@localhost:5432/yuledkcs?sslmode=disable

# 构建
build:
	go build -o bin/api main.go

# 运行
run:
	go run main.go

# 开发模式（带热重载）
dev:
	air

# 测试
test:
	go test -v ./...

test-coverage:
	go test -coverprofile=coverage.out ./...
	go tool cover -html=coverage.out -o coverage.html

# 数据库迁移
migrate-up:
	psql $(DB_URL) -f database/migrations/001_initial_schema.sql

migrate-down:
	psql $(DB_URL) -c "DROP TABLE IF EXISTS key_usage_logs, key_shares, keys, vehicles, users CASCADE;"

# 种子数据
seed:
	psql $(DB_URL) -f database/seeds/001_test_data.sql

# 重置数据库
reset-db: migrate-down migrate-up seed

# Docker 操作
docker-up:
	cd ../deploy && docker-compose -f docker-compose.dev.yml up -d

docker-down:
	cd ../deploy && docker-compose -f docker-compose.dev.yml down

docker-logs:
	cd ../deploy && docker-compose -f docker-compose.dev.yml logs -f

# 代码质量
lint:
	golangci-lint run ./...

fmt:
	go fmt ./...

# 清理
clean:
	rm -rf bin/
	rm -f coverage.out coverage.html

# 帮助
help:
	@echo "可用命令:"
	@echo "  make build        - 构建项目"
	@echo "  make run          - 运行项目"
	@echo "  make dev          - 开发模式（热重载）"
	@echo "  make test         - 运行测试"
	@echo "  make migrate-up   - 执行数据库迁移"
	@echo "  make seed         - 导入种子数据"
	@echo "  make reset-db     - 重置数据库"
	@echo "  make docker-up    - 启动 Docker 环境"
	@echo "  make docker-down  - 停止 Docker 环境"
	@echo "  make lint         - 代码检查"
	@echo "  make fmt          - 格式化代码"
