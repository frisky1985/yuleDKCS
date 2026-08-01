# Helm Chart: dkcs

> **部署方式说明（重要）**
>
> 本项目现行走 **kustomize** 部署（`backend/cloud/deploy/k8s/` + `overlays/staging/`），
> 是本仓库的**官方推荐部署路径**。本 Helm Chart 为历史保留的备选方案，
> 已同步为与 kustomize 一致的 PostgreSQL 架构，可作为参考或由运维团队
> 在需要 Helm 生态（如 Argo CD / Flux）时使用。

## 与 kustomize 的对齐点

| 组件 | kustomize（现行） | Helm（本 Chart） |
|:-----|:------------------|:-----------------|
| 数据库 | `deploy/k8s/postgres/statefulset.yaml` — postgres:16-alpine, 单实例 | `templates/postgres-statefulset.yaml` — 同镜像/同单实例 |
| 数据库账号 | `POSTGRES_USER=yuledkcs` / `POSTGRES_DB=yuledkcs` | `values.yaml → postgres.user/database` 默认一致 |
| 数据库密码 | Secret `dkcs-secrets` key `postgres-password` | Secret `{{ .Release.Name }}-secrets` key `postgres-password` |
| Hub 连接 | Secret key `database-url` | 同 key（模板自动拼接） |
| 管理 API 凭据 | Secret key `admin-username` / `admin-password` | 同 key（P1-2 fail-closed 必需） |

> 注：早期版本引用 MySQL（`mysql-statefulset` + `mysql-password`）已移除。

## 使用

```bash
helm upgrade --install dkcs ./backend/cloud/deploy/helm/dkcs \
  --namespace dkcs-prod \
  --set secrets.postgres.password='<强随机密码>' \
  --set secrets.admin.username='<admin>' \
  --set secrets.admin.password='<强随机密码>'
```

- 部署前请修改 `values.yaml` 中所有 `change-me-*` 占位值。
- dkcs 服务启动时自动执行 `db/migrations/` 下的 PG schema 迁移
  （环境变量 `DB_MIGRATIONS_DIR`，默认 `db/migrations`，镜像需包含迁移文件或挂载）。
