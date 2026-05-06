# Draw.io Skill 安装与使用指南

## 📦 安装摘要

| 项目 | 状态 | 说明 |
|------|------|------|
| **drawio-skills** | ✅ 已安装 | OpenClaw Skill，版本 2.2.0 |
| **Skill 路径** | `C:\Users\Lenovo\.agents\skills\drawio\` | |
| **draw.io Desktop** | ⚠️ 需验证 | Microsoft Store 版本 v29.6.6.0 |
| **App Execution Alias** | ❌ 未配置 | `draw.io` 命令不可用 |

## 🔧 Draw.io Skill 功能

### 核心能力

1. **Create (创建)** - 从自然语言/YAML/Mermaid/CSV 创建图表
2. **Edit (编辑)** - 修改现有图表
3. **Replicate (复刻)** - 根据上传图片重绘图表

### 支持的图表类型

- 流程图 (Flowchart)
- 系统架构图 (System Architecture)
- 网络拓扑图 (Network Topology)
- UML 图 (UML Diagrams)
- ER 图 (Entity Relationship)
- 时序图 (Sequence Diagram)
- 状态机 (State Machine)
- 组织结构图 (Org Chart)
- 思维导图 (Mind Map)
- 云架构图 (Cloud Infrastructure - AWS/GCP/Azure)
- 学术论文图 (Academic Paper Figures)
- 技术路线图 (Technical Roadmap)

### 内置主题 (6个)

- `tech-blue` - 科技蓝（默认）
- `academic` - 学术风格（黑白）
- `academic-color` - 学术彩色
- `nature` - 自然风格
- `dark` - 深色模式
- `high-contrast` - 高对比度

## 🚀 快速开始

### 1. 创建图表（最简单方式）

直接告诉我要画什么图：

```
画一个用户登录流程图，包含：输入用户名密码 → 验证 → 成功/失败分支
```

或者：

```
/drawio create 生成一个横向 tech-blue 登录流程图，共 6 个节点
```

### 2. 创建网络拓扑图

```
生成一个 tech-blue 网络拓扑图，包含防火墙、核心交换机、两台应用服务器和私有数据库子网
```

### 3. 创建系统架构图

```
画一个微服务架构图，包含：
- API Gateway
- User Service
- Order Service
- Payment Service
- Message Queue
- PostgreSQL Database
- Redis Cache
```

### 4. 复刻图片

```
/drawio replicate
[上传截图或图片]
颜色模式：preserve-original
```

## 📝 YAML Spec 格式（高级用法）

如果你想精确控制图表细节，可以使用 YAML spec：

```yaml
meta:
  title: "用户登录流程"
  theme: tech-blue
  layout: horizontal

nodes:
  - id: start
    label: "开始"
    shape: ellipse
    
  - id: input
    label: "输入用户名密码"
    shape: rectangle
    
  - id: validate
    label: "验证用户"
    shape: diamond
    
  - id: success
    label: "登录成功"
    shape: rectangle
    
  - id: fail
    label: "登录失败"
    shape: rectangle
    
  - id: end
    label: "结束"
    shape: ellipse

edges:
  - from: start
    to: input
    
  - from: input
    to: validate
    
  - from: validate
    to: success
    label: "是"
    
  - from: validate
    to: fail
    label: "否"
    
  - from: success
    to: end
    
  - from: fail
    to: input
    label: "重试"
```

## 🔧 Draw.io Desktop 配置（可选）

draw.io Desktop 用于：
- 导出 PNG/PDF/JPG 格式
- 导出嵌入式 SVG（带 draw.io 元数据）
- 高质量正交路由 SVG

### 安装方式

#### 方法 1：Microsoft Store（已安装）

```
winget install 9MVVSZK43QQW --source msstore
```

当前状态：已通过 Microsoft Store 安装 v29.6.6.0，但 App Execution Alias 未配置。

**需要手动配置**：
1. 打开 Windows 设置 → 应用 → 应用执行别名
2. 确保 "draw.io" 已启用
3. 或从开始菜单直接启动 draw.io

#### 方法 2：下载安装包（推荐）

从 GitHub 下载桌面版：
```powershell
# 下载
$url = "https://github.com/jgraph/drawio-desktop/releases/download/v29.6.6/draw.io-29.6.6-windows-installer.exe"
Invoke-WebRequest -Uri $url -OutFile "$env:TEMP\drawio-installer.exe"

# 安装
Start-Process "$env:TEMP\drawio-installer.exe" -Wait
```

安装后，`draw.io` 命令将可用。

### 验证 draw.io Desktop

```powershell
draw.io --version
```

如果命令不可用，skill 仍然可以工作，只是：
- ✅ 可以生成 `.drawio` 文件（可用 draw.io 打开编辑）
- ✅ 可以生成基础 SVG（预览质量）
- ❌ 不能直接导出 PNG/PDF/JPG
- ❌ 不能生成带正交路由的高质量 SVG

## 📂 输出文件说明

### 标准产物三件套（推荐）

当图表后续需要继续编辑时，保持这三个文件一起：

1. **`<name>.drawio`** - draw.io 原生格式，可继续编辑
2. **`<name>.spec.yaml`** - YAML 规格文件，skill 使用的源
3. **`<name>.arch.json`** - 架构元数据

### 导出格式

- `.drawio` - ✅ 始终可用
- `.svg` - ✅ 始终可用（预览质量）
- `.svg` (embedded) - ⚠️ 需要 draw.io Desktop
- `.png` - ⚠️ 需要 draw.io Desktop
- `.pdf` - ⚠️ 需要 draw.io Desktop
- `.jpg` - ⚠️ 需要 draw.io Desktop

## 📚 参考示例

Skill 内置了多个示例 YAML 文件：

| 示例文件 | 说明 |
|---------|------|
| `login-flow.yaml` | 登录流程图 |
| `microservices.yaml` | 微服务架构图 |
| `neural-network.yaml` | 神经网络图 |
| `campus-lan-topology.yaml` | 校园网络拓扑 |
| `aws-vpc-topology.yaml` | AWS VPC 架构 |
| `onprem-dmz-topology.yaml` | DMZ 网络架构 |
| `system-architecture-paper.yaml` | 论文级系统架构图 |
| `research-pipeline.yaml` | 研究流程图 |
| `technical-roadmap-paper.yaml` | 技术路线图 |

## 🎨 学术论文图表

对于论文、IEEE、期刊投稿，使用学术模式：

```
/drawio create 学术论文级系统架构图，IEEE 风格，tech-blue 主题
```

学术模式会：
1. 自动归类图表类型（architecture/roadmap/workflow）
2. 应用 IEEE 网络图规范
3. 检查标题、图例、字体大小
4. 导出矢量 SVG（适合论文）

## 🔍 常用命令

### CLI 验证和导出

```bash
# 验证 YAML spec
node <skill-dir>/scripts/cli.js input.yaml output.drawio --validate --write-sidecars

# 导出 SVG
node <skill-dir>/scripts/cli.js input.yaml output.svg --validate --write-sidecars

# 使用 draw.io Desktop 导出高质量 SVG
node <skill-dir>/scripts/cli.js input.yaml output.svg --validate --use-desktop

# 导出 PNG（需要 Desktop）
node <skill-dir>/scripts/cli.js input.yaml output.png --validate --use-desktop

# 导入已有的 .drawio 文件转成 YAML bundle
node <skill-dir>/scripts/cli.js existing.drawio --input-format drawio --export-spec --write-sidecars
```

### 严格模式（论文级质量）

```bash
# 开启严格校验
node <skill-dir>/scripts/cli.js input.yaml output.svg --validate --strict --write-sidecars
```

## 💡 使用技巧

1. **简单图表** - 直接用自然语言描述即可
2. **复杂图表** - 使用 YAML spec 精确控制
3. **复刻图片** - 上传截图，指定颜色模式
4. **学术论文** - 使用 `academic-paper` profile
5. **云架构** - 自动识别 AWS/GCP/Azure 图标
6. **含公式** - 支持 LaTeX、MathJax 格式

## 📖 更多文档

- [在线文档](https://bahayonghang.github.io/drawio-skills/zh/)
- [工作流概览](https://bahayonghang.github.io/drawio-skills/zh/guide/workflows)
- [CLI 工具](https://bahayonghang.github.io/drawio-skills/zh/guide/cli)
- [设计系统](https://bahayonghang.github.io/drawio-skills/zh/guide/design-system)

## ⚠️ 注意事项

1. **离线优先** - Skill 默认在本地生成，不需要网络
2. **draw.io Desktop 可选** - 基础功能不需要，PNG/PDF 导出需要
3. **Mermaid/CSV 是输入** - 会被转换成 YAML spec，不是最终格式
4. **sidecar 文件重要** - 保持 `.spec.yaml` 和 `.arch.json` 与 `.drawio` 一起
5. **SVG 默认预览质量** - 需要高质量请使用 `--use-desktop`

---

**下一步**：直接告诉我你想画什么图即可开始！例如：
- "画一个微服务架构图"
- "创建一个登录流程图"
- "生成一个 AWS VPC 网络拓扑"
