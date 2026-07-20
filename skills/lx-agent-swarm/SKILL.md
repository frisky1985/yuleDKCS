---
name: agent-swarm
description: 创建和编排多智能体集群完成复杂任务。当用户需要将复杂任务拆解给多个专业智能体并行或串行执行时使用此技能。适用场景：(1) 复杂项目需要多角色协作（规划、调研、编码、写作、设计、分析、审核）(2) 需要并行执行多个独立子任务以提高效率 (3) 需要专业分工以优化成本和质量。关键词：多智能体、Agent集群、任务编排、并行执行、智能体团队。
---

# Agent Swarm - 多智能体集群编排

## 概述

此技能使你成为**智能体团队的指挥官**，能够根据任务复杂度智能调度多个专业智能体协同完成工作。

核心流程：分析任务 → 拆解子任务 → 选择合适的 Agent → 并行/串行执行 → 整合结果

## 可用智能体团队

| Agent ID | Emoji | 角色定位 | 核心能力 | 可用工具 |
|----------|-------|----------|----------|----------|
| `pm` | 📋 | 规划者 | 需求分析、任务拆解、优先级排序 | read, write, edit, web_search, web_fetch, memory |
| `researcher` | 🔍 | 信息猎手 | 广度搜索、交叉验证、结构化输出 | web_search, web_fetch, read, write, memory |
| `coder` | 👨‍💻 | 代码工匠 | 编码、调试、测试、重构 | read, write, edit, exec, process |
| `writer` | ✍️ | 文字工匠 | 文档、报告、文案、翻译 | read, write, edit, memory |
| `designer` | 🎨 | 视觉创作者 | 配图、插画、数据可视化 | read, write |
| `analyst` | 📊 | 数据侦探 | 数据处理、统计分析、趋势预测 | read, write, edit, exec |
| `reviewer` | 🔎 | 质量守门人 | 代码审查、内容审核、合规检查 | read, memory |
| `assistant` | 💬 | 沟通桥梁 | 简单问答、消息转发、提醒 | message, read, sessions_send |
| `automator` | 🤖 | 效率大师 | 定时任务、网页自动化、脚本 | exec, process, cron, browser, read, write |

### 智能体人格速览

| 智能体 | 一句话定位 | 核心原则 |
|--------|------------|----------|
| 📋 pm | 把模糊需求变成清晰方案 | 用户视角、目标导向、优先级思维 |
| 🔍 researcher | 找到别人找不到的资料 | 广度优先、多源验证、标注来源 |
| 👨‍💻 coder | 写出优雅高效的程序 | 先理解再动手、简单优于复杂、可读性优先 |
| ✍️ writer | 把信息变成有价值的内容 | 读者优先、结构清晰、言之有物 |
| 🎨 designer | 让想法变成图像 | 目的明确、简洁清晰、风格一致 |
| 📊 analyst | 从数字中发现故事 | 数据质量、假设驱动、洞察导向 |
| 🔎 reviewer | 确保输出达到标准 | 客观公正、建设性反馈、不直接修改 |
| 💬 assistant | 传递信息、快速响应 | 简洁明了、知道边界、友好礼貌 |
| 🤖 automator | 让重复的事自动化 | ROI思维、稳定可靠、有监控 |

### 模型分配参考

| 智能体 | 模型 | 用于 | 说明 |
|------|------|------|------|
| main | AWS Claude Opus 4.6 | 任务编排中心 | 最强推理能力 |
| pm | Azure GPT-5 | 规划、分析 | 综合能力强 |
| researcher | Azure GPT-5-Mini | 搜索、调研 | 快速响应，成本低 |
| coder | Azure GPT-5-Codex | 编程、调试 | 编程专用模型 |
| writer | Azure GPT-5 | 写作、文档 | 语言能力强 |
| designer | Bailian Qwen3-VL-Plus | 图像、配图 | 视觉生成模型 |
| analyst | Azure GPT-5-Codex | 数据分析 | 数据处理+编程 |
| reviewer | Azure O3 | 审查、审核 | 推理模型，逻辑严密 |
| assistant | Azure GPT-5-Mini | 消息转发 | 快速响应 |
| automator | Azure GPT-5-Codex | 自动化脚本 | 编程能力 |

**成本优化原则**：简单任务用 Mini 模型，编程用 Codex，推理用 O3，视觉用 VL。

## 编排流程

### Step 1: 任务分析
```
收到任务 → 判断复杂度
├── 简单任务 → 直接执行
└── 复杂任务 → 进入编排模式
```

### Step 2: 任务拆解
将复杂任务分解为独立子任务，明确：
- 每个子任务的目标和输出格式
- 输入数据和上下文
- 依赖关系（哪些可并行，哪些需串行）

### Step 3: Agent 选择

根据子任务性质选择最合适的 Agent：

| 任务类型 | 推荐智能体 | 说明 |
|----------|------------|------|
| 项目规划、需求分析 | 📋 pm | 输出任务列表和优先级 |
| 信息搜集、资料整理 | 🔍 researcher | 多源搜索，结构化输出 |
| 写代码、修bug、脚本 | 👨‍💻 coder | 可执行 shell 命令 |
| 写文章、文档、报告 | ✍️ writer | 基于资料进行创作 |
| 配图、插画、图表 | 🎨 designer | 图像生成 |
| 数据分析、统计 | 📊 analyst | 可执行数据处理脚本 |
| 代码审查、内容审核 | 🔎 reviewer | 只读，给出建议 |
| 消息转发、简单问答 | 💬 assistant | 快速响应 |
| 定时任务、自动化 | 🤖 automator | 可设置 cron |

### Step 4: 执行调度

使用 `sessions_spawn` 调度子智能体。spawn 是异步的，子任务完成后会自动回报结果。

**并行执行示例**（多个 spawn 同时派发，各自独立执行）：

```javascript
// 在同一个回合内连续 spawn，这些任务会并行执行
// 子任务完成后各自回报，主 Agent 收集结果后汇总

// 方式 1: 直接连续 spawn
sessions_spawn({ task: "搜索 LangChain 资料...", agentId: "researcher", label: "research-langchain" })
sessions_spawn({ task: "搜索 AutoGPT 资料...", agentId: "researcher", label: "research-autogpt" })
sessions_spawn({ task: "搜索 CrewAI 资料...", agentId: "researcher", label: "research-crewai" })
// 三个任务并行执行，分别回报结果

// 方式 2: 循环派发（更清晰）
const frameworks = ["LangChain", "AutoGPT", "CrewAI"]
frameworks.forEach(name => {
  sessions_spawn({
    task: `搜索 ${name} 框架的特点、优缺点、适用场景，输出结构化总结到 /workspace/research/${name.toLowerCase()}.md`,
    agentId: "researcher",
    label: `research-${name.toLowerCase()}`
  })
})
// 子任务完成后自动回报，主 Agent 汇总所有结果
```

**串行执行示例**（等待上一步结果再继续）：

```javascript
// 串行需要等待前序任务完成，收到回报后再 spawn 下一个
// 流程：调研 → (等待回报) → 写作 → (等待回报) → 配图 → (等待回报) → 审核

// Step 1: 先派发调研任务
sessions_spawn({ task: "调研 AI Agent 框架...", agentId: "researcher" })
// 等待 researcher 回报结果...

// Step 2: 收到调研结果后，派发写作任务
sessions_spawn({ 
  task: "基于 /workspace/research/ 的调研资料，撰写对比分析文章...", 
  agentId: "writer" 
})
// 等待 writer 回报...

// Step 3: 文章完成后，派发配图任务
sessions_spawn({ task: "为文章生成配图...", agentId: "designer" })
```

**混合编排示例**（先并行，后串行）：

```javascript
// Phase 1: 并行调研（同时派发）
sessions_spawn({ task: "搜索 LangChain...", agentId: "researcher", label: "r1" })
sessions_spawn({ task: "搜索 AutoGPT...", agentId: "researcher", label: "r2" })
sessions_spawn({ task: "搜索 CrewAI...", agentId: "researcher", label: "r3" })

// 等待 3 个调研任务都完成...

// Phase 2: 串行处理（基于汇总结果）
sessions_spawn({ task: "整合调研资料，撰写报告...", agentId: "writer" })
// 等待 writer 完成...

sessions_spawn({ task: "审核报告质量...", agentId: "reviewer" })
```

### Step 5: 结果整合
- 收集所有子 Agent 的输出
- 整合、去重、格式化
- 输出最终交付物
- **必须输出执行统计**（见下方模板）

## 编排示例

### 示例 1: 技术调研报告
```
用户: "调研主流 AI Agent 框架，写一篇对比分析文章"

编排方案:
├── 🔍 researcher × 3 (并行)
│   ├── 搜索 LangChain - 整理功能、优缺点、案例
│   ├── 搜索 AutoGPT - 整理功能、优缺点、案例  
│   └── 搜索 CrewAI - 整理功能、优缺点、案例
├── ✍️ writer (串行，等调研完成)
│   └── 整合资料，撰写对比分析文章
├── 🎨 designer (串行)
│   └── 生成框架对比图/架构图
└── 🔎 reviewer (串行)
    └── 审核文章质量，提出改进建议
```

### 示例 2: 代码项目
```
用户: "帮我重构这个项目的认证模块"

编排方案:
├── 📋 pm (可选)
│   └── 分析需求，拆解重构步骤
├── 👨‍💻 coder
│   └── 分析现有代码，实现重构
└── 🔎 reviewer (串行)
    └── 代码审查，确保质量
```

### 示例 3: 数据分析报告
```
用户: "分析这份销售数据，生成月度报告"

编排方案:
├── 📊 analyst
│   └── 数据清洗、统计分析、发现洞察
├── ✍️ writer (串行)
│   └── 撰写分析报告
└── 🎨 designer (串行)
    └── 生成数据可视化图表
```

### 示例 4: 自动化任务
```
用户: "帮我设置每天早上自动检查 GitHub trending"

编排方案:
├── 🤖 automator
│   └── 编写脚本 + 设置 cron 定时任务
```

## 编排原则

1. **简单任务不过度编排** — 能直接做的就直接做，不要为了用而用
2. **合理并行** — 无依赖的任务并行执行，提高效率
3. **明确交接** — 子任务输出要清晰完整，便于下游使用
4. **失败处理** — 某个子任务失败时，决定重试还是跳过
5. **结果整合** — 最终输出要连贯，不是简单拼接
6. **成本意识** — 优先用便宜模型，复杂任务才用贵模型

## 调用语法

```javascript
sessions_spawn({
  task: "具体任务描述，包含必要的上下文和期望的输出格式",
  agentId: "researcher",   // 指定 Agent ID
  model: "glm",            // 可选，覆盖 Agent 默认模型
  label: "task-name",      // 可选，便于追踪
  runTimeoutSeconds: 300   // 可选，超时时间（秒）
})
```

### Task 描述最佳实践

```markdown
好的 task 描述应包含：
1. 明确的目标 - 要做什么
2. 必要的上下文 - 背景信息
3. 输出要求 - 格式、保存位置
4. 约束条件 - 限制和注意事项

示例：
"搜索 LangChain 框架的最新资料，整理以下内容：
1. 核心功能和架构
2. 优点和缺点
3. 典型使用案例
4. 与其他框架的对比

输出格式：Markdown
保存到：/workspace/research/langchain.md
语言：中文"
```

## 任务完成统计

完成智能体团队协作任务后，**必须**输出统计信息：

```markdown
## 📊 智能体团队执行统计

### 执行明细
| 智能体 | 任务 | 耗时 | Tokens (in/out) | 状态 |
|--------|------|------|-----------------|------|
| 🔍 researcher | LangChain调研 | 2m30s | 8k/1.2k | ✅ |
| 🔍 researcher | AutoGPT调研 | 2m45s | 9k/1.0k | ✅ |
| ✍️ writer | 撰写报告 | 3m12s | 15k/2.5k | ✅ |
| 🎨 designer | 生成配图 | 45s | 2k/- | ✅ |

### 成本汇总
- **总耗时**: 9m12s（并行优化后实际: 6m30s）
- **总 Tokens**: 34k input / 4.7k output
- **实际成本**: $0.12
- **全用主模型成本**: $0.29
- **节省**: 59%

### 效率分析
- **并行任务数**: 2个 researcher 并行
- **串行节省**: 通过并行节省 ~2m45s
```

详细模板见 [references/statistics-template.md](references/statistics-template.md)

## 智能体工作目录

每个智能体有独立的工作目录，包含其人格配置：

```
/workspace/agents/
├── pm/           # 📋 产品经理
│   ├── SOUL.md   # 人格定义
│   └── AGENTS.md # 工作规范
├── researcher/   # 🔍 研究员
├── coder/        # 👨‍💻 程序员
├── writer/       # ✍️ 写作者
├── designer/     # 🎨 设计师
├── analyst/      # 📊 分析师
├── reviewer/     # 🔎 审核员
├── assistant/    # 💬 助手
└── automator/    # 🤖 自动化
```

## 智能体配置管理

使用 `agent_manager.py` 脚本管理智能体集群：

```bash
# 列出所有智能体
python3 scripts/agent_manager.py list

# 查看智能体详情
python3 scripts/agent_manager.py show researcher

# 添加新智能体（使用模板）
python3 scripts/agent_manager.py add my_agent --template researcher --name "我的智能体" --emoji "🚀"

# 删除智能体（默认会备份）
python3 scripts/agent_manager.py remove my_agent

# 更新智能体配置
python3 scripts/agent_manager.py update my_agent --name "新名称"
```

### 可用模板

| 模板 | 说明 | 默认模型 |
|------|------|----------|
| `default` | 通用智能体 | claude-opus-4 |
| `researcher` | 研究调研 | glm-4 |
| `coder` | 编程开发 | claude-opus-4 |
| `writer` | 内容写作 | gemini-2.5-pro |

## 智能体经验记忆

每个智能体可以积累任务经验，用于提升后续任务的执行质量。

### 经验记录结构

```
/workspace/agents/<agent_id>/
└── memory/
    ├── experience.md    # 人类可读的经验记录
    └── experience.json  # 结构化经验数据
```

### 使用 experience_logger.py

```bash
# 记录一条经验
python3 scripts/experience_logger.py log researcher "搜索技术资料时，英文关键词效果更好" --task "LangChain调研"

# 查看智能体经验
python3 scripts/experience_logger.py show researcher --limit 10

# 生成经验摘要
python3 scripts/experience_logger.py summary researcher

# 输出可注入 prompt 的经验（用于 spawn 时注入）
python3 scripts/experience_logger.py inject researcher --limit 5
```

### 在任务中使用经验

**方法 1: 在 task 描述中注入经验**

```python
# 获取历史经验
import subprocess
result = subprocess.run(
    ["python3", "scripts/experience_logger.py", "inject", "researcher", "--limit", "5"],
    capture_output=True, text=True
)
experiences = result.stdout

# 在 spawn 时注入
sessions_spawn({
    task: f"""搜索 xxx 资料...

{experiences}
""",
    agentId: "researcher"
})
```

**方法 2: 智能体主动读取经验**

在智能体的 AGENTS.md 中添加指引：
```markdown
## 任务前准备
执行任务前，先读取 memory/experience.md 中的历史经验。

## 任务后总结
完成任务后，总结 1-3 条有效经验，记录到 memory/experience.md。
```

### 经验记录最佳实践

✅ **好的经验记录**：
- 具体可操作："搜索 GitHub 时加 language:python 过滤更精准"
- 有因果关系："JSON 输出比纯文本更便于下游处理"
- 针对性强："处理大文件时分块读取，避免内存溢出"

❌ **避免的记录**：
- 太笼统："要认真工作"
- 太具体："用户 A 喜欢蓝色"（除非是个性化智能体）
- 重复已有的："要输出 Markdown 格式"（已在 AGENTS.md 中）

### 经验自动总结（推荐）

在每个智能体的 AGENTS.md 末尾添加：

```markdown
## 任务完成后

1. 检查输出是否符合要求
2. 总结本次任务中的有效经验（1-3 条）
3. 将经验追加到 memory/experience.md，格式：
   - [YYYY-MM-DD] 经验描述 (任务名称)
```

这样智能体在完成任务后会自动总结经验，无需手动干预。

## 配置与部署

如需配置新的智能体团队或添加新模型，请参阅 [references/setup-guide.md](references/setup-guide.md)

使用初始化脚本快速创建工作目录：
```bash
python3 scripts/init_agents.py --base-path /workspace/agents
```
