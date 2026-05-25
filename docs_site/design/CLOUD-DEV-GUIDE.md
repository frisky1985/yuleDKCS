# 云端开发指南（服务平台）

> **版本**：v1.0  
> **日期**：2026-04-26  
> **作者**：code_designer  
> **适用范围**：云端团队  
> **技术栈**：Go / gRPC / TiDB / Redis / Kafka / Kubernetes

---

## 目录

1. [技术栈总览](#1-技术栈总览)
2. [开发环境搭建](#2-开发环境搭建)
3. [代码架构设计](#3-代码架构设计)
4. [核心模块开发指南](#4-核心模块开发指南)
5. [关键算法/流程实现](#5-关键算法流程实现)
6. [接口调用示例](#6-接口调用示例)
7. [测试要点](#7-测试要点)
8. [参考规范](#8-参考规范)

---

## 1. 技术栈总览

### 1.1 技术选型

| 层级 | 技术选型 | 说明 |
|------|---------|------|
| **主语言** | Go 1.21+ | 高并发、低延迟 |
| **微服务框架** | Kratos + gRPC | 契约优先、服务治理 |
| **API Gateway** | APISIX | 云原生、高性能 |
| **数据库** | TiDB 7.0 | 分布式 HTAP |
| **缓存** | Redis Cluster 7.0 | 会话、Token、热点数据 |
| **消息队列** | Apache Kafka 3.6 | 事件驱动、异步解耦 |
| **服务注册/发现** | Nacos 2.2 | 配置+注册一体化 |
| **容器编排** | Kubernetes 1.28 | 自动伸缩、滚动更新 |
| **可观测性** | Prometheus + Grafana + ELK | 指标+日志+链路 |
| **链路追踪** | OpenTelemetry + Jaeger | 分布式追踪 |
| **CI/CD** | GitLab CI + ArgoCD | GitOps |
| **密钥管理** | HashiCorp Vault + 云 HSM | 密钥生命周期 |
| **安全认证** | OAuth 2.0 / OIDC | 身份认证 |

### 1.2 服务划分

| 服务 | 职责 | 技术栈 | 副本数 |
|------|------|--------|--------|
| **API Gateway** | 统一入口、认证、限流 | APISIX + Go | 4 |
| **User Svc** | 用户注册登录 | Go + Kratos | 3 |
| **Key Svc** | 钥匙生命周期管理 | Go + Kratos | 4 |
| **Vehicle Svc** | 车辆管理 | Go + Kratos | 3 |
| **KMS Svc** | 密钥管理 | Go + Kratos + HSM | 3 |
| **Auth Svc** | 权限管理 | Go + Kratos | 3 |
| **OEM Adapter** | 车企对接 | Go + Kratos | 2 |
| **Event Svc** | 事件处理 | Go + Kratos | 3 |
| **Notification Svc** | 消息推送 | Go + Kratos | 2 |
| **Audit Svc** | 审计日志 | Go + Kratos | 2 |

### 1.3 项目结构

```
digital-key-cloud/
├── api/                         # API 定义（Protobuf）
│   ├── key/
│   │   └── v1/
│   │       ├── key_service.proto
│   │       └── key_types.proto
│   ├── vehicle/
│   │   └── v1/
│   ├── auth/
│   │   └── v1/
│   ├── kms/
│   │   └── v1/
│   └── common/
│       └── v1/
├── cmd/                         # 服务入口
│   ├── gateway/
│   ├── user-svc/
│   ├── key-svc/
│   ├── vehicle-svc/
│   ├── kms-svc/
│   └── ...
├── internal/                   # 内部代码
│   ├── config/                # 配置
│   ├── data/                  # 数据层
│   │   ├── mysql/            # TiDB
│   │   ├── redis/            # Redis
│   │   └── kafka/            # Kafka
│   ├── domain/               # 领域层
│   │   ├── key/              # 钥匙领域
│   │   ├── vehicle/          # 车辆领域
│   │   └── user/             # 用户领域
│   ├── service/              # 服务层
│   │   ├── key_svc/
│   │   └── vehicle_svc/
│   ├── repo/                 # 仓库层
│   └── biz/                  # 业务逻辑
├── pkg/                       # 公共包
│   ├── errors/              # 错误定义
│   ├── middleware/          # 中间件
│   ├── utils/               # 工具
│   └── crypto/              # 密码学工具
├── third_party/             # 第三方 proto
├── configs/                 # 配置文件
│   ├── gateway.yaml
│   ├── user-svc.yaml
│   └── key-svc.yaml
├── scripts/                  # 脚本
│   ├── proto/               # Proto 编译脚本
│   ├── db/                  # 数据库迁移
│   └── deploy/              # 部署脚本
├── test/                    # 测试代码
├── Makefile
└── go.mod
```

---

## 2. 开发环境搭建

### 2.1 环境要求

| 项目 | 版本 |
|------|------|
| 操作系统 | Ubuntu 22.04 LTS 或 macOS 13+ |
| Go | 1.21+ |
| Docker | 24.0+ |
| Docker Compose | 2.20+ |
| kubectl | 1.28+ |
| golangci-lint | 1.55+ |
| protobuf | 24.0+ (protoc) |
| protoc-gen-go | v1.32+ |
| protoc-gen-go-grpc | 1.3+ |

### 2.2 工具链安装

```bash
# 1. 安装 Go
wget https://go.dev/dl/go1.21.6.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.21.6.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
go version  # go version go1.21.6 linux/amd64

# 2. 安装 protobuf 编译器
sudo apt install -y protobuf-compiler
protoc --version  # libprotoc 3.21.x

# 3. 安装 Go 插件
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.32.0
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.3.0
go install github.com/go-kratos/kratos/cmd/kratos/v2@latest
go install github.com/go-kratos/better-sqlite3/cmd/protoc-gen-go-sql@latest

# 4. 安装 Docker & Docker Compose
sudo apt install -y docker.io docker-compose

# 5. 安装 kubectl
curl -LO "https://dl.k8s.io/release/v1.28.0/bin/linux/amd64/kubectl"
chmod +x kubectl && sudo mv kubectl /usr/local/bin/

# 6. 克隆代码
git clone git@github.com:digitalkey/cloud.git
cd cloud

# 7. 安装 Go 依赖
go mod download
```

### 2.3 本地开发环境（Docker Compose）

```yaml
# docker-compose.dev.yaml
version: '3.8'

services:
  # TiDB (MySQL 兼容)
  tidb:
    image: pingcap/tidb:v7.1.0
    ports:
      - "4000:4000"  # MySQL 协议
      - "10080:10080"  # Status 端口
    volumes:
      - ./data/tidb:/tidb
    command: ["--store=mocktikv", "--path=/tidb/tidb"]

  # Redis Cluster
  redis:
    image: redis:7.2-alpine
    ports:
      - "6379:6379"
    command: redis-server --appendonly yes
    volumes:
      - ./data/redis:/data

  # Kafka + Zookeeper
  kafka:
    image: confluentinc/cp-kafka:7.5.0
    depends_on:
      - zookeeper
    ports:
      - "9092:9092"
    environment:
      KAFKA_BROKER_ID: 1
      KAFKA_ZOOKEEPER_CONNECT: zookeeper:2181
      KAFKA_ADVERTISED_LISTENERS: PLAINTEXT://localhost:9092
      KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR: 1
    volumes:
      - ./data/kafka:/var/lib/kafka/data

  zookeeper:
    image: confluentinc/cp-zookeeper:7.5.0
    ports:
      - "2181:2181"
    environment:
      ZOOKEEPER_CLIENT_PORT: 2181

  # Nacos (服务注册 + 配置中心)
  nacos:
    image: nacos/nacos-server:v2.2.3
    ports:
      - "8848:8848"
      - "9848:9848"
    environment:
      MODE: standalone
      SPRING_DATASOURCE_PLATFORM: mysql
    volumes:
      - ./data/nacos:/home/nacos/data

  # Jaeger (链路追踪)
  jaeger:
    image: jaegertracing/all-in-one:1.50
    ports:
      - "16686:16686"  # UI
      - "4317:4317"    # OTLP gRPC
      - "4318:4318"    # OTLP HTTP
```

```bash
# 启动本地开发环境
docker-compose -f docker-compose.dev.yaml up -d

# 验证服务
curl http://localhost:8848/nacos/v1/cs/configs?dataId=app.config&group=DEFAULT_GROUP
```

### 2.4 编译 Proto 文件

```bash
# 编译所有 proto 文件
make proto

# 或手动编译
protoc --proto_path=api \
       --proto_path=third_party \
       --go_out=paths=source_relative:. \
       --go-grpc_out=paths=source_relative:. \
       api/key/v1/key_service.proto
```

### 2.5 运行服务

```bash
# 运行 Key Svc
cd cmd/key-svc
go run . -conf ../../configs/key-svc.yaml

# 或使用 Make
make run SERVICE=key-svc
```

---

## 3. 代码架构设计

### 3.1 微服务分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                      API Gateway (APISIX)                   │
│         认证 │ 限流 │ 协议转换 │ 请求路由                    │
└────────────────────────────┬────────────────────────────────┘
                             │ gRPC / HTTP
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│   User Svc    │   │   Key Svc     │   │  Vehicle Svc  │
│               │   │               │   │               │
│  · 注册登录    │   │  · 钥匙CRUD   │   │  · 车辆绑定    │
│  · Token      │   │  · 配对管理    │   │  · 状态同步    │
│  · OAuth      │   │  · 分享管理    │   │  · OTA管理    │
└───────┬───────┘   └───────┬───────┘   └───────┬───────┘
        │                   │                   │
        │          ┌────────┴────────┐         │
        │          │                 │         │
        ▼          ▼                 ▼         ▼
┌───────────────────────────────────────────────────────────┐
│                     Service Mesh (Istio/Envoy)            │
│              服务发现 │ 负载均衡 │ 熔断 │ 链路加密         │
└────────────────────────────┬──────────────────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│    TiDB       │   │    Redis      │   │    Kafka      │
│   (MySQL)     │   │   Cluster     │   │               │
│               │   │               │   │               │
│  · 用户数据   │   │  · 会话缓存    │   │  · 事件流     │
│  · 钥匙数据   │   │  · Token      │   │  · 异步解耦    │
│  · 车辆数据   │   │  · 热点数据    │   │               │
└───────────────┘   └───────────────┘   └───────────────┘
```

### 3.2 服务内部架构（Clean Architecture）

```
┌─────────────────────────────────────────────────────────────┐
│                    Handler (gRPC/HTTP)                      │
│         参数校验 │ 路由分发 │ 响应封装                        │
├─────────────────────────────────────────────────────────────┤
│                     Service (业务逻辑)                       │
│         事务编排 │ 领域逻辑 │ 策略引擎                        │
├─────────────────────────────────────────────────────────────┤
│                      Repository (数据访问)                   │
│         DAO │ 缓存 │ 事件发布                               │
├─────────────────────────────────────────────────────────────┤
│                        Data Source                          │
│         MySQL │ Redis │ Kafka │ HSM                          │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 核心代码结构

```go
// cmd/key-svc/main.go
package main

import (
    "flag"
    "github.com/digitalkey/key-svc/internal/conf"
    "github.com/go-kratos/kratos/v2"
    "github.com/go-kratos/kratos/v2/config"
    "github.com/go-kratos/kratos/v2/config/file"
    "github.com/go-kratos/kratos/v2/log"
)

func main() {
    flag.Parse()
    
    logger := log.NewStdLogger(os.Stdout)
    log := log.NewHelper(logger)
    
    c := config.New(
        config.WithSource(
            file.NewSource(flagConf),
        ),
        config.WithDecoder(func(kv *config.KeyValue, v map[string]interface{}) error {
            return yaml.Unmarshal(kv.Value, v)
        }),
    )
    defer c.Close()
    
    if err := c.Load(); err != nil {
        log.Error("config load failed: %v", err)
        return
    }
    
    var bc conf.Bootstrap
    if err := c.Scan(&bc); err != nil {
        log.Error("config scan failed: %v", err)
        return
    }
    
    app, cleanup, err := wireApp(&bc, logger)
    if err != nil {
        log.Error("wire app failed: %v", err)
        return
    }
    defer cleanup()
    
    if err := app.Run(); err != nil {
        log.Error("app run failed: %v", err)
    }
}

// wire.go - Google Wire 依赖注入
//go:build wireinject

package main

func wireApp(*conf.Bootstrap, log.Logger) (*kratos.App, func(), error) {
    panic(wire.Build(
        newApp,
        keySvc.Server,
        keySvc.Service,
        keySvc.Repo,
        data.NewMySQL,
        data.NewRedis,
        data.NewKafka,
        biz.NewKeyUsecase,
        service.NewKeyService,
        repo.NewKeyRepo,
    ))
}
```

### 3.4 核心领域模型

```go
// internal/domain/key/key.go

// 钥匙实体
type Key struct {
    ID           string
    UserID       string
    VehicleID    string
    KeyType      KeyType
    Status       KeyStatus
    Protocol     Protocol
    PublicKey    string
    CertSerial   string
    Permissions  []Permission
    ValidFrom    time.Time
    ValidUntil   *time.Time
    MaxUsageCount int
    UsageCount   int
    CreatedAt    time.Time
    UpdatedAt    time.Time
    LastUsedAt   *time.Time
}

// 钥匙类型
type KeyType int

const (
    KeyTypePrimary   KeyType = 1  // 车主钥匙
    KeyTypeSecondary KeyType = 2  // 副钥匙
    KeyTypeTemporary KeyType = 3  // 临时钥匙
    KeyTypeService   KeyType = 4  // 服务钥匙
)

// 钥匙状态
type KeyStatus int

const (
    KeyStatusCreated   KeyStatus = 1
    KeyStatusPaired   KeyStatus = 2
    KeyStatusActive   KeyStatus = 3
    KeyStatusSuspended KeyStatus = 4
    KeyStatusRevoked  KeyStatus = 5
    KeyStatusExpired  KeyStatus = 6
)

// 钥匙领域服务
type KeyDomainService struct {
    repo      KeyRepository
    kmsClient *kms.KMSClient
    eventPub  EventPublisher
}

func (s *KeyDomainService) CreateKey(ctx context.Context, cmd *CreateKeyCommand) (*Key, error) {
    // 1. 检查车辆是否存在
    vehicle, err := s.repo.GetVehicle(ctx, cmd.VehicleID)
    if err != nil {
        return nil, ErrVehicleNotFound
    }
    
    // 2. 检查钥匙数量限制（单车辆最多 10 把）
    count, err := s.repo.CountKeysByVehicle(ctx, cmd.VehicleID)
    if err != nil {
        return nil, err
    }
    if count >= MaxKeysPerVehicle {
        return nil, ErrKeyLimitExceeded
    }
    
    // 3. 调用 KMS 生成密钥对
    keyPair, err := s.kmsClient.GenerateKeyPair(ctx, &kms.GenKeyRequest{
        Algorithm: kms.Algorithm(cmd.Protocol),
    })
    if err != nil {
        return nil, ErrKeyGeneration
    }
    
    // 4. 创建钥匙实体
    key := &Key{
        ID:            generateID(),
        UserID:        cmd.UserID,
        VehicleID:     cmd.VehicleID,
        KeyType:       cmd.KeyType,
        Status:        KeyStatusCreated,
        Protocol:      cmd.Protocol,
        PublicKey:     keyPair.PublicKey,
        CertSerial:    keyPair.CertSerial,
        Permissions:   cmd.Permissions,
        ValidFrom:     time.Now(),
        ValidUntil:    cmd.ValidUntil,
        MaxUsageCount: cmd.MaxUsageCount,
        CreatedAt:    time.Now(),
    }
    
    // 5. 保存到数据库
    if err := s.repo.CreateKey(ctx, key); err != nil {
        return nil, err
    }
    
    // 6. 发布领域事件
    s.eventPub.Publish(ctx, &KeyCreatedEvent{Key: key})
    
    return key, nil
}

func (s *KeyDomainService) ActivateKey(ctx context.Context, keyID, sessionKey string) error {
    key, err := s.repo.GetKey(ctx, keyID)
    if err != nil {
        return ErrKeyNotFound
    }
    
    // 验证会话密钥
    if !s.kmsClient.VerifySessionKey(ctx, sessionKey, key.PublicKey) {
        return ErrInvalidSessionKey
    }
    
    // 更新状态
    key.Status = KeyStatusActive
    key.UpdatedAt = time.Now()
    
    if err := s.repo.UpdateKey(ctx, key); err != nil {
        return err
    }
    
    // 发布事件
    s.eventPub.Publish(ctx, &KeyActivatedEvent{KeyID: keyID})
    
    return nil
}

func (s *KeyDomainService) RevokeKey(ctx context.Context, keyID, reason string) error {
    key, err := s.repo.GetKey(ctx, keyID)
    if err != nil {
        return ErrKeyNotFound
    }
    
    if key.Status == KeyStatusRevoked {
        return ErrAlreadyRevoked
    }
    
    // 更新状态
    key.Status = KeyStatusRevoked
    key.UpdatedAt = time.Now()
    
    if err := s.repo.UpdateKey(ctx, key); err != nil {
        return err
    }
    
    // 通知车端（通过 Kafka）
    s.eventPub.Publish(ctx, &KeyRevokedEvent{
        KeyID:   keyID,
        VehicleID: key.VehicleID,
        Reason:  reason,
    })
    
    return nil
}
```

---

## 4. 核心模块开发指南

### 4.1 用户认证服务（IAM）

```go
// internal/service/auth_svc.go

type AuthService struct {
    authRepo UserAuthRepository
    kmsClient *kms.KMSClient
    tokenCache *redis.Cache
}

func (s *AuthService) Login(ctx context.Context, req *LoginRequest) (*LoginResponse, error) {
    // 1. 验证验证码
    if err := s.verifyCode(ctx, req.Phone, req.Code); err != nil {
        return nil, err
    }
    
    // 2. 获取或创建用户
    user, err := s.authRepo.FindOrCreateUser(ctx, req.Phone)
    if err != nil {
        return nil, ErrInternal
    }
    
    // 3. 签发 JWT Token
    accessToken, err := s.issueAccessToken(user)
    if err != nil {
        return nil, ErrTokenIssue
    }
    
    refreshToken, err := s.issueRefreshToken(user)
    if err != nil {
        return nil, ErrTokenIssue
    }
    
    // 4. 缓存 Token
    s.tokenCache.Set(ctx, user.ID, accessToken, time.Hour)
    
    // 5. 记录登录日志
    s.publishLoginEvent(ctx, user, req.DeviceInfo)
    
    return &LoginResponse{
        UserId:         user.ID,
        AccessToken:    accessToken,
        RefreshToken:   refreshToken,
        ExpiresIn:     3600,
    }, nil
}

func (s *AuthService) RefreshToken(ctx context.Context, refreshToken string) (*TokenResponse, error) {
    // 1. 验证 Refresh Token
    claims, err := s.validateRefreshToken(refreshToken)
    if err != nil {
        return nil, ErrInvalidToken
    }
    
    // 2. 获取用户
    user, err := s.authRepo.GetUser(ctx, claims.UserID)
    if err != nil {
        return nil, ErrUserNotFound
    }
    
    // 3. 签发新 Token
    accessToken, err := s.issueAccessToken(user)
    if err != nil {
        return nil, ErrTokenIssue
    }
    
    newRefreshToken, err := s.issueRefreshToken(user)
    if err != nil {
        return nil, ErrTokenIssue
    }
    
    // 4. 作废旧 Refresh Token
    s.tokenCache.Delete(ctx, "refresh:"+refreshToken)
    
    return &TokenResponse{
        AccessToken:  accessToken,
        RefreshToken: newRefreshToken,
        ExpiresIn:    3600,
    }, nil
}

func (s *AuthService) issueAccessToken(user *User) (string, error) {
    now := time.Now()
    claims := &TokenClaims{
        UserID:    user.ID,
        Phone:     user.Phone,
        TokenType: TokenTypeAccess,
        StandardClaims: jwt.StandardClaims{
            Issuer:    "digitalkey",
            Subject:   user.ID,
            ExpiresAt: now.Add(time.Hour).Unix(),
            IssuedAt:  now.Unix(),
            NotBefore: now.Unix(),
        },
    }
    
    token := jwt.NewWithClaims(jwt.SigningMethodRS256, claims)
    return token.SignedString(s.privateKey)
}
```

### 4.2 钥匙管理服务（DKMS）

```go
// internal/service/key_svc.go

type KeyService struct {
    keyRepo     KeyRepository
    vehicleRepo VehicleRepository
    kmsClient   *kms.KMSClient
    eventPub    EventPublisher
}

func (s *KeyService) CreateKey(ctx context.Context, req *CreateKeyRequest) (*CreateKeyResponse, error) {
    // 1. 验证请求
    if err := s.validateCreateRequest(req); err != nil {
        return nil, err
    }
    
    // 2. 检查车辆绑定关系
    if err := s.verifyVehicleBinding(ctx, req.UserID, req.VehicleID); err != nil {
        return nil, err
    }
    
    // 3. 调用 KMS 生成密钥对
    keyGenResp, err := s.kmsClient.GenerateKeyPair(ctx, &kms.GenKeyRequest{
        UserID:   req.UserID,
        VehicleID: req.VehicleID,
        Algorithm: string(req.Protocol),
    })
    if err != nil {
        return nil, ErrKeyGeneration
    }
    
    // 4. 生成配对 Token
    pairingToken := generatePairingToken()
    
    // 5. 创建钥匙记录
    key := &Key{
        ID:            generateID(),
        UserID:        req.UserID,
        VehicleID:     req.VehicleID,
        KeyType:       req.KeyType,
        Status:        KeyStatusCreated,
        Protocol:      req.Protocol,
        PublicKey:     keyGenResp.PublicKey,
        CertSerial:    keyGenResp.CertSerial,
        PairingToken:  pairingToken,
        PairingTokenExpiredAt: time.Now().Add(30 * time.Minute),
        Permissions:   req.Permissions,
        ValidFrom:     time.Now(),
        ValidUntil:    req.ValidUntil,
        MaxUsageCount: req.MaxUsageCount,
    }
    
    if err := s.keyRepo.Create(ctx, key); err != nil {
        return nil, ErrKeyCreation
    }
    
    // 6. 发布领域事件
    s.eventPub.Publish(ctx, domain.NewKeyCreatedEvent(key))
    
    return &CreateKeyResponse{
        KeyId:        key.ID,
        PublicKey:    keyGenResp.PublicKey,
        PairingToken: pairingToken,
        ExpiresAt:   key.PairingTokenExpiredAt.Unix(),
    }, nil
}

func (s *KeyService) InitiatePairing(ctx context.Context, req *InitiatePairingRequest) (*InitiatePairingResponse, error) {
    // 1. 验证配对 Token
    key, err := s.keyRepo.GetByPairingToken(ctx, req.PairingToken)
    if err != nil {
        return nil, ErrInvalidPairingToken
    }
    
    if time.Now().After(key.PairingTokenExpiredAt) {
        return nil, ErrPairingTokenExpired
    }
    
    // 2. 生成配对会话
    sessionID := generateSessionID()
    serverNonce := generateNonce(32)
    
    // 3. 调用 KMS 生成服务器公钥
    serverKey, err := s.kmsClient.GenerateServerPublicKey(ctx, &kms.GenServerKeyRequest{
        SessionID: sessionID,
        Algorithm: string(key.Protocol),
    })
    if err != nil {
        return nil, ErrKeyGeneration
    }
    
    // 4. 保存配对会话
    session := &PairingSession{
        SessionID:     sessionID,
        KeyID:         key.ID,
        VehicleID:     key.VehicleID,
        ServerNonce:   serverNonce,
        ServerPubKey:  serverKey.PublicKey,
        ExpiresAt:     time.Now().Add(5 * time.Minute),
    }
    s.keyRepo.SavePairingSession(ctx, session)
    
    return &InitiatePairingResponse{
        SessionID:     sessionID,
        ServerPubKey:  serverKey.PublicKey,
        Nonce:         serverNonce,
        ExpiresAt:    session.ExpiresAt.Unix(),
    }, nil
}

func (s *KeyService) ConfirmPairing(ctx context.Context, req *ConfirmPairingRequest) (*ConfirmPairingResponse, error) {
    // 1. 获取配对会话
    session, err := s.keyRepo.GetPairingSession(ctx, req.SessionID)
    if err != nil {
        return nil, ErrInvalidSession
    }
    
    if time.Now().After(session.ExpiresAt) {
        return nil, ErrSessionExpired
    }
    
    // 2. 获取钥匙
    key, err := s.keyRepo.GetKey(ctx, session.KeyID)
    if err != nil {
        return nil, ErrKeyNotFound
    }
    
    // 3. 验证手机签名
    verified, err := s.kmsClient.VerifySignature(ctx, &kms.VerifyRequest{
        Algorithm:   string(key.Protocol),
        PublicKey:   key.PublicKey,
        Data:        session.ServerNonce,
        Signature:   req.Signature,
    })
    if err != nil || !verified {
        return nil, ErrSignatureVerification
    }
    
    // 4. 激活钥匙
    key.Status = KeyStatusActive
    key.UpdatedAt = time.Now()
    
    if err := s.keyRepo.UpdateKey(ctx, key); err != nil {
        return nil, ErrKeyUpdate
    }
    
    // 5. 删除配对会话
    s.keyRepo.DeletePairingSession(ctx, req.SessionID)
    
    // 6. 通知车端（通过 Kafka）
    s.eventPub.Publish(ctx, domain.NewKeyActivatedEvent(key))
    
    return &ConfirmPairingResponse{
        KeyId: key.ID,
        Status: "ACTIVE",
    }, nil
}

func (s *KeyService) RevokeKey(ctx context.Context, req *RevokeKeyRequest) (*RevokeKeyResponse, error) {
    key, err := s.keyRepo.GetKey(ctx, req.KeyId)
    if err != nil {
        return nil, ErrKeyNotFound
    }
    
    // 验证权限
    if key.UserID != req.RequesterID {
        return nil, ErrPermissionDenied
    }
    
    key.Status = KeyStatusRevoked
    key.RevokedAt = time.Now()
    key.RevokeReason = req.Reason
    
    if err := s.keyRepo.UpdateKey(ctx, key); err != nil {
        return nil, ErrKeyUpdate
    }
    
    // 通知车端
    s.eventPub.Publish(ctx, domain.NewKeyRevokedEvent(key, req.Reason))
    
    // 通知被分享者
    if key.ShareID != "" {
        s.notifyShareRecipient(ctx, key.ShareID)
    }
    
    return &RevokeKeyResponse{
        KeyId:     key.ID,
        Status:    "REVOKED",
        RevokedAt: key.RevokedAt.Unix(),
    }, nil
}
```

### 4.3 密钥管理服务（KMS）

```go
// internal/service/kms_svc.go

type KMSService struct {
    hsmClient *hsm.Client
    vault     *vault.Client
    caService *CAService
}

func (s *KMSService) GenerateKeyPair(ctx context.Context, req *GenKeyRequest) (*GenKeyResponse, error) {
    var (
        publicKey string
        certSerial string
        err error
    )
    
    switch req.Algorithm {
    case kms.AlgorithmICCE:
        // 国密 SM2 密钥对生成
        sm2Key, err := sm2.GenerateKey(rand.Reader)
        if err != nil {
            return nil, ErrKeyGeneration
        }
        
        // 使用 HSM 存储私钥
        keyID, err := s.hsmClient.ImportKey(ctx, &hsm.ImportKeyRequest{
            Algorithm: hsm.AlgorithmSM2,
            PrivateKey: sm2Key.D.Bytes(),
            Label:      "key_" + req.UserID,
        })
        if err != nil {
            return nil, ErrKeyStorage
        }
        
        publicKey = hex.EncodeToString(sm2Key.PublicKey.X.Bytes())
        publicKey += hex.EncodeToString(sm2Key.PublicKey.Y.Bytes())
        
        // 生成国密证书
        cert, err := s.caService.IssueCertificate(ctx, &IssueCertRequest{
            Subject:   req.UserID,
            PublicKey: publicKey,
            Algorithm: "SM2",
        })
        certSerial = cert.Serial
        
    case kms.AlgorithmCCC:
        // ECDSA P-256 密钥对生成
        p256Key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
        if err != nil {
            return nil, ErrKeyGeneration
        }
        
        // 存储到 Vault
        keyID, err := s.vault.ImportKey(ctx, &vault.ImportKeyRequest{
            Algorithm: "ecdsa-p256",
            PrivateKey: p256Key.D.Bytes(),
            Label:      "key_" + req.UserID,
        })
        if err != nil {
            return nil, ErrKeyStorage
        }
        
        // 生成 X.509 证书
        cert, err := s.caService.IssueCertificate(ctx, &IssueCertRequest{
            Subject:   req.UserID,
            PublicKey: hex.EncodeToString(
                elliptic.Marshal(elliptic.P256(), 
                    p256Key.PublicKey.X, p256Key.PublicKey.Y),
            ),
            Algorithm: "ECDSA-P256",
        })
        certSerial = cert.Serial
    }
    
    return &GenKeyResponse{
        KeyID:       keyID,
        PublicKey:   publicKey,
        CertSerial:  certSerial,
        CreatedAt:   time.Now().Unix(),
    }, nil
}

func (s *KMSService) Sign(ctx context.Context, req *SignRequest) (*SignResponse, error) {
    switch req.Algorithm {
    case "SM2":
        sig, err := s.hsmClient.Sign(ctx, &hsm.SignRequest{
            KeyID:     req.KeyID,
            Algorithm: hsm.AlgorithmSM2,
            Data:      req.Data,
            HashID:    sm3.ID,
        })
        if err != nil {
            return nil, ErrSign
        }
        return &SignResponse{Signature: sig}, nil
        
    case "ECDSA-P256":
        sig, err := s.vault.Sign(ctx, &vault.SignRequest{
            KeyID:     req.KeyID,
            Algorithm: "sha256",
            Data:      req.Data,
        })
        if err != nil {
            return nil, ErrSign
        }
        return &SignResponse{Signature: sig}, nil
    }
    
    return nil, ErrUnsupportedAlgorithm
}

func (s *KMSService) VerifySignature(ctx context.Context, req *VerifyRequest) (*VerifyResponse, error) {
    var verified bool
    
    switch req.Algorithm {
    case "SM2":
        verified = sm2.Verify(
            req.PublicKey,
            req.Data,
            req.Signature,
        )
    case "ECDSA-P256":
        key := new(ecdsa.PublicKey)
        key.Curve = elliptic.P256()
        key.X, key.Y = elliptic.Unmarshal(elliptic.P256(), req.PublicKey)
        
        verified = ecdsa.Verify(
            key,
            req.Data,
            req.Signature,
        )
    }
    
    return &VerifyResponse{Valid: verified}, nil
}
```

### 4.4 消息队列处理

```go
// internal/data/kafka/consumer.go

type KeyEventConsumer struct {
    consumerGroup *kafka.ConsumerGroup
    keySvc        *service.KeyService
    vehicleSvc    *service.VehicleService
    notifSvc      *service.NotificationService
}

func (c *KeyEventConsumer) Start(ctx context.Context) error {
    handler := &keyEventHandler{
        keySvc:     c.keySvc,
        vehicleSvc: c.vehicleSvc,
        notifSvc:   c.notifSvc,
    }
    
    go func() {
        for {
            select {
            case <-ctx.Done():
                return
            default:
                msg, err := c.consumerGroup.Consume(ctx, "digitalkey.key.lifecycle", handler)
                if err != nil {
                    log.Errorf("kafka consume error: %v", err)
                }
                _ = msg
            }
        }
    }()
    
    return nil
}

type keyEventHandler struct {
    keySvc     *service.KeyService
    vehicleSvc *service.VehicleService
    notifSvc   *service.NotificationService
}

func (h *keyEventHandler) Handle(ctx context.Context, msg *kafka.Message) error {
    var event domain.KeyEvent
    if err := json.Unmarshal(msg.Value, &event); err != nil {
        return err
    }
    
    switch event.Type {
    case domain.KeyEventCreated:
        return h.handleKeyCreated(ctx, &event)
    case domain.KeyEventActivated:
        return h.handleKeyActivated(ctx, &event)
    case domain.KeyEventRevoked:
        return h.handleKeyRevoked(ctx, &event)
    case domain.KeyEventSuspended:
        return h.handleKeySuspended(ctx, &event)
    }
    
    return nil
}

func (h *keyEventHandler) handleKeyActivated(ctx context.Context, event *domain.KeyEvent) error {
    // 1. 同步钥匙到车端
    _, err := h.vehicleSvc.SyncKeysToVehicle(ctx, &vehicle.SyncKeysRequest{
        VehicleID: event.VehicleID,
        Keys: []*vehicle.VehicleKey{
            {
                KeyID:      event.KeyID,
                PublicKey:  event.PublicKey,
                KeyType:    vehicle.KeyType(event.KeyType),
                Status:     vehicle.KeyStatus_ACTIVE,
            },
        },
    })
    if err != nil {
        log.Errorf("sync key to vehicle failed: %v", err)
        // 不重试，车端下次上线会重新同步
    }
    
    // 2. 发送通知
    h.notifSvc.Send(ctx, &notification.SendRequest{
        UserID:   event.UserID,
        Template: "key_activated",
        Data: map[string]string{
            "keyId":     event.KeyID,
            "vehicleId": event.VehicleID,
        },
    })
    
    return nil
}

func (h *keyEventHandler) handleKeyRevoked(ctx context.Context, event *domain.KeyEvent) error {
    // 1. 通知车端立即吊销
    _, err := h.vehicleSvc.RevokeKey(ctx, &vehicle.RevokeKeyRequest{
        VehicleID: event.VehicleID,
        KeyID:     event.KeyID,
        Reason:    event.Reason,
    })
    if err != nil {
        log.Errorf("notify vehicle key revoked failed: %v", err)
    }
    
    // 2. 发送通知给车主
    h.notifSvc.Send(ctx, &notification.SendRequest{
        UserID:   event.UserID,
        Template: "key_revoked",
        Data: map[string]string{
            "keyId": event.KeyID,
            "reason": event.Reason,
        },
        Push: &notification.PushConfig{
            Title:   "钥匙已吊销",
            Content: "您的数字钥匙已被吊销",
            Badge:   1,
        },
    })
    
    return nil
}
```

---

## 5. 关键算法/流程实现

### 5.1 JWT Token 验证中间件

```go
// pkg/middleware/auth.go

func AuthUnaryInterceptor() grpc.UnaryServerInterceptor {
    return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
        // 1. 从 metadata 获取 Token
        md, ok := metadata.FromIncomingContext(ctx)
        if !ok {
            return nil, status.Errorf(codes.Unauthenticated, "missing metadata")
        }
        
        authHeader := md.Get("authorization")
        if len(authHeader) == 0 {
            return nil, status.Errorf(codes.Unauthenticated, "missing token")
        }
        
        // 2. 解析 Bearer Token
        tokenString := strings.TrimPrefix(authHeader[0], "Bearer ")
        if tokenString == authHeader[0] {
            return nil, status.Errorf(codes.Unauthenticated, "invalid token format")
        }
        
        // 3. 验证 JWT
        claims, err := validateToken(tokenString)
        if err != nil {
            return nil, status.Errorf(codes.Unauthenticated, "invalid token: %v", err)
        }
        
        // 4. 检查 Token 类型
        if claims.TokenType != "access" {
            return nil, status.Errorf(codes.Unauthenticated, "invalid token type")
        }
        
        // 5. 检查是否在黑名单
        if isTokenBlacklisted(ctx, tokenString) {
            return nil, status.Errorf(codes.Unauthenticated, "token revoked")
        }
        
        // 6. 注入用户信息到 Context
        ctx = context.WithValue(ctx, UserIDKey, claims.UserID)
        ctx = context.WithValue(ctx, TokenClaimsKey, claims)
        
        return handler(ctx, req)
    }
}

func validateToken(tokenString string) (*TokenClaims, error) {
    token, err := jwt.ParseWithClaims(tokenString, &TokenClaims{}, func(token *jwt.Token) (interface{}, error) {
        // 验证签名算法
        if _, ok := token.Method.(*jwt.SigningMethodRSA); !ok {
            return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
        }
        return publicKey, nil
    })
    
    if err != nil {
        return nil, err
    }
    
    claims, ok := token.Claims.(*TokenClaims)
    if !ok || !token.Valid {
        return nil, ErrInvalidToken
    }
    
    return claims, nil
}
```

### 5.2 分布式锁（Redis）

```go
// pkg/utils/distributed_lock.go

type DistributedLock struct {
    redis  *redis.Client
    key    string
    value  string
    expiry time.Duration
}

func NewLock(redis *redis.Client, key string, expiry time.Duration) *DistributedLock {
    return &DistributedLock{
        redis:  redis,
        key:    "lock:" + key,
        value:  generateUUID(),
        expiry: expiry,
    }
}

func (l *DistributedLock) Acquire(ctx context.Context) (bool, error) {
    // SET key value NX PX milliseconds
    result, err := l.redis.SetNX(ctx, l.key, l.value, l.expiry).Result()
    if err != nil {
        return false, err
    }
    return result, nil
}

func (l *DistributedLock) Release(ctx context.Context) error {
    // Lua 脚本保证原子性：只删除自己持有的锁
    script := redis.NewScript(`
        if redis.call("get", KEYS[1]) == ARGV[1] then
            return redis.call("del", KEYS[1])
        else
            return 0
        end
    `)
    
    _, err := script.Run(ctx, l.redis, []string{l.key}, l.value).Result()
    return err
}

func (l *DistributedLock) Extend(ctx context.Context, expiry time.Duration) error {
    // 延长锁的持有时间
    script := redis.NewScript(`
        if redis.call("get", KEYS[1]) == ARGV[1] then
            return redis.call("pexpire", KEYS[1], ARGV[2])
        else
            return 0
        end
    `)
    
    result, err := script.Run(ctx, l.redis, []string{l.key}, l.value, expiry.Milliseconds()).Int()
    if err != nil {
        return err
    }
    
    if result == 0 {
        return ErrLockNotHeld
    }
    
    l.expiry = expiry
    return nil
}

// 使用示例：钥匙激活时防止并发
func (s *KeyService) ActivateKey(ctx context.Context, keyID string) error {
    lock := utils.NewLock(s.redis, "key:activate:"+keyID, 30*time.Second)
    
    acquired, err := lock.Acquire(ctx)
    if err != nil {
        return err
    }
    if !acquired {
        return ErrConcurrentOperation
    }
    defer lock.Release(ctx)
    
    // 执行激活逻辑
    // ...
}
```

### 5.3 限流器（令牌桶）

```go
// pkg/middleware/ratelimit.go

type TokenBucketLimiter struct {
    rate     float64        // 每秒补充的令牌数
    capacity int64          // 桶容量
    tokens   int64          // 当前令牌数
    lastTime time.Time      // 上次更新时间
    mu       sync.Mutex
}

func NewTokenBucketLimiter(rate float64, capacity int64) *TokenBucketLimiter {
    return &TokenBucketLimiter{
        rate:     rate,
        capacity: capacity,
        tokens:   capacity,
        lastTime: time.Now(),
    }
}

func (l *TokenBucketLimiter) Allow() bool {
    return l.AllowN(1)
}

func (l *TokenBucketLimiter) AllowN(n int64) bool {
    l.mu.Lock()
    defer l.mu.Unlock()
    
    now := time.Now()
    elapsed := now.Sub(l.lastTime).Seconds()
    
    // 补充令牌
    l.tokens += int64(elapsed * l.rate)
    if l.tokens > l.capacity {
        l.tokens = l.capacity
    }
    l.lastTime = now
    
    if l.tokens >= n {
        l.tokens -= n
        return true
    }
    
    return false
}

// gRPC 限流中间件
func RateLimitInterceptor(limiter *TokenBucketLimiter) grpc.UnaryServerInterceptor {
    return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
        if !limiter.Allow() {
            return nil, status.Errorf(codes.ResourceExhausted, "rate limit exceeded")
        }
        return handler(ctx, req)
    }
}
```

---

## 6. 接口调用示例

### 6.1 gRPC 客户端调用

```go
// examples/grpc_client/main.go

package main

import (
    "context"
    "log"
    
    "google.golang.org/grpc"
    "google.golang.org/grpc/credentials/insecure"
    
    keypb "github.com/digitalkey/cloud/api/key/v1"
)

func main() {
    // 1. 建立连接
    conn, err := grpc.Dial(
        "key-svc:9000",
        grpc.WithTransportCredentials(insecure.NewCredentials()),
        grpc.WithUnaryInterceptor(
            grpc脂胺.ChainUnaryClient(
                clientInterceptor,
                grpc_retry.UnaryClientInterceptor(
                    grpc_retry.WithMax(3),
                    grpc_retry.WithBackoff(grpc_retry.BackoffExponential(100 * time.Millisecond)),
                ),
            ),
        ),
    )
    if err != nil {
        log.Fatalf("dial failed: %v", err)
    }
    defer conn.Close()
    
    // 2. 创建客户端
    client := keypb.NewKeyServiceClient(conn)
    
    // 3. 调用接口
    ctx := metadata.AppendToOutgoingContext(context.Background(),
        "authorization", "Bearer "+token,
    )
    
    resp, err := client.CreateKey(ctx, &keypb.CreateKeyRequest{
        UserId:    "usr_123",
        VehicleId: "veh_456",
        KeyType:   keypb.KeyType_PRIMARY,
        Protocol:  keypb.Protocol_ICCE,
    })
    if err != nil {
        log.Fatalf("create key failed: %v", err)
    }
    
    log.Printf("Key created: %s, PairingToken: %s", resp.KeyId, resp.PairingToken)
}

func clientInterceptor(ctx context.Context, method string, req, reply interface{}, cc *grpc.ClientConn, invoker grpc.UnaryInvoker) error {
    start := time.Now()
    err := invoker(ctx, method, req, reply, cc)
    log.Printf("gRPC call %s took %v, error: %v", method, time.Since(start), err)
    return err
}
```

### 6.2 Kafka 生产者

```go
// internal/data/kafka/producer.go

type EventProducer struct {
    producer *kafka.Producer
}

func NewEventProducer(brokers []string) (*EventProducer, error) {
    config := &kafka.ConfigMap{
        "bootstrap.servers":  strings.Join(brokers, ","),
        "acks":               "all",
        "retries":            3,
        "retry.backoff.ms":   100,
        "enable.idempotence": true,  // 幂等 producer
    }
    
    producer, err := kafka.NewProducer(config)
    if err != nil {
        return nil, err
    }
    
    return &EventProducer{producer: producer}, nil
}

func (p *EventProducer) PublishKeyEvent(ctx context.Context, topic string, event *domain.KeyEvent) error {
    data, err := json.Marshal(event)
    if err != nil {
        return err
    }
    
    msg := &kafka.Message{
        Topic: topic,
        Key:   []byte(event.KeyID),  // 按 keyId 分区，保证同一钥匙事件有序
        Value: data,
        Headers: []kafka.Header{
            {Key: "event_type", Value: []byte(event.Type)},
            {Key: "timestamp", Value: []byte(strconv.FormatInt(event.Timestamp, 10))},
        },
    }
    
    // 异步发送
    go func() {
        err := p.producer.Produce(msg, nil)
        if err != nil {
            log.Errorf("kafka produce error: %v", err)
        }
    }()
    
    return nil
}

// 优雅关闭
func (p *EventProducer) Close() {
    p.producer.Flush(5 * time.Second)
    p.producer.Close()
}
```

### 6.3 REST API 中间层

```go
// cmd/gateway/main.go

// 使用 APISIX 配置，或 Go HTTP Server

func main() {
    router := gin.New()
    router.Use(gin.Recovery())
    router.Use(middleware.Logger())
    router.Use(middleware.CORS())
    
    // 认证中间件
    auth := middleware.NewAuthMiddleware(authSvc)
    
    // 限流中间件
    limiter := middleware.NewRateLimitMiddleware(redis, 100, 200) // 100 QPS，burst 200
    
    // 路由
    v1 := router.Group("/api/v1")
    v1.Use(auth.Handler())
    v1.Use(limiter.Handler())
    {
        // 钥匙管理
        v1.POST("/keys", keyHandler.CreateKey)
        v1.GET("/keys", keyHandler.ListKeys)
        v1.GET("/keys/:id", keyHandler.GetKey)
        v1.POST("/keys/:id/suspend", keyHandler.SuspendKey)
        v1.POST("/keys/:id/resume", keyHandler.ResumeKey)
        v1.DELETE("/keys/:id", keyHandler.RevokeKey)
        
        // 钥匙分享
        v1.POST("/keys/:id/shares", shareHandler.CreateShare)
        v1.GET("/keys/:id/shares", shareHandler.ListShares)
        v1.DELETE("/keys/:id/shares/:shareId", shareHandler.RevokeShare)
        
        // 车辆控制
        v1.POST("/vehicles/:id/controls", controlHandler.SendControl)
        v1.GET("/vehicles/:id/controls/:requestId", controlHandler.GetControlStatus)
    }
    
    router.Run(":8080")
}

// HTTP Handler 示例
type KeyHandler struct {
    keySvc *service.KeyService
}

func (h *KeyHandler) CreateKey(c *gin.Context) {
    var req CreateKeyRequest
    if err := c.ShouldBindJSON(&req); err != nil {
        c.JSON(400, gin.H{"code": 10001, "message": err.Error()})
        return
    }
    
    userID := c.GetString("userId")
    
    resp, err := h.keySvc.CreateKey(c.Request.Context(), &service.CreateKeyRequest{
        UserID:    userID,
        VehicleID: req.VehicleID,
        KeyType:   req.KeyType,
        Protocol:  req.Protocol,
    })
    if err != nil {
        c.JSON(500, gin.H{"code": 50001, "message": err.Error()})
        return
    }
    
    c.JSON(200, gin.H{
        "code": 0,
        "data": gin.H{
            "keyId":        resp.KeyID,
            "publicKey":    resp.PublicKey,
            "pairingToken": resp.PairingToken,
        },
    })
}
```

---

## 7. 测试要点

### 7.1 单元测试

```go
// internal/service/key_svc_test.go

func TestKeyService_CreateKey(t *testing.T) {
    // 准备 Mock
    mockRepo := &MockKeyRepository{}
    mockKMS := &MockKMSClient{}
    mockEvent := &MockEventPublisher{}
    
    svc := NewKeyService(mockRepo, mockKMS, mockEvent)
    
    ctx := context.Background()
    
    // Mock 期望
    mockRepo.On("GetVehicle", ctx, "veh_456").Return(&Vehicle{ID: "veh_456"}, nil)
    mockRepo.On("CountKeysByVehicle", ctx, "veh_456").Return(3, nil)
    mockKMS.On("GenerateKeyPair", ctx, mock.Anything).Return(&kms.GenKeyResponse{
        PublicKey:  "pub_key_123",
        CertSerial: "cert_123",
    }, nil)
    mockRepo.On("Create", ctx, mock.Anything).Return(nil)
    mockEvent.On("Publish", ctx, mock.Anything).Return(nil)
    
    // 执行
    resp, err := svc.CreateKey(ctx, &CreateKeyRequest{
        UserID:    "usr_123",
        VehicleID: "veh_456",
        KeyType:   KeyTypePrimary,
        Protocol:  ProtocolICCE,
    })
    
    // 断言
    assert.NoError(t, err)
    assert.NotEmpty(t, resp.KeyID)
    assert.NotEmpty(t, resp.PairingToken)
    
    mockRepo.AssertExpectations(t)
    mockKMS.AssertExpectations(t)
    mockEvent.AssertExpectations(t)
}

func TestKeyService_CreateKey_KeyLimitExceeded(t *testing.T) {
    mockRepo := &MockKeyRepository{}
    mockKMS := &MockKMSClient{}
    mockEvent := &MockEventPublisher{}
    
    svc := NewKeyService(mockRepo, mockKMS, mockEvent)
    ctx := context.Background()
    
    // Mock 达到上限
    mockRepo.On("GetVehicle", ctx, "veh_456").Return(&Vehicle{ID: "veh_456"}, nil)
    mockRepo.On("CountKeysByVehicle", ctx, "veh_456").Return(10, nil)  // 达到上限
    
    _, err := svc.CreateKey(ctx, &CreateKeyRequest{
        UserID:    "usr_123",
        VehicleID: "veh_456",
        KeyType:   KeyTypePrimary,
    })
    
    assert.Error(t, err)
    assert.Equal(t, ErrKeyLimitExceeded, err)
}
```

### 7.2 集成测试

```go
// test/integration/key_svc_test.go

func TestKeyServiceE2E(t *testing.T) {
    if testing.Short() {
        t.Skip("skip e2e test in short mode")
    }
    
    // 1. 启动测试容器
    compose := testcontainers.ComposeUp(t, "docker-compose.test.yaml")
    defer compose.Down()
    
    // 2. 等待服务就绪
    waitForService(t, "localhost:4000", 30*time.Second)
    waitForService(t, "localhost:6379", 30*time.Second)
    waitForService(t, "localhost:9092", 30*time.Second)
    
    // 3. 创建测试客户端
    db, _ := sql.Open("mysql", "root:@tcp(localhost:4000)/digitalkey_test")
    defer db.Close()
    
    redisClient := redis.NewClient(&redis.Options{
        Addr: "localhost:6379",
    })
    defer redisClient.Close()
    
    // 4. 执行测试
    t.Run("CreateAndActivateKey", func(t *testing.T) {
        // 创建钥匙
        resp, err := keyClient.CreateKey(context.Background(), &keypb.CreateKeyRequest{
            UserId:    "usr_test",
            VehicleId: "veh_test",
            KeyType:   keypb.KeyType_PRIMARY,
            Protocol:  keypb.Protocol_ICCE,
        })
        assert.NoError(t, err)
        
        // 激活钥匙
        activateResp, err := keyClient.ConfirmPairing(context.Background(), &keypb.ConfirmPairingRequest{
            SessionId: "session_test",
            Signature: []byte("test_signature"),
        })
        assert.NoError(t, err)
        assert.Equal(t, "ACTIVE", activateResp.Status)
    })
    
    // 5. 清理测试数据
    db.Exec("DELETE FROM keys WHERE user_id = 'usr_test'")
}
```

### 7.3 性能测试

```bash
# 使用 ghz 进行 gRPC 性能测试
ghz --insecure \
    --proto api/key/v1/key_service.proto \
    --call digitalkey.key.v1.KeyService/CreateKey \
    -d '{"userId":"usr_test","vehicleId":"veh_test","keyType":1,"protocol":1}' \
    -n 10000 \
    -c 100 \
    localhost:9000

# 输出示例
# Summary:
#   Count:   10000
#   Fastest: 5ms
#   Slowest: 245ms
#   Average: 28ms
#   RPS:     3424.65

# Percentiles:
#   50%:     25ms
#   90%:     35ms
#   95%:     42ms
#   99%:     68ms
#   99.9%:   150ms
```

### 7.4 测试覆盖矩阵

| 服务 | 单元测试覆盖 | 集成测试 | 性能目标 |
|------|------------|---------|---------|
| User Svc | ≥ 80% | 用户注册登录流程 | P99 < 100ms |
| Key Svc | ≥ 85% | 钥匙生命周期 | P99 < 200ms |
| Vehicle Svc | ≥ 80% | 车辆绑定与控制 | P99 < 300ms |
| KMS Svc | ≥ 90% | 密钥生成与签名 | P99 < 500ms |
| Auth Svc | ≥ 85% | Token 验证 | P99 < 50ms |
| OEM Adapter | ≥ 75% | 车企对接 | P99 < 500ms |
| Event Svc | ≥ 80% | 事件处理 | P99 < 100ms |
| **整体系统** | | 端到端测试 | TPS ≥ 100,000 |

---

## 8. 参考规范

| 规范 | 版本 | 说明 |
|------|------|------|
| OAuth 2.0 | RFC 6749 | 授权框架 |
| JWT | RFC 7519 | JSON Web Token |
| gRPC | v1.50+ | RPC 框架 |
| Protocol Buffers | proto3 | 接口定义语言 |
| TiDB | 7.0+ | 分布式数据库 |
| Redis | 7.0+ | 缓存 |
| Kafka | 3.6+ | 消息队列 |
| ISO 27001 | 2022 | 信息安全管理 |
| GDPR | - | 通用数据保护条例 |
| 中国数据安全法 | 2021 | 数据安全法律 |

---

*文档版本：v1.0 | 最后更新：2026-04-26 | 作者：code_designer*
