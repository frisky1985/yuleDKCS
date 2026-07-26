# 安全策略 (Security Policy)

## 报告安全漏洞 (Reporting a Vulnerability)

我们非常重视 yuleDKCS 数字钥匙系统的安全性。如果您发现了安全漏洞或有安全方面的疑虑，请通过以下方式报告：

- **电子邮件**: security@yule-technology.com
- **PGP 加密**: 如有敏感信息，建议使用 PGP 加密邮件（PGP 密钥详见下方）
- **替代渠道**: 如无法使用邮件，可通过 GitHub Issues 创建 **Private Security Advisory**（标记为 `security` 标签，不公开细节）

我们承诺对报告的漏洞进行**及时响应**，并在修复前对报告内容**严格保密**。

> 请勿在公开渠道（GitHub Issues、讨论组、社交媒体）披露未修复的漏洞。

## 预期响应时间 (Response SLA)

| 阶段 | 预期时间 | 说明 |
|------|----------|------|
| **确认收到** | 72 小时内 | 我们会在 3 个工作日内确认收到您的报告 |
| **初步评估** | 5 个工作日内 | 评估漏洞严重性、影响范围、修复难度 |
| **修复目标** | 严重/高危: 7 天内 | 发布紧急修复版本 (hotfix) |
| | 中危: 30 天内 | 纳入下一个常规发布 |
| | 低危: 90 天内 | 纳入后续版本计划 |
| **公开披露** | 修复发布后 | 修复完成后公开致谢和公告 |

## 支持的版本 (Supported Versions)

| 版本 | 安全更新支持 | 状态 |
|------|-------------|------|
| v2.1.x | ✅ 积极维护中 | 当前稳定版 |
| v2.0.x | ⚠️ 仅安全修复 | 维护模式 |
| v1.x   | ❌ 不再支持 | 已终止 |

## 安全公告 (Security Advisories)

安全公告将通过以下渠道发布：

1. **GitHub Security Advisories** — 在仓库的 Security 标签页发布
2. **CHANGELOG.md** — 每个版本的变更日志中标注安全修复
3. **邮件通知** — 影响严重的安全问题将以邮件形式通知已知的部署方

安全公告编号格式：`yuleDKCS-SA-YYYY-NNN`

## PGP 密钥 (PGP Key)

**TBD** — PGP 公钥信息将在后续补充。

如需在 PGP 密钥就绪前发送加密信息，请联系 security@yule-technology.com 获取临时安全传输方案。

## 漏洞分级标准 (Severity Classification)

| 级别 | 描述 | 示例 |
|------|------|------|
| **严重 (Critical)** | 可导致远程代码执行、未授权管理员访问、密钥材料泄露 | 数字钥匙私钥泄露、Hub 未授权 RCE |
| **高危 (High)** | 可导致权限提升、敏感数据泄露、身份伪造 | JWT 签名绕过、越权访问他人钥匙 |
| **中危 (Medium)** | 部分功能受影响，需特定条件触发 | 速率限制绕过、日志泄露敏感信息 |
| **低危 (Low)** | 影响有限，难以利用 | 信息泄露、非关键配置错误 |

## 致谢政策 (Acknowledgement Policy)

我们公开感谢所有负责任地报告安全问题的研究人员，除非报告人要求匿名。致谢将在每个版本的安全公告中列出，并将在以下位置展示：

- `docs/ACKNOWLEDGEMENTS.md`
- 对应安全公告的致谢章节
- 项目官网的安全页面

### 奖励计划 (Bug Bounty)

目前 yuleDKCS 尚未设立正式的漏洞奖励计划。对于影响重大的高危/严重漏洞报告，我们保留酌情提供奖励的权利。

## 安全最佳实践建议 (Security Best Practices for Deployers)

部署 yuleDKCS 的用户请遵循以下安全建议：

- **密钥管理**: 使用硬件安全模块（HSM）或 SE050 存储密钥材料
- **网络隔离**: Hub 和 DKCS 服务置于内网，仅通过 Ingress 暴露 REST API
- **TLS 加密**: 所有外部通信必须使用 TLS 1.3
- **定期轮换**: 定期轮换 JWT 签名密钥、数据库密码等敏感凭证
- **审计日志**: 开启审计日志，记录所有关键操作
- **最小权限**: 遵循最小权限原则配置 API 权限

## 联系方式 (Contact)

- 安全邮箱: security@yule-technology.com
- GitHub: https://github.com/yule-technology/yuleDKCS/security
