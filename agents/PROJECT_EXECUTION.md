# 项目执行示例

## 概述

本示例演示如何通过飞书触发项目团队执行完整的工作流程。

## 流程说明

```
用户(飞书) → Main Agent → pm_project → pm_product → arch 
                                              ↓
                                    ┌─────────┼─────────┐
                                    ↓         ↓         ↓
                                frontend  backend  embedded
                                    ↓         ↓         ↓
                                    └─────────┼─────────┘
                                              ↓
                                       code_reviewer
                                              ↓
                                    ┌─────────┼─────────┐
                                    ↓         ↓         ↓
                                test_fe   test_be   test_ee
                                    └─────────┼─────────┘
                                              ↓
                                         repo_manager
                                              ↓
                                         Main Agent
```

## 如何启动项目

### 方式一：通过飞书发送任务

在飞书向 QClaw 发送项目需求：

```
开始新项目：开发一个电商网站的移动端H5页面
- 需要支持商品浏览、购物车、订单管理
- 需要后端提供 REST API
- 需要支持嵌入式设备扫码功能
```

### 方式二：手动触发

使用 `/project start` 命令启动项目

## 消息流转示例

### Step 1: Main Agent 接收任务

```
【用户需求】
开发一个电商网站的移动端H5页面

【需求详情】
- 商品浏览
- 购物车功能
- 订单管理
- 扫码功能（嵌入式）

【发送者】
user (via feishu)
【接收者】
main
```

### Step 2: Main → pm_project

```javascript
sessions_send({
  sessionKey: "pm_project",
  message: `
【项目启动】
用户需要在飞书发起一个新项目。

项目需求：
- 开发一个电商网站的移动端H5页面
- 需要支持商品浏览、购物车、订单管理
- 需要后端提供 REST API
- 需要支持嵌入式设备扫码功能

请执行：
1. 分析需求
2. 拆解任务
3. 制定排期计划
4. 将任务发送给 pm_product

【发送者】
main
【接收者】
pm_project
  `
})
```

### Step 3: pm_project → pm_product

```javascript
sessions_send({
  sessionKey: "pm_product",
  message: `
【任务拆解完成】
任务列表：
1. 前端页面开发 - 前端工程师 - 3天
2. 后端API开发 - 后端工程师 - 4天
3. 嵌入式扫码功能 - 嵌入式工程师 - 2天
4. 集成测试 - 测试工程师 - 2天

排期计划：
- 阶段1 (Day 1-2): 需求分析与设计
- 阶段2 (Day 3-6): 开发阶段
- 阶段3 (Day 7-8): 测试阶段
- 阶段4 (Day 9): 部署上线

请执行：
1. 详细需求分析
2. 设计产品功能
3. 定义接口规范
4. 发送给 arch 进行架构设计

【发送者】
pm_project
【接收者】
pm_product
  `
})
```

### Step 4: pm_product → arch

```javascript
sessions_send({
  sessionKey: "arch",
  message: `
【需求设计完成】
## 功能模块
1. 商品模块：浏览、搜索、详情
2. 购物车模块：添加、删除、修改数量
3. 订单模块：创建、支付、查询

## 技术任务
### 前端
1. 首页开发 - 高优先级
2. 商品列表页 - 高优先级
3. 购物车页面 - 高优先级
4. 订单页面 - 中优先级

### 后端
1. 用户认证 API - 高优先级
2. 商品管理 API - 高优先级
3. 订单管理 API - 中优先级

### 嵌入式
1. 扫码功能 - 高优先级
2. 设备通信 - 中优先级

## 接口规范
- POST /api/auth/login
- GET /api/products
- POST /api/cart
- POST /api/orders

请执行架构设计：
1. 系统架构设计
2. 技术选型
3. 模块划分
4. 接口详细定义
5. 并行发送给 frontend、backend、embedded

【发送者】
pm_product
【接收者】
arch
  `
})
```

### Step 5: arch → (frontend/backend/embedded)

```javascript
// 并行发送给三个开发 agent
sessions_send({
  sessionKey: "frontend",
  message: `
【架构设计完成 - 前端任务】
## 系统架构
- 框架：Vue 3 + Vite
- 状态管理：Pinia
- UI库：Vant

## 模块设计
### 首页模块
- 职责：商品展示
- 依赖：无
- 接口：GET /api/products

### 购物车模块
- 职责：购物车管理
- 依赖：用户认证
- 接口：POST /api/cart

## 接口规范
请按照以下规范开发：

### GET /api/products
响应：
{
  "code": 0,
  "data": {
    "list": [...],
    "total": 100
  }
}

请开始开发，完成后发送给 code_reviewer

【发送者】
arch
【接收者】
frontend
  `
})

// 类似发送给 backend 和 embedded
```

### Step 6: 开发 agents → code_reviewer

```javascript
sessions_send({
  sessionKey: "code_reviewer",
  message: `
【前端开发完成】
## 代码文件
- src/views/Home.vue - 首页
- src/views/ProductList.vue - 商品列表
- src/views/Cart.vue - 购物车

## 接口对接情况
| 接口 | 状态 | 备注 |
|------|------|------|
| GET /api/products | ✅ | 已完成 |
| POST /api/cart | ✅ | 已完成 |

请进行代码检视

【发送者】
frontend
【接收者】
code_reviewer
  `
})
```

### Step 7: code_reviewer → test_*

```javascript
sessions_send({
  sessionKey: "test_frontend",
  message: `
【代码检视通过 - 前端测试】
## 检视结果
- 代码质量：良好
- 规范性：通过
- 建议：无

## 测试要求
1. 接口测试
2. UI功能测试
3. 兼容性测试

请执行测试，完成后：
- 成功 → 发送给 repo_manager
- 失败 → 发送给 pm_project（打回）

【发送者】
code_reviewer
【接收者】
test_frontend
  `
})
```

### Step 8: 测试通过 → repo_manager

```javascript
sessions_send({
  sessionKey: "repo_manager",
  message: `
【前端测试通过】
## 测试结果
| 测试项 | 状态 | 备注 |
|--------|------|------|
| 接口测试 | ✅ | 通过 |
| UI测试 | ✅ | 通过 |
| 兼容性测试 | ✅ | Chrome/Firefox/Safari |

请合并代码到主分支

【发送者】
test_frontend
【接收者】
repo_manager
  `
})
```

### Step 9: repo_manager → main

```javascript
sessions_send({
  sessionKey: "main",
  message: `
【项目完成报告】
## 合并状态
| 模块 | 状态 | 分支 | 提交 |
|------|------|------|------|
| 前端 | ✅ | main | abc123 |
| 后端 | ✅ | main | def456 |
| 嵌入式 | ✅ | main | ghi789 |

## 版本信息
- 当前版本: v1.0.0
- 发布标签: release/v1.0.0

## 仓库地址
https://github.com/your-org/ecommerce-mobile

## 变更日志
- 新增商品浏览功能
- 新增购物车功能
- 新增订单管理功能
- 新增扫码功能

项目已全部完成！

【发送者】
repo_manager
【接收者】
main
  `
})
```

## 文件锁定机制

为确保同一时间只有一个 agent 操作文件，使用文件锁：

```javascript
// 获取锁
const lockFile = "C:\\Users\\Lenovo\\.qclaw\\workspace\\project_lock.json";
const lockData = {
  locked_by: "frontend",
  file: "src/views/Cart.vue",
  timestamp: new Date().toISOString()
};
// 检查锁文件是否存在
// 如果不存在，创建锁文件
// 如果存在且不是自己，等待或跳过

// 释放锁
// 删除锁文件
```

## Agent Session Keys

| Agent | Session Key |
|-------|-------------|
| pm_project | pm_project |
| pm_product | pm_product |
| arch | arch |
| frontend | frontend |
| backend | backend |
| embedded | embedded |
| code_reviewer | code_reviewer |
| test_frontend | test_frontend |
| test_backend | test_backend |
| test_embedded | test_embedded |
| repo_manager | repo_manager |
