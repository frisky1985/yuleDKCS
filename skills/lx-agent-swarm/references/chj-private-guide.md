# CHJ-Private 模型配置指南

本指南专门针对使用 **chj-private** 提供商的用户，提供最优的模型选择方案。

## 模型选择策略

我们根据每个智能体的核心任务特点，从 chj-private 提供商中精心挑选了最适合的模型：

| 智能体 | 模型 ID | 选择理由 |
|--------|---------|----------|
| 🎯 **main** (主智能体) | `aws-claude-opus-4-6` | 最强推理和编排能力，适合复杂任务分解 |
| 📋 **pm** (规划者) | `azure-gpt-5` | 综合能力强，适合需求分析和项目规划 |
| 🔍 **researcher** (研究员) | `azure-gpt-5-mini` | 快速响应，成本低，适合信息搜索和整理 |
| 👨‍💻 **coder** (程序员) | `azure-gpt-5-codex` | 编程专用模型，代码质量高 |
| ✍️ **writer** (写作者) | `azure-gpt-5` | 语言能力强，适合文档写作 |
| 🎨 **designer** (设计师) | `bailian-qwen3-vl-plus` | 视觉生成模型，支持图像处理 |
| 📊 **analyst** (分析师) | `azure-gpt-5-codex` | 数据处理+编程能力，适合数据分析 |
| 🔎 **reviewer** (审核员) | `azure-o3` | 推理模型，逻辑严密，适合代码审查 |
| 💬 **assistant** (助手) | `azure-gpt-5-mini` | 快速响应，适合简单问答和消息转发 |
| 🤖 **automator** (自动化) | `azure-gpt-5-codex` | 编程能力强，适合编写自动化脚本 |

## 快速配置步骤

### 步骤 1: 复制配置模板

将 `chj-private-config-template.json` 中的 `agents.list` 配置复制到你的 `~/.openclaw/openclaw.json` 文件中。

```bash
# 查看模板配置
cat /workspace/.agents/skills/agent-swarm/references/chj-private-config-template.json

# 备份当前配置
cp ~/.openclaw/openclaw.json ~/.openclaw/openclaw.json.backup

# 编辑配置文件
# 将模板中的 agents.list 部分合并到你的 openclaw.json
```

### 步骤 2: 确认 chj-private 提供商配置

确保你的 `openclaw.json` 中已经配置了 `chj-private` 提供商：

```json
{
  "models": {
    "providers": {
      "chj-private": {
        "baseUrl": "http://127.0.0.1:8765/v1",
        "api": "openai-completions",
        "apiKey": "proxy-handled",
        "models": [
          // ... 你的模型列表
        ]
      }
    }
  }
}
```

### 步骤 3: 创建智能体工作目录

```bash
# 使用初始化脚本创建目录
python3 /workspace/.agents/skills/agent-swarm/scripts/init_agents.py --base-path /workspace/agents

# 脚本会自动创建以下目录：
# /workspace/agents/pm/
# /workspace/agents/researcher/
# /workspace/agents/coder/
# /workspace/agents/writer/
# /workspace/agents/designer/
# /workspace/agents/analyst/
# /workspace/agents/reviewer/
# /workspace/agents/assistant/
# /workspace/agents/automator/
```

### 步骤 4: 重启 Gateway

```bash
openclaw gateway restart
```

### 步骤 5: 验证配置

```bash
# 查看智能体列表
openclaw agents list

# 应该显示所有 9 个智能体及其配置的模型
```

## 模型能力对照表

| 模型系列 | 适用场景 | 特点 |
|---------|----------|------|
| **Azure GPT-5** | 通用任务 | 综合能力强，平衡性好 |
| **Azure GPT-5-Codex** | 编程任务 | 代码生成、数据处理、自动化脚本 |
| **Azure GPT-5-Mini** | 快速任务 | 响应快，成本低，适合简单任务 |
| **Azure O3** | 推理任务 | 逻辑严密，适合审查、复杂推理 |
| **Bailian Qwen3-VL-Plus** | 视觉任务 | 图像理解和生成 |
| **AWS Claude Opus 4.6** | 复杂编排 | 最强推理能力，任务编排 |

## 成本优化建议

1. **分级使用模型**
   - 简单任务（搜索、转发）→ Azure GPT-5-Mini
   - 编程任务（代码、分析、自动化）→ Azure GPT-5-Codex
   - 通用任务（规划、写作）→ Azure GPT-5
   - 推理任务（审查、复杂决策）→ Azure O3
   - 视觉任务（配图、图表）→ Bailian Qwen3-VL-Plus
   - 编排任务（主智能体）→ AWS Claude Opus 4.6

2. **并行执行提高效率**
   - 多个 researcher 同时搜索不同主题
   - 独立的子任务并行处理，缩短总时间

3. **避免过度编排**
   - 简单任务不要分解成多个子任务
   - 能一次完成的不要分两次

## 使用示例

### 示例 1: 技术调研项目

```javascript
// 并行搜索多个框架（使用 azure-gpt-5-mini，快速且便宜）
["LangChain", "AutoGPT", "CrewAI"].forEach(name => {
  sessions_spawn({
    task: `搜索 ${name} 框架的特点、优缺点、适用场景，输出到 /workspace/research/${name.toLowerCase()}.md`,
    agentId: "researcher", // 自动使用 azure-gpt-5-mini
    label: `research-${name.toLowerCase()}`
  })
})

// 等待调研完成后，使用 writer 整合（使用 azure-gpt-5）
sessions_spawn({
  task: "基于 /workspace/research/ 中的资料，撰写框架对比分析文章",
  agentId: "writer" // 自动使用 azure-gpt-5
})

// 最后使用 reviewer 审核（使用 azure-o3，推理能力强）
sessions_spawn({
  task: "审核文章质量，检查逻辑严密性和完整性",
  agentId: "reviewer" // 自动使用 azure-o3
})
```

### 示例 2: 代码重构项目

```javascript
// 使用 coder 重构代码（使用 azure-gpt-5-codex）
sessions_spawn({
  task: "重构 /workspace/src/auth.js，提取公共逻辑，添加错误处理",
  agentId: "coder" // 自动使用 azure-gpt-5-codex
})

// 使用 reviewer 审查代码（使用 azure-o3）
sessions_spawn({
  task: "审查重构后的代码，检查安全性、可读性和最佳实践",
  agentId: "reviewer" // 自动使用 azure-o3
})
```

### 示例 3: 数据分析报告

```javascript
// 使用 analyst 分析数据（使用 azure-gpt-5-codex）
sessions_spawn({
  task: "分析 /workspace/data/sales.csv，计算关键指标，发现趋势",
  agentId: "analyst" // 自动使用 azure-gpt-5-codex
})

// 使用 writer 撰写报告（使用 azure-gpt-5）
sessions_spawn({
  task: "基于分析结果，撰写月度销售分析报告",
  agentId: "writer" // 自动使用 azure-gpt-5
})

// 使用 designer 生成图表（使用 bailian-qwen3-vl-plus）
sessions_spawn({
  task: "生成销售趋势图和数据可视化图表",
  agentId: "designer" // 自动使用 bailian-qwen3-vl-plus
})
```

## 常见问题

### Q: 如何临时覆盖某个智能体的默认模型？

A: 在 `sessions_spawn` 调用时指定 `model` 参数：

```javascript
sessions_spawn({
  task: "特别复杂的代码审查任务",
  agentId: "reviewer",
  model: "chj-private/aws-claude-opus-4-6" // 临时使用更强的模型
})
```

### Q: 如何查看当前智能体使用的模型？

A: 使用命令行工具：

```bash
openclaw agents list
```

### Q: 某个模型不可用怎么办？

A: 检查以下几点：
1. 确认 `chj-private` 提供商的 baseUrl 可访问
2. 确认模型 ID 在 `openclaw.json` 的 `models` 列表中
3. 检查 gateway 日志：`openclaw gateway logs`

### Q: 如何添加新的智能体？

A: 参考 [setup-guide.md](setup-guide.md) 中的智能体管理部分，或使用脚本：

```bash
python3 /workspace/.agents/skills/agent-swarm/scripts/agent_manager.py add my_agent \
  --name "我的智能体" \
  --emoji "🚀" \
  --model "chj-private/azure-gpt-5"
```

## 相关资源

- [完整配置指南](setup-guide.md) - 深入了解智能体配置
- [SKILL.md](../SKILL.md) - 技能使用说明
- [智能体管理脚本](../scripts/agent_manager.py) - 配置管理工具

## 反馈与支持

如果你在使用过程中遇到问题或有改进建议，欢迎反馈。
