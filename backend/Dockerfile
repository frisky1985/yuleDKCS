# yuleDKCS Backend Dockerfile
# 多阶段构建优化构建速度和image大小

# ========== 构建阶段 ==========
FROM golang:1.22-alpine AS builder

# 安装构建依赖
RUN apk add --no-cache git ca-certificates tzdata

# 设置工作目录
WORKDIR /app

# 先复制go.mod和go.sum，利用Docker缓存
COPY go.mod go.sum ./
RUN go mod download

# 复制源代码
COPY . .

# 构建二进制
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build \
    -ldflags='-w -s -extldflags "-static"' \
    -a -installsuffix cgo \
    -o bin/api \
    main.go

# 运行测试
RUN go test -v ./...

# ========== 生产阶段 ==========
FROM scratch AS production

# 复制时区数据
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo

# 复制CA证书
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/

# 复制构建产物
COPY --from=builder /app/bin/api /api

# 设置时区
ENV TZ=Asia/Shanghai

# 暴露端口
EXPOSE 8080

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=40s --retries=3 \
    CMD ["/api", "health"]

# 运行
ENTRYPOINT ["/api"]

# ========== 开发阶段 ==========
FROM golang:1.22-alpine AS development

RUN apk add --no-cache git ca-certificates curl

WORKDIR /app

# 安装air热重载工具
RUN go install github.com/cosmtrek/air@latest

COPY go.mod go.sum ./
RUN go mod download

COPY . .

EXPOSE 8080

CMD ["air", "-c", ".air.toml"]
