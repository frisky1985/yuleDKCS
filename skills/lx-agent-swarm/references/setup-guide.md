# 多智能体系统配置指南

本指南帮助你配置和部署完整的多智能体团队。

## 一、架构概览

```
主智能体 (main) - 🎯 AWS Claude Opus 4.6 - 任务编排中心
    │ sessions_spawn
    ├── 📋 pm        (Azure GPT-5)       → 规划者
    ├── 🔍 researcher (Azure GPT-5-Mini) → 信息猎手（快速）
    ├── 👨‍💻 coder     (Azure GPT-5-Codex) → 代码工匠（编程专用）
    ├── ✍️ writer    (Azure GPT-5)       → 文字工匠
    ├── 🎨 designer  (Qwen3-VL-Plus)     → 视觉创作者
    ├── 📊 analyst   (Azure GPT-5-Codex) → 数据侦探
    ├── 🔎 reviewer  (Azure O3)          → 质量守门人（推理）
    ├── 💬 assistant (Azure GPT-5-Mini)  → 沟通桥梁（快速）
    └── 🤖 automator (Azure GPT-5-Codex) → 效率大师（编程）
```

## 二、快速配置步骤

### 步骤 1：创建智能体工作目录

```bash
# 使用初始化脚本
python3 /workspace/openclaw/skills/agent-swarm/scripts/init_agents.py --base-path /workspace/agents

# 或手动创建
mkdir -p /workspace/agents/{pm,researcher,coder,writer,designer,analyst,reviewer,assistant,automator}
```

### 步骤 2：更新 openclaw.json 配置

在 `~/.openclaw/openclaw.json` 的 `agents` 部分添加配置：

```json
{
  "agents": {
    "list": [
      {
        "id": "main",
        "default": true,
        "workspace": "/workspace",
        "identity": {
          "name": "主智能体",
          "emoji": "🎯"
        },
        "subagents": {
          "allowAgents": [
            "pm", "researcher", "coder", "writer", "designer",
            "analyst", "reviewer", "assistant", "automator"
          ]
        }
      },
      {
        "id": "pm",
        "workspace": "/workspace/agents/pm",
        "model": { "primary": "chj-private/azure-gpt-5" },
        "identity": { "name": "产品经理", "emoji": "📋" },
        "tools": {
          "allow": ["read", "write", "edit", "web_search", "web_fetch", "memory_search", "memory_get"],
          "deny": ["exec", "process", "gateway", "browser", "message", "cron"]
        }
      },
      {
        "id": "researcher",
        "workspace": "/workspace/agents/researcher",
        "model": { "primary": "chj-private/azure-gpt-5-mini" },
        "identity": { "name": "研究员", "emoji": "🔍" },
        "tools": {
          "allow": ["web_search", "web_fetch", "read", "write", "memory_search", "memory_get"],
          "deny": ["exec", "process", "gateway", "browser", "message", "cron", "edit"]
        }
      },
      {
        "id": "coder",
        "workspace": "/workspace/agents/coder",
        "model": { "primary": "chj-private/azure-gpt-5-codex" },
        "identity": { "name": "程序员", "emoji": "👨‍💻" },
        "tools": {
          "allow": ["read", "write", "edit", "exec", "process"],
          "deny": ["web_search", "web_fetch", "browser", "message", "gateway", "cron"]
        }
      },
      {
        "id": "writer",
        "workspace": "/workspace/agents/writer",
        "model": { "primary": "chj-private/azure-gpt-5" },
        "identity": { "name": "写作者", "emoji": "✍️" },
        "tools": {
          "allow": ["read", "write", "edit", "memory_search", "memory_get"],
          "deny": ["exec", "process", "browser", "gateway", "message", "cron", "web_search", "web_fetch"]
        }
      },
      {
        "id": "designer",
        "workspace": "/workspace/agents/designer",
        "model": { "primary": "chj-private/bailian-qwen3-vl-plus" },
        "identity": { "name": "设计师", "emoji": "🎨" },
        "tools": {
          "allow": ["read", "write"],
          "deny": ["exec", "process", "browser", "gateway", "message", "edit", "cron", "web_search", "web_fetch"]
        }
      },
      {
        "id": "analyst",
        "workspace": "/workspace/agents/analyst",
        "model": { "primary": "chj-private/azure-gpt-5-codex" },
        "identity": { "name": "分析师", "emoji": "📊" },
        "tools": {
          "allow": ["read", "write", "edit", "exec"],
          "deny": ["browser", "gateway", "message", "web_search", "web_fetch", "cron", "process"]
        }
      },
      {
        "id": "reviewer",
        "workspace": "/workspace/agents/reviewer",
        "model": { "primary": "chj-private/azure-o3" },
        "identity": { "name": "审核员", "emoji": "🔎" },
        "tools": {
          "allow": ["read", "memory_search", "memory_get"],
          "deny": ["write", "edit", "exec", "process", "browser", "gateway", "message", "cron", "web_search", "web_fetch"]
        }
      },
      {
        "id": "assistant",
        "workspace": "/workspace/agents/assistant",
        "model": { "primary": "chj-private/azure-gpt-5-mini" },
        "identity": { "name": "助手", "emoji": "💬" },
        "tools": {
          "allow": ["message", "read", "sessions_send"],
          "deny": ["write", "edit", "exec", "process", "browser", "gateway", "cron", "web_search", "web_fetch"]
        }
      },
      {
        "id": "automator",
        "workspace": "/workspace/agents/automator",
        "model": { "primary": "chj-private/azure-gpt-5-codex" },
        "identity": { "name": "自动化", "emoji": "🤖" },
        "tools": {
          "allow": ["exec", "process", "cron", "browser", "read", "write"],
          "deny": ["message", "gateway", "web_search", "web_fetch"]
        }
      }
    ]
  }
}
```

### 步骤 3：重启 Gateway

```bash
openclaw gateway restart
```

## 三、智能体工具权限矩阵

| 智能体 | read | write | edit | exec | process | web_search | web_fetch | browser | message | cron | memory |
|--------|:----:|:-----:|:----:|:----:|:-------:|:----------:|:---------:|:-------:|:-------:|:----:|:------:|
| 📋 pm | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ |
| 🔍 researcher | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ |
| 👨‍💻 coder | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| ✍️ writer | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| 🎨 designer | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| 📊 analyst | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| 🔎 reviewer | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| 💬 assistant | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| 🤖 automator | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ | ✅ | ❌ |

### 权限设计原则

1. **最小权限** — 只给必要的工具
2. **职责隔离** — reviewer 只读不写，防止直接修改
3. **安全边界** — gateway/message 等敏感工具严格限制
4. **成本控制** — researcher/assistant 用便宜模型

## 四、添加新模型

### 配置示例（以 Gemini 3 Pro Image 为例）

```json
{
  "models": {
    "vendor-gemini-3-pro-image": {
      "baseUrl": "https://your-gateway-url/v1beta",
      "apiKey": "YOUR_API_KEY_HERE",
      "api": "google-generative-ai",
      "authHeader": "x-goog-api-key",
      "models": [
        {
          "id": "gemini-3-pro-image-preview",
          "name": "Gemini 3 Pro Image",
          "reasoning": false,
          "input": ["text", "image"],
          "contextWindow": 1000000,
          "maxTokens": 65536
        }
      ]
    }
  },
  "modelAliases": {
    "gemini-image": "vendor-gemini-3-pro-image/gemini-3-pro-image-preview"
  }
}
```

### 关键配置项说明

| 配置项 | 说明 |
|--------|------|
| baseUrl | 模型 API 的基础 URL |
| apiKey | API 密钥 |
| api | API 格式类型（openai / google-generative-ai / anthropic） |
| authHeader | 认证头名称 |
| models[].id | 模型 ID，用于调用时指定 |
| models[].input | 支持的输入类型 |

## 五、验证配置

```bash
# 检查智能体列表
openclaw agents list

# 测试 spawn
# 在主会话中执行
sessions_spawn({ task: "测试任务", agentId: "researcher" })

# 查看子会话
openclaw sessions list
```

## 六、常见问题

| 问题 | 解决方案 |
|------|----------|
| spawn 报错 "agentId is not allowed" | 检查主智能体的 `subagents.allowAgents` 是否包含目标智能体 ID |
| 子智能体无法使用某工具 | 检查该智能体的 `tools.allow` 和 `tools.deny` 配置 |
| 401 Unauthorized | 检查 apiKey 是否正确 |
| 400 Bad Request | 检查 api 字段是否与模型匹配 |
| 子智能体输出为空 | 检查 task 描述是否清晰，包含输出要求 |

## 七、智能体人格文件

每个智能体工作目录应包含：

| 文件 | 用途 | 必需 |
|------|------|:----:|
| SOUL.md | 人格定义、核心原则 | ✅ |
| AGENTS.md | 工作规范、可用工具 | ✅ |
| BOOTSTRAP.md | 首次运行指引 | 可选 |
| IDENTITY.md | 身份信息 | 可选 |
| TOOLS.md | 工具使用笔记 | 可选 |

详细的人格模板见 `scripts/init_agents.py`
