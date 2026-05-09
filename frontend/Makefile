# yuleDKCS Frontend Makefile
.PHONY: all install dev build test lint clean

# 变量
APP_NAME := yuledkcs-frontend
NODE_VERSION := $(shell node -v 2>/dev/null || echo "unknown")
NPM_VERSION := $(shell npm -v 2>/dev/null || echo "unknown")

# 默认目标
all: install lint test build

# 安装依赖
install:
	@echo "==> Installing dependencies..."
	npm ci

# 开发模式运行
dev:
	@echo "==> Starting development server..."
	npm run dev

# 生产构建
build:
	@echo "==> Building for production..."
	npm run build

# 运行测试
test:
	@echo "==> Running tests..."
	npm run test

# 运行测试并生成覆盖率报告
test-coverage:
	@echo "==> Running tests with coverage..."
	npm run test:coverage

# 运行 E2E 测试
test-e2e:
	@echo "==> Running E2E tests..."
	npm run test:e2e

# 代码检查
lint:
	@echo "==> Running linter..."
	npm run lint

# 自动修复代码格式
lint-fix:
	@echo "==> Fixing lint issues..."
	npm run lint:fix

# 类型检查
type-check:
	@echo "==> Running type check..."
	npm run type-check

# 格式化代码
fmt:
	@echo "==> Formatting code..."
	npm run format

# 预览生产构建
preview:
	@echo "==> Previewing production build..."
	npm run preview

# 清理
clean:
	@echo "==> Cleaning..."
	rm -rf dist/
	rm -rf node_modules/
	rm -f package-lock.json
	rm -rf coverage/

# 更新依赖
update-deps:
	@echo "==> Updating dependencies..."
	npm update
	npm audit fix

# 安全检查
security:
	@echo "==> Running security audit..."
	npm audit

# 构建 Docker 镜像
docker-build:
	@echo "==> Building Docker image..."
	docker build -t $(APP_NAME):latest .

# 分析构建包大小
analyze:
	@echo "==> Analyzing bundle size..."
	npm run analyze

# 生成组件
generate-component:
	@read -p "Enter component name: " name; \
	npm run generate:component $$name

# 生成页面
generate-page:
	@read -p "Enter page name: " name; \
	npm run generate:page $$name

# 部署到 Vercel (示例)
deploy-vercel:
	@echo "==> Deploying to Vercel..."
	vercel --prod

# 帮助
help:
	@echo "Available targets:"
	@echo "  make install       - Install dependencies"
	@echo "  make dev          - Start development server"
	@echo "  make build        - Build for production"
	@echo "  make test         - Run tests"
	@echo "  make test-coverage- Run tests with coverage"
	@echo "  make lint         - Run linter"
	@echo "  make lint-fix     - Fix lint issues"
	@echo "  make type-check   - Run type check"
	@echo "  make fmt          - Format code"
	@echo "  make preview      - Preview production build"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make security     - Run security audit"
	@echo "  make docker-build - Build Docker image"
	@echo "  make analyze      - Analyze bundle size"
	@echo "  make help         - Show this help"

# 版本信息
version:
	@echo "Node.js: $(NODE_VERSION)"
	@echo "npm: $(NPM_VERSION)"
