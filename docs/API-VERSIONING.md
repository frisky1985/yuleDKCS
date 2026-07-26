# yuleDKCS API 版本化策略

> 版本: v1.0 | 日期: 2026-07-27

## 版本化策略

yuleDKCS 采用 **URI Path 版本化** + **Header 内容协商** 双重策略。

### REST API 版本化

```
https://api.yule-technology.com/v1/keys
https://api.yule-technology.com/v2/keys
```

- 版本号嵌入 URI 路径：`/v{major}/`
- 仅 Major 版本体现在 URI 中
- Minor/Patch 版本通过 `Accept-Version` Header 协商

### gRPC API 版本化

```protobuf
package dkcs.v1;
package dkcs.v2;
```

- 版本号编码在 protobuf package 名称中
- 服务端同时注册多个版本

## 版本生命周期

```
alpha → beta → stable → deprecated → sunset
```

| 阶段 | 说明 | 持续时间 | 行为 |
|------|------|---------|------|
| **alpha** | 内部测试 | 不定 | 可随时变更，不保证兼容 |
| **beta** | 公开预览 | ≥ 30 天 | 功能冻结，仅修复 Bug |
| **stable** | 正式发布 | ≥ 6 个月 | 完全向后兼容 |
| **deprecated** | 废弃通知 | ≥ 3 个月 | 仍可用，返回 Deprecation Header |
| **sunset** | 下线 | — | 返回 410 Gone |

## 向后兼容承诺

Stable 版本保证以下兼容：

1. **请求兼容**: 不会移除或重命名必填字段
2. **响应兼容**: 不会移除字段，仅追加新字段
3. **语义兼容**: 不会改变既有 API 的语义
4. **错误格式**: 错误响应结构不变

### 允许的变更（不视为破坏性）

- 追加新的可选请求字段
- 追加新的响应字段
- 追加新的 API 端点
- 追加新的枚举值
- 提升错误消息的清晰度

## 废弃与迁移

1. 新版 API 发布时，旧版进入 deprecated 阶段
2. 在 `Deprecation` 和 `Sunset` HTTP Header 中标注下线日期
3. 向注册邮箱发送迁移通知（至少提前 90 天）
4. 旧版下线后返回 `410 Gone`
5. 提供迁移指南文档

## 版本号规范

遵循 [SemVer 2.0](https://semver.org/)：

```
MAJOR.MINOR.PATCH
```

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的 Bug 修复
