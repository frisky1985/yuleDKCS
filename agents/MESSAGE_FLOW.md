# 项目团队 Agent 消息流转配置

## 架构概览

```
用户输入 → Main Agent (总指挥)
                ↓
         pm_project (项目经理) - 任务拆解与排期
                ↓
         pm_product (产品经理) - 需求设计与任务拆分
                ↓
            arch (架构师) - 架构分析与设计
                ↓
    ┌──────────┼──────────┐
    ↓          ↓          ↓
 frontend  backend   embedded
 (前端开发) (后端开发) (嵌入式开发)
    │          │          │
    └──────────┼──────────┘
               ↓
      code_reviewer (代码检视)
               ↓
    ┌──────────┼──────────┐
    ↓          ↓          ↓
test_frontend test_backend test_embedded
 (前端测试)  (后端测试)  (嵌入式测试)
    │          │          │
    └──────────┼──────────┘
               ↓ (测试全部通过)
       repo_manager (仓库管理)
               ↓
          最终交付
```

## Agent IDs

| Agent | ID |
|-------|-----|
| 项目经理 | pm_project |
| 产品经理 | pm_product |
| 架构师 | arch |
| 前端开发 | frontend |
| 后端开发 | backend |
| 嵌入式开发 | embedded |
| 代码检视 | code_reviewer |
| 前端测试 | test_frontend |
| 后端测试 | test_backend |
| 嵌入式测试 | test_embedded |
| 仓库管理 | repo_manager |

## 消息流转规则

### 1. Main Agent → pm_project
- **触发**: 用户输入项目需求
- **消息**: 项目需求描述 + 交付要求
- **操作**: 使用 sessions_send 发送任务到 pm_project

### 2. pm_project → pm_product
- **触发**: 收到 Main Agent 的任务
- **消息**: 任务列表 + 初步排期
- **操作**: 使用 sessions_send 发送任务拆解到 pm_product

### 3. pm_product → arch
- **触发**: 收到 pm_project 的任务拆解
- **消息**: 详细需求 + 任务列表
- **操作**: 使用 sessions_send 发送需求到 arch

### 4. arch → (frontend/backend/embedded)
- **触发**: 收到 pm_product 的需求
- **消息**: 架构设计方案 + 接口定义
- **操作**: 使用 sessions_send 并行发送到三个开发 agent

### 5. (frontend/backend/embedded) → code_reviewer
- **触发**: 完成代码开发
- **消息**: 代码文件路径 + 接口文档
- **操作**: 使用 sessions_send 发送到 code_reviewer

### 6. code_reviewer → (test_frontend/test_backend/test_embedded)
- **触发**: 代码检视通过
- **消息**: 检视结果 + 代码位置
- **操作**: 使用 sessions_send 发送到对应测试 agent

### 7. (test_*) → pm_project (失败) 或 repo_manager (成功)
- **触发**: 测试完成
- **消息**: 测试结果
- **操作**: 
  - 失败: 使用 sessions_send 发送失败消息到 pm_project 重新流转
  - 成功: 使用 sessions_send 发送到 repo_manager

### 8. repo_manager → Main Agent
- **触发**: 所有测试通过
- **消息**: 合并结果 + 仓库地址
- **操作**: 使用 sessions_send 发送完成报告到 Main Agent

## 文件锁定规则

为确保同一时间只有一个 agent 操作文件，使用文件锁机制：
- 锁文件路径: `C:\Users\Lenovo\.qclaw\workspace\project_lock.json`
- 锁格式: `{"locked_by": "<agent_id>", "file": "<file_path>", "timestamp": "<ISO时间>"}`
- 获取锁: 检查锁文件是否存在，不存在则创建
- 释放锁: 完成任务后删除锁文件
