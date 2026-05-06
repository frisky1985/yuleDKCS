# 多智能体项目团队工作流

## 团队架构

```
用户输入
    ↓
main agent (项目总指挥)
    ↓ 通知任务下发
pm_project (项目经理)
    ↓ 拆解任务排期
pm_product (产品经理)
    ↓ 需求设计，拆解task
arch (架构师)
    ↓ 架构分析，接口设计
┌──────────┬──────────┬──────────┐
frontend   backend   embedded
(前端开发)  (后端开发)  (嵌入式开发)
    ↓          ↓          ↓
└──────────┼──────────┘
           ↓
    code_reviewer (代码检视)
           ↓
┌──────────┼──────────┐
↓          ↓          ↓
test_    test_     test_
frontend backend   embedded
(前端测试)(后端测试)(嵌入式测试)
    ↓          ↓          ↓
└──────────┼──────────┘
           ↓
    repo_manager (仓库管理)
           ↓
    pm_project (项目完成)
```

## Agents 列表

| Agent ID | 角色 | 职责 |
|----------|------|------|
| pm_project | 项目经理 | 任务拆解、排期管理、进度跟踪 |
| pm_product | 产品经理 | 需求分析、PRD编写、任务拆解 |
| arch | 架构师 | 架构设计、技术选型、接口定义 |
| frontend | 前端开发 | UI实现、组件开发、接口对接 |
| backend | 后端开发 | API开发、业务逻辑、数据库 |
| embedded | 嵌入式开发 | 固件开发、设备通信、硬件接口 |
| code_reviewer | 代码检视 | 代码审查、安全审计、质量评估 |
| test_frontend | 前端测试 | 功能测试、接口测试、兼容性测试 |
| test_backend | 后端测试 | API测试、集成测试、性能测试 |
| test_embedded | 嵌入式测试 | 固件测试、通信测试、稳定性测试 |
| repo_manager | 仓库管理 | 代码合并、版本管理、发布流程 |

## 消息流转协议

### 1. 启动项目
```
main → pm_project: 启动项目，下发项目目标
pm_project → pm_product: 进行需求设计
```

### 2. 需求设计
```
pm_product → arch: 提交PRD和需求文档
```

### 3. 架构设计
```
arch → frontend: 前端架构设计和接口规范
arch → backend: 后端架构设计和数据库设计
arch → embedded: 嵌入式架构和通信协议
```

### 4. 代码开发
```
frontend → code_reviewer: 提交前端代码
backend → code_reviewer: 提交后端代码
embedded → code_reviewer: 提交嵌入式代码
```

### 5. 代码检视
```
code_reviewer → test_frontend: 前端代码通过检视
code_reviewer → test_backend: 后端代码通过检视
code_reviewer → test_embedded: 嵌入式代码通过检视
```

### 6. 测试验证
```
test_frontend → repo_manager: 前端测试通过
test_backend → repo_manager: 后端测试通过
test_embedded → repo_manager: 嵌入式测试通过
```

### 7. 发布完成
```
repo_manager → pm_project: 项目发布完成
pm_project → main: 项目总结汇报
```

## 消息格式

```json
{
  "type": "task_assign|task_complete|review_request|review_result|test_result|release_notice",
  "from": "agent_id",
  "to": "agent_id",
  "task_id": "uuid",
  "content": {
    "title": "任务标题",
    "description": "任务描述",
    "deliverables": ["交付物1", "交付物2"],
    "deadline": "2024-01-01",
    "priority": "high|medium|low"
  },
  "attachments": ["文件路径或URL"]
}
```

## 使用 sessions_send 的示例

```python
# main agent 启动项目
sessions_send(
    label="pm_project",
    message="启动新项目：电商平台开发\n目标：构建一个支持Web和移动端的电商平台\n截止日期：2024-06-01"
)

# pm_project 通知 pm_product
sessions_send(
    label="pm_product",
    message="任务：进行需求设计\n项目：电商平台\n需要输出：PRD文档、用户故事、功能清单"
)

# 代码检视反馈
sessions_send(
    label="frontend",
    message="代码检视结果：通过\n检视内容：登录页面组件\n备注：代码规范良好，可以进入测试阶段"
)
```
