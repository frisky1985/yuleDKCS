## 任务背景
用户希望学习一份 OpenClaw Agent Prompt Templates Markdown 文件，整理成 multi-agents 软件开发工程范例文档，并上传到用户云盘。

## 执行过程
1. 解析用户提供的原始 Markdown（包含 7 个核心 Agent + 6 个子 Agent + 总指挥模板 + 通信协议速查表）
2. 在本地 `docs/multi-agent-workflow/` 目录下新建 `multi-agent-sw-dev-paradigm.md`，重新组织为规范文档
3. 调用 cloud-upload-backup skill 将文件上传至腾讯 SMH 云盘

## 关键结果
- 新建文件：`C:\Users\Lenovo\.qclaw\workspace\docs\multi-agent-workflow\multi-agent-sw-dev-paradigm.md`
- 文档结构：Agent 角色总览（含 ASCII 架构图）、Agent ID 速查表、8 阶段流水线详解、统一通信协议格式、流程示意图、实施建议
- 云盘链接：https://jsonproxy.3g.qq.com/urlmapper/irn8N1（30 天有效）

## 结论建议
文档已整理完毕并成功上传至云盘。该模板定义了 14 个 Agent 协作的完整流水线，覆盖需求→设计→开发→审查→测试→合并→交付全生命周期，具备并行开发、质量门禁、阻塞上报等工程化特性，可直接作为 multi-agent 软件开发项目的参考范例使用。