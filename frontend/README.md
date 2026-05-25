# yuleDKCS 前端

数字钥匙连接系统 (Digital Key Connectivity System) 的 Web 管理仪表盘。

## 技术栈

| 类别       | 技术                                                                 |
| ---------- | -------------------------------------------------------------------- |
| 框架       | React 18 + TypeScript                                                |
| 构建工具   | Vite 5                                                               |
| UI 组件库  | MUI (Material-UI) 5 — 组件 / Icons / Emotion                        |
| 状态管理   | Zustand 4 (客户端状态) + TanStack Query 5 (服务端状态)               |
| 路由       | React Router 6                                                       |
| HTTP 客户端| Axios 1.7                                                            |
| 动画       | Framer Motion                                                        |
| 测试       | Vitest + React Testing Library                                       |
| 代码规范   | ESLint + TypeScript strict mode                                      |

## 目录结构

```
frontend/
├── public/                      # 静态资源
├── src/
│   ├── api/                     # API 客户端函数
│   │   ├── client.ts            # Axios 实例 + 拦截器
│   │   ├── auth.ts              # 认证相关 API
│   │   ├── keys.ts              # 数字钥匙 API + 类型定义
│   │   ├── transform.ts         # 后端响应数据转换层
│   │   └── vehicles.ts          # 车辆管理 API
│   ├── components/              # 可复用 UI 组件
│   │   └── ShareKeyDialog.tsx
│   ├── hooks/                   # 自定义 React Hooks
│   │   └── useApi.ts
│   ├── pages/                   # 页面组件 (路由对应)
│   │   ├── auth/
│   │   │   ├── LoginPage.tsx
│   │   │   ├── RegisterPage.tsx
│   │   │   └── ForgotPasswordPage.tsx
│   │   ├── DashboardPage.tsx    # 仪表盘
│   │   ├── Dashboard.tsx        # (备用入口)
│   │   ├── Login.tsx            # (备用入口)
│   │   ├── KeysPage.tsx         # 钥匙列表
│   │   ├── KeyDetailPage.tsx    # 钥匙详情
│   │   ├── KeyUsageLogsPage.tsx # 使用记录
│   │   ├── VehiclesPage.tsx     # 车辆列表
│   │   └── VehicleDetailPage.tsx# 车辆详情
│   ├── router/
│   │   └── index.tsx            # 路由配置
│   ├── services/                # WebSocket 等服务
│   │   ├── index.ts
│   │   ├── websocket.ts
│   │   └── websocket.types.ts
│   ├── store/                   # Zustand 状态仓库
│   │   └── auth.ts              # 认证状态 (持久化)
│   ├── test/                    # 测试文件
│   │   ├── setup.ts
│   │   └── auth.test.tsx
│   ├── types/                   # TypeScript 类型定义
│   │   └── index.ts
│   ├── App.tsx                  # 根组件
│   └── main.tsx                 # 入口文件
├── .env.example                 # 环境变量模板
├── .gitignore
├── AGENTS.md                    # AI Agent 开发指南
├── index.html
├── Makefile
├── package.json
├── tsconfig.json
├── tsconfig.node.json
├── vite.config.ts               # Vite 配置 (含 API 代理)
├── README.md                    # 本文件
└── API.md                       # API 接口文档
```

## 快速开始

### 前置要求

- Node.js 20+
- npm 或 pnpm

### 安装

```bash
cd frontend
npm install
```

### 环境变量

复制 `.env.example` 为 `.env` 并根据需要修改:

```bash
cp .env.example .env
```

| 变量           | 说明             | 默认值                           |
| -------------- | ---------------- | -------------------------------- |
| `VITE_API_URL` | 后端 API 基础 URL | `http://localhost:3000`          |

### 开发

```bash
npm run dev
```

启动开发服务器 (默认 http://localhost:5173)。Vite 已配置 API 代理，`/api` 请求会自动转发到后端。

### 构建

```bash
npm run build        # TypeScript 检查 + Vite 构建
npm run preview      # 预览构建结果
```

### 代码质量

```bash
npm run lint         # ESLint 检查
npm run lint:fix     # 自动修复
npm run type-check   # TypeScript 类型检查
npm run format       # Prettier 格式化
```

### 测试

```bash
npm run test              # 运行测试
npm run test:watch        # 监视模式
npm run test:coverage     # 覆盖率报告 (目标 ≥70%)
```

## 架构说明

### 数据流

```
用户操作 → React 组件 → TanStack Query → API 函数 (Axios) → 后端 API
                           ↓
                    Zustand (客户端状态)
                    React Query 缓存 (服务端状态)
```

### 状态管理策略

| 状态类型     | 工具               | 说明                                      |
| ------------ | ------------------ | ----------------------------------------- |
| 服务端状态   | TanStack Query     | 所有 API 数据 (车辆、钥匙、日志等)        |
| 客户端状态   | Zustand            | 认证信息 (持久化到 localStorage)          |
| 临时 UI 状态 | React `useState`   | 对话框打开/关闭、Tab 切换等               |

### API 调用模式

所有 API 调用通过 `src/api/client.ts` 中的 Axios 实例进行:

1. **请求拦截器** — 自动注入 JWT Token (`Authorization: Bearer <token>`)
2. **响应拦截器** — 401 响应时自动清除认证状态并跳转登录页
3. **统一响应格式** — 后端返回 `{ code, message, data }`，使用 `parseResponse()` 解析

### 路由保护

- `ProtectedRoute` — 未认证用户重定向到 `/login`
- `PublicRoute` — 已认证用户重定向到 `/`

## 代码规范

- 函数式组件 + Hooks，避免 class 组件
- 使用 `async/await` 而非回调
- 文件命名: 组件使用 PascalCase (`VehicleDetailPage.tsx`), 非组件使用 camelCase (`auth.ts`)
- API 模块使用 `api` 对象命名空间 (`keysApi.getMyKeys()`)
- 类型定义优先放在 `src/types/`，模块内部类型就近声明
