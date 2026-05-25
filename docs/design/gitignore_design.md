# .gitignore 设计说明
## yuleDKCS 项目 — 2026-05-25

### 目标
防止编译产物、依赖目录、凭证文件、IDE配置被误提交到Git仓库。

### 覆盖范围

| 类别 | 忽略项 | 说明 |
|------|--------|------|
| **编译产物** | `*.o`, `*.a`, `*.out` | C/C++ 编译中间文件 |
| **构建目录** | `build/` | CMake 构建输出 |
| **Go** | `vendor/`, `bin/` | Go 依赖和二进制 |
| **Node** | `node_modules/`, `dist/` | JS 依赖和构建 |
| **Python** | `__pycache__/`, `*.pyc` | Python 缓存 |
| **IDE** | `.vscode/`, `.idea/` | 编辑器配置 |
| **安全** | `.env`, `*.pem`, `*.key` | 凭证和密钥文件 |
| **OS** | `.DS_Store`, `Thumbs.db` | 系统文件 |
