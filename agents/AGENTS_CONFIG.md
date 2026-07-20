# 多智能体项目团队配置

## 已创建的 Agents

| 序号 | Agent Label | Session Key | 角色 |
|------|-------------|-------------|------|
| 1 | pm_project | agent:main:subagent:b3ee6551-33ae-4d39-9aa8-23b8015a6f2f | 项目经理 |
| 2 | pm_product | agent:main:subagent:bf15b2b5-0c9e-4837-b428-fdb74b79f2b6 | 产品经理 |
| 3 | arch | agent:main:subagent:43bab61f-da58-4fc9-a65f-2e30ba3dcdf4 | 架构师 |
| 4 | frontend | agent:main:subagent:23c68213-7e33-4534-8146-371f7aae6d89 | 前端开发 |
| 5 | backend | agent:main:subagent:e2722274-ccda-4356-8857-91b43cab195b | 后端开发 |
| 6 | embedded | agent:main:subagent:d463c088-414e-415c-8fa1-50f24f995489 | 嵌入式开发 |
| 7 | code_reviewer | agent:main:subagent:d15ca67e-a20b-454c-98ce-fe733db975bb | 代码检视 |
| 8 | test_frontend | agent:main:subagent:f64d18a7-cdee-4cdc-9aef-586243ea5b52 | 前端测试 |
| 9 | test_backend | agent:main:subagent:8efdfc15-80ec-47d8-997b-f90a03a074d5 | 后端测试 |
| 10 | test_embedded | agent:main:subagent:4237d156-7013-45f4-8225-fb7e72201792 | 嵌入式测试 |
| 11 | repo_manager | agent:main:subagent:e562e3ec-4339-41ef-a032-80efa6df32d4 | 仓库管理 |

## 工作流

```
用户输入
    ↓
main agent (项目总指挥)
    ↓ sessions_send
pm_project (项目经理)
    ↓ sessions_send
pm_product (产品经理)
    ↓ sessions_send
arch (架构师)
    ↓ sessions_send
┌──────────┬──────────┬──────────┐
frontend   backend   embedded
(前端开发)  (后端开发)  (嵌入式开发)
    ↓          ↓          ↓
└──────────┼──────────┘
           ↓ sessions_send
    code_reviewer (代码检视)
           ↓ sessions_send
┌──────────┼──────────┐
↓          ↓          ↓
test_    test_     test_
frontend backend   embedded
(前端测试)(后端测试)(嵌入式测试)
    ↓          ↓          ↓
└──────────┼──────────┘
           ↓ sessions_send
    repo_manager (仓库管理)
           ↓ sessions_send
    pm_project (项目完成)
```

## 消息发送示例

### 启动项目
```python
sessions_send(
    label="pm_project",
    message="启动新项目：电商平台开发\n目标：构建一个支持Web和移动端的电商平台\n截止日期：2024-06-01"
)
```

### 任务分派
```python
sessions_send(
    label="pm_product",
    message="【任务】进行需求设计\n项目：电商平台\n需要输出：PRD文档、用户故事、功能清单\n优先级：高\n截止日期：2024-03-25"
)
```

### 代码检视反馈
```python
sessions_send(
    label="frontend",
    message="【代码检视结果】通过\n检视内容：登录页面组件\n备注：代码规范良好，可以进入测试阶段"
)
```

## 文件结构

```
C:\Users\Lenovo\.qclaw\workspace\agents\
├── pm_project/
│   └── SOUL.md          # 项目经理角色定义
├── pm_product/
│   └── SOUL.md          # 产品经理角色定义
├── arch/
│   └── SOUL.md          # 架构师角色定义
├── frontend/
│   └── SOUL.md          # 前端开发角色定义
├── backend/
│   └── SOUL.md          # 后端开发角色定义
├── embedded/
│   └── SOUL.md          # 嵌入式开发角色定义
├── code_reviewer/
│   └── SOUL.md          # 代码检视角色定义
├── test_frontend/
│   └── SOUL.md          # 前端测试角色定义
├── test_backend/
│   └── SOUL.md          # 后端测试角色定义
├── test_embedded/
│   └── SOUL.md          # 嵌入式测试角色定义
├── repo_manager/
│   └── SOUL.md          # 仓库管理角色定义
├── WORKFLOW.md          # 工作流文档
├── feishu-bindings.toml # 飞书通道绑定配置
└── AGENTS_CONFIG.md     # 本配置文件
```

## 飞书通道绑定

编辑 `feishu-bindings.toml` 文件，将群ID替换为实际的飞书群聊ID：

```toml
[[bindings]]
agent = "pm_project"
channel = "feishu"
target = "你的飞书群ID"
```

## 注意事项

1. 所有 agents 使用 `sessions_send` 进行消息传递
2. 每个 agent 都有独立的 session，可以并行工作
3. 消息流转遵循 WORKFLOW.md 中定义的流程
4. 可以通过 `sessions_list` 查看所有 agents 状态
5. 可以通过 `subagents list` 查看子 agents 列表
