# Agent Father Skill 快速配置指南

## 📦 安装后第一步

### 1. 设置环境变量

```bash
# 添加到 ~/.bashrc 或 ~/.zshrc
export OPENCLAW_BASE=/path/to/your/.openclaw

# 立即生效
source ~/.bashrc  # 或 source ~/.zshrc
```

### 2. 配置飞书 API ⭐

**好消息：** 脚本会自动从 `openclaw.json` 读取飞书配置，无需手动设置！

#### 方法 1: 自动读取（推荐）

如果已配置飞书通道，脚本会自动读取：

```bash
# 检查 openclaw.json 中是否有飞书配置
cat $OPENCLAW_BASE/openclaw.json | grep -A 5 '"feishu"'

# 应该看到：
# "feishu": {
#   "enabled": true,
#   "appId": "cli_xxx",
#   "appSecret": "xxx",
#   ...
# }
```

#### 方法 2: 环境变量覆盖（可选）

如果需要临时使用不同的配置：

```bash
export FEISHU_APP_ID="cli_your_app_id"
export FEISHU_APP_SECRET="your_app_secret"
```

---

## 🚀 使用示例

### 测试配置

```bash
# 测试创建群组（验证配置是否正确）
./scripts/create-feishu-chat.sh --name "测试群" --description "测试配置"

# 成功输出：
# ✅ 已从 openclaw.json 读取飞书配置
#    APP_ID: cli_xxx
#    APP_SECRET: [已隐藏]
# 正在创建群聊：测试群...
# ✅ 群聊创建成功!
# 群 ID: oc_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
# 群名称：测试群
```

### 创建第一个员工

```bash
# 完整流程（简化版 4 步）
./scripts/create-employee.sh "客服工程师" "CS-001" "13800138000" "客服工作群"
```

### 输出示例

```
🚀 开始创建员工：客服工程师 (CS-001)
========================================
   OpenClaw 基础目录：/home/meichuan/.openclaw
   工作区目录：/home/meichuan/.openclaw/workspace

📋 步骤 1: 创建飞书群组
   ✅ 飞书群组已创建：oc_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

📋 步骤 2: 创建 Agent 配置
🏗️ 创建 Agent: 客服工程师 (CS-001)
   ✅ agent.json 已创建
   ✅ IDENTITY.md 已创建
   ✅ SOUL.md 已创建

📋 步骤 3: 更新员工名单
   ✅ 员工名单已更新

📋 步骤 4: 岗前培训
🎓 开始岗前培训：客服工程师 (CS-001)
   ✅ 培训大纲已创建
   ✅ 任务清单已创建
   ✅ 创建 Inbox 使用指南
   ✅ 创建岗前培训指南

========================================
✅ 员工创建流程完成！

📊 信息汇总：
   姓名：客服工程师
   工号：CS-001
   联系方式：13800138000
   会话 ID: session_cs-001_1234567890
   群 ID: oc_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

⚠️ 待完成事项：
   1. 更新 openclaw.json 配置（添加 agent、binding、groups）
   2. 发送入职通知到群组
   3. 跟踪培训进度
```

---

## ❓ 常见问题

### Q: 脚本报错 "未在 openclaw.json 中找到飞书配置"？

**A:** 说明 openclaw.json 中没有飞书配置，有两种解决方案：

**方案 1: 添加飞书配置到 openclaw.json**
```json
{
  "channels": {
    "feishu": {
      "enabled": true,
      "appId": "cli_your_app_id",
      "appSecret": "your_app_secret"
    }
  }
}
```

**方案 2: 使用环境变量**
```bash
export FEISHU_APP_ID="cli_your_app_id"
export FEISHU_APP_SECRET="your_app_secret"
```

### Q: 没有飞书应用怎么办？

**A:** 可以跳过群组创建步骤，手动创建群组：

```bash
# 1. 手动在飞书中创建群组
# 2. 获取群 ID（从群组 URL 或 openclaw 日志）
# 3. 使用 create-agent.sh 并指定群 ID
./scripts/create-agent.sh "客服工程师" "CS-001" "13800138000" "oc_xxx"
```

### Q: 如何验证配置是否正确？

```bash
# 1. 检查环境变量
echo $OPENCLAW_BASE

# 2. 检查 openclaw.json 配置
cat $OPENCLAW_BASE/openclaw.json | grep -A 5 '"feishu"'

# 3. 测试创建群组
./scripts/create-feishu-chat.sh --name "测试"
```

---

## 📋 检查清单

使用前请确认：

- [ ] 已设置 `OPENCLAW_BASE` 环境变量
- [ ] openclaw.json 中有飞书配置（appId 和 appSecret）
- [ ] 或已设置 FEISHU_APP_ID 和 FEISHU_APP_SECRET 环境变量
- [ ] 已测试 `create-feishu-chat.sh`

---

_配置完成后，就可以开始创建员工了！_
