# 📊 数据库选型决策记录

> **日期**: 2026-07-30
> **背景**: OPS-RUNBOOK.md 描述使用 **MySQL 8.0**，DEPLOYMENT_GUIDE.md 描述使用 **PostgreSQL 15+**，存在不一致。

---

## 环境检查

| 文件 | 声称数据库 | 影响 |
|:-----|:----------:|:-----|
| `docs/OPS-RUNBOOK.md` §3 | MySQL 8.0 | 运维操作手册 |
| `docs/DEPLOYMENT_GUIDE.md` §1.1 | PostgreSQL 15+ | 部署配置指南 |
| `docker-compose.yml` (deploy/) | PostgreSQL 15 | 实际部署模板 |

---

## 决策：采用 **PostgreSQL 15+**

### 选择理由

| 维度 | PostgreSQL | MySQL | 说明 |
|:-----|:----------:|:-----:|:-----|
| 已有代码 | 实际 docker-compose 使用 PostgreSQL | ❌ 无 MySQL 模板 | 迁移成本为零 |
| JSONB 支持 | ✅ 原生、可索引 | ⚠️ JSON 功能较弱 | 密钥管理需存储灵活 JSON 结构 |
| 并发控制 | MVCC 实现更成熟 | ✅ 足够 | 数字钥匙高并发场景 |
| 地理空间 | PostGIS ✅ | ⚠️ 需额外配置 | 未来停车场/充电桩场景 |
| 许可证 | PostgreSQL 许可（宽松） | GPL / 商业 | 开源项目友好 |
| Go 生态 | pgx ✅ 纯 Go 驱动 | go-sql-driver ✅ | 两者均可 |
| K8s 运维 | CloudNativePG / CrunchyData ✅ | MySQL Operator ✅ | 两者均可 |
| 社区活跃 | 活跃，版本迭代快 | 广泛 | 无显著差异 |

### 否决 MySQL 的原因

1. 部署模板、Docker Compose 已用 PostgreSQL，切换成本高
2. 密钥系统需存储嵌套 JSON（权限位、证书链），PostgreSQL JSONB 更优
3. 项目使用 Go pgx 驱动（已在 go.mod 中），切换需改代码

---

## 更新操作

以下文件已统一为 PostgreSQL：

| 文件 | 改动 |
|:-----|:-----|
| `docs/OPS-RUNBOOK.md` | 所有 "MySQL 8.0" → "PostgreSQL 15+" |
| `docs/DEPLOYMENT_GUIDE.md` | 已确认一致（原即为 PostgreSQL 描述） |

---

## 后续注意事项

- 所有新文档中数据库统一写 **PostgreSQL 15+**
- K8s 部署使用 **CloudNativePG Operator** 管理有状态 PostgreSQL 集群
- 备份策略：WAL 持续归档 + 每日全量（参考 `docs/SLA.md` §7）
- 生产环境部署主从架构（至少一主一备）
