# Java 适配器测试启动计划

**日期**: 2026-07-18
**模块**: `backend/adapters/`
**当前状况**: 14 个 Java 源文件，**零测试文件**

---

## 1. 现状概览

### 模块结构

```
backend/adapters/
├── pom.xml                          # 父 POM (Spring Boot 3.2.5)
├── docker/
├── adapter-core/                    # 核心抽象层 (7 files, 752 LOC)
│   └── src/main/java/com/digitalkey/adapter/core/
│       ├── TspAdapter.java           (104行 - 接口定义)
│       ├── AbstractTspAdapter.java   (162行 - 抽象基类)
│       ├── AdapterRegistry.java      (173行 - 适配器注册)
│       ├── AdapterConfig.java        (120行 - Spring 配置)
│       ├── AdapterCoreApplication.java (20行 - 启动入口)
│       ├── AdapterHealthIndicator.java (37行 - 健康检查)
│       └── AdapterMetrics.java       (136行 - 指标/Micrometer)
├── adapter-ccc/                      # CCC 适配器 (2 files, 232 LOC)
│   └── src/main/java/com/digitalkey/adapter/ccc/
│       ├── CccAdapter.java           (73行)
│       └── CccClient.java            (159行)
├── adapter-iccoa/                    # iCCOA 适配器 (2 files, 232 LOC)
│   └── src/main/java/com/digitalkey/adapter/iccoa/
│       ├── IccoaAdapter.java         (74行)
│       └── IccoaClient.java          (158行)
├── adapter-icce/                     # ICCE 适配器 (1 file, 109 LOC)
│   └── src/main/java/com/digitalkey/adapter/icce/
│       └── IcceAdapter.java          (109行)
└── adapter-grpc-server/              # gRPC 服务器适配器 (2 files, 296 LOC)
    └── src/main/java/com/digitalkey/adapter/grpcserver/
        ├── GrpcServer.java           (35行)
        └── AdapterServiceImpl.java   (261行)
```

**总计**: 14 源文件, 1621 行代码, 0 测试文件

### 技术栈

| 项 | 值 |
|----|-----|
| Java | 17 |
| Spring Boot | 3.2.5 |
| gRPC | 1.62.2 |
| Protobuf | 3.25.4 |
| Micrometer | 1.13.4 |
| Build | Maven (父 POM 管理) |

---

## 2. 环境问题

**当前机器未安装 Java/Maven**:

```bash
$ java --version
=> 未安装
$ mvn --version
=> 未安装
```

**无 test 目录** — 检查结果：

```bash
$ find backend/adapters -type d -name "test"
=> (空结果)
$ find backend/adapters -name "*Test*"
=> (空结果)
```

---

## 3. 测试计划

### 阶段 1 — 环境准备（推荐使用 Docker）

由于本地无 Java 环境，推荐使用 Docker 进行构建和测试：

```bash
# 使用 Maven Docker 镜像运行测试
cd /Users/stefan/.openclaw/workspace/yuleDKCS/backend/adapters
docker run -it --rm \
  -v "$PWD":/app \
  -w /app \
  -v "$HOME/.m2":/root/.m2 \
  maven:3.9-eclipse-temurin-17 \
  mvn test
```

或手动安装：

```bash
# macOS
brew install openjdk@17 maven

# Linux (Ubuntu/Debian)
sudo apt install openjdk-17-jdk maven
```

### 阶段 2 — 测试优先级

按业务风险排序：

| 优先级 | 模块 | 测试重点 | 文件数 | 说明 |
|--------|------|---------|--------|------|
| P0 | `adapter-core` | TspAdapter 接口契约、AbstractTspAdapter 默认行为 | 7 | 所有适配器依赖于 core |
| P0 | `adapter-core` | AdapterRegistry 注册/查找逻辑 | - | 核心路由逻辑 |
| P0 | `adapter-core` | AdapterMetrics Micrometer 指标 | - | 监控关键路径 |
| P1 | `adapter-icce` | IcceAdapter TSP 交互流程 | 1 | 仅 14 文件中最小但关键 |
| P1 | `adapter-iccoa` | IccoaAdapter + IccoaClient | 2 | iCCOA 协议 |
| P1 | `adapter-ccc` | CccAdapter + CccClient | 2 | CCC 协议 |
| P2 | `adapter-grpc-server` | AdapterServiceImpl gRPC 处理 | 2 | 集成层 |
| P2 | `adapter-grpc-server` | GrpcServer 启动配置 | - | 基础设施 |

### 阶段 3 — 推荐的测试框架与依赖

```xml
<!-- adapter-core/pom.xml 添加测试依赖 -->
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-test</artifactId>
        <scope>test</scope>
    </dependency>
    <dependency>
        <groupId>org.mockito</groupId>
        <artifactId>mockito-core</artifactId>
        <scope>test</scope>
    </dependency>
    <dependency>
        <groupId>io.grpc</groupId>
        <artifactId>grpc-testing</artifactId>
        <version>${grpc.version}</version>
        <scope>test</scope>
    </dependency>
</dependencies>
```

### 阶段 4 — 测试示例骨架

```java
// adapter-core/src/test/java/com/digitalkey/adapter/core/TspAdapterTest.java
class TspAdapterTest {

    @Test
    void shouldRegisterAndLookupAdapter() {
        AdapterRegistry registry = new AdapterRegistry();
        registry.register("icce", mockIcceAdapter);
        assertNotNull(registry.lookup("icce"));
    }

    @Test
    void shouldThrowOnUnknownAdapter() {
        AdapterRegistry registry = new AdapterRegistry();
        assertThrows(AdapterNotFoundException.class,
            () -> registry.lookup("unknown"));
    }
}
```

### 阶段 5 — 测试覆盖率目标

| 模块 | 目标覆盖率 | 关键路径 |
|------|-----------|---------|
| adapter-core | ≥ 85% | 注册/查找/指标/健康检查 |
| adapter-icce | ≥ 75% | TSP 交互超时/重试/错误处理 |
| adapter-iccoa | ≥ 75% | 客户端调用/签名验证 |
| adapter-ccc | ≥ 75% | 客户端调用/消息序列化 |
| adapter-grpc-server | ≥ 70% | 请求转发/错误映射 |

---

## 4. 手动检查结果

通过阅读源码，发现以下需要测试覆盖的关键场景：

### adapter-core
- `TspAdapter` 接口: `shareKey()`, `revokeKey()`, `getKeyStatus()` 方法返回 CompletableFuture，需要测试超时和异常路径
- `AdapterRegistry`: 线程安全、重复注册、未注册查找
- `AbstractTspAdapter`: 模板方法模式，需验证骨架逻辑

### adapter-ccc / adapter-iccoa / adapter-icce
- TSP HTTP 调用超时处理
- 签名生成与验证
- 错误响应映射

### adapter-grpc-server
- gRPC service 定义与实现映射
- 错误码转换 (gRPC status ↔ 业务错误)

---

## 5. 建议下一步

1. **立即**: 在 CI/CD 环境中安装 Java 17 + Maven，确保 `mvn compile` 通过
2. **此 sprint**: 为 `adapter-core` 编写单元测试（预计 ~15 测试用例）
3. **下个 sprint**: 覆盖 `adapter-icce` + `adapter-iccoa`
4. **后续**: `adapter-ccc` ↔ `adapter-grpc-server` 集成测试

---

*本计划基于 2026-07-18 代码快照生成。*
