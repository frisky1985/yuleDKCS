# Staging 环境配置分析报告

**日期**: 2026-07-16

---

## 现有配置

| 文件 | 用途 | 存在 |
|---|---|---|
| `docker-compose.yml` | 开发/基础配置 | ✅ |
| `docker-compose.prod.yml` | 生产环境配置 | ✅ |
| `docker-compose.staging.yml` | 预发布环境配置 | ❌ **不存在** |

## docker-compose.prod.yml 分析

当前生产配置包含以下服务：
- **hub** — gRPC + REST 服务 (8080/9090)
- **postgres** — PostgreSQL 16 Alpine
- **redis** — Redis 7 Alpine
- **zookeeper** — Kafka 依赖
- **kafka** — 消息队列

## Staging 配置建议

Stage（预发布）环境应介于 dev 和 prod 之间：
1. 使用与 prod **相似但更轻量的基础设施**（降低资源消耗）
2. 启用 **debug 级别的日志和调试端点**
3. 使用卷保留数据（便于调试）
4. 降低健康检查频率
5. **不映射敏感端口到宿主机**

### 推荐变更对照

| 项 | prod | staging (建议) |
|---|---|---|
| GIN_MODE | release | debug |
| LOG_LEVEL | info | debug |
| 端口映射 | hub:8080/9090 暴露 | 仅内部网络或 localhost |
| JWT_SECRET | ${JWT_SECRET} 变量 | 固定测试密钥 |
| PostgreSQL 端口 | 5432 暴露 | 仅内部网络 |
| Kafka 版本 | 7.6 | 7.6 (一致) |
| 健康检查间隔 | hub 30s / PG 5s | 放宽 (60s/15s) |
| 重启策略 | unless-stopped | unless-stopped |
| 资源限制 | 无 | 建议设置 CPU/内存上限 |

### 已创建: `docker-compose.staging.yml`

> 文件路径: `/Users/stefan/yuleDKCS/docker-compose.staging.yml`
> 
> 基于 `docker-compose.prod.yml` 修改，移除了端口暴露、切换为 debug 模式、添加了资源限制。
