#!/bin/bash
# 新员工岗前培训脚本（可移植版本）
# 用法：./onboarding.sh <agent-name> <role>

set -e

# ========== 配置检测 ==========

OPENCLAW_BASE="${OPENCLAW_BASE:-}"

if [ -z "$OPENCLAW_BASE" ] && command -v openclaw &> /dev/null; then
    OPENCLAW_BASE="$(openclaw config get baseDir 2>/dev/null || echo "")"
fi

if [ -z "$OPENCLAW_BASE" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    OPENCLAW_BASE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
fi

if [ -z "$OPENCLAW_BASE" ]; then
    OPENCLAW_BASE="$HOME/.openclaw"
fi

WORKSPACE_DIR="$OPENCLAW_BASE/workspace"
if [ ! -d "$WORKSPACE_DIR" ] && [ -d "$OPENCLAW_BASE/workspace-dev" ]; then
    WORKSPACE_DIR="$OPENCLAW_BASE/workspace-dev"
fi

# ========== 参数解析 ==========

AGENT_NAME="$1"
ROLE="$2"

if [ -z "$AGENT_NAME" ] || [ -z "$ROLE" ]; then
    echo "❌ 用法：$0 <agent-name> <role>"
    echo "示例：$0 '客服工程师' 'CS-001'"
    exit 1
fi

# ========== Agent ID 生成 ==========

AGENT_ID=$(echo "$ROLE" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd 'a-z0-9-')
WORKSPACE_AGENT_NAME=$(echo "$AGENT_NAME" | tr -d '/*?"<>|:')
AGENT_DIR="$WORKSPACE_DIR/agents/$WORKSPACE_AGENT_NAME"

# 确保目录存在
mkdir -p "$AGENT_DIR"

echo "🎓 开始岗前培训：$AGENT_NAME ($ROLE)"
echo "   工作区：$WORKSPACE_DIR"
echo "   培训目录：$AGENT_DIR"
echo ""

# ========== 创建培训大纲 ==========

cat > "$AGENT_DIR/ONBOARDING.md" << EOF
# 岗前培训大纲 - ${AGENT_NAME}

**工号：** ${ROLE}  
**培训日期：** $(date +%Y-%m-%d)  
**培训状态：** ⏳ 进行中

---

## 📚 培训内容

### 1. 公司文化
- [ ] 了解公司使命和价值观
- [ ] 熟悉团队协作方式
- [ ] 学习沟通规范

### 2. 岗位职责
- [ ] 明确岗位工作内容
- [ ] 了解工作流程
- [ ] 掌握工作标准
- [ ] 阅读 IDENTITY.md 和 SOUL.md

### 3. 工具使用
- [ ] OpenClaw 框架基础
- [ ] 消息收发机制
- [ ] 工具调用方法
- [ ] 文件操作规范

### 4. 技能培训 ⭐
- [ ] 学习相关 Skill 使用
- [ ] 掌握专业技能
- [ ] 实践操作练习

### 5. 通知机制学习 ⭐⭐⭐
- [ ] 学习 INBOX-GUIDE.md
- [ ] 了解 Inbox 使用方法
- [ ] 练习发送消息给同事
- [ ] 完成实践练习

### 6. 实战演练
- [ ] 模拟任务处理
- [ ] 问题解决训练
- [ ] 团队协作练习

---

## ✅ 培训检查清单

| 项目 | 状态 | 备注 |
|------|------|------|
| 身份文件创建 | ⬜ | IDENTITY.md |
| SOUL 文件创建 | ⬜ | SOUL.md |
| agent.json 配置 | ⬜ | 包含会话 ID、群 ID、联系方式 |
| 工具权限配置 | ⬜ | openclaw.json |
| 群组加入 | ⬜ | 相关群组 |
| 导师分配 | ⬜ | 指定导师 |
| **通知机制学习** | ⬜ | **INBOX-GUIDE.md** ⭐ |
| 通知系统部署 | ⬜ | heartbeat-check.sh + crontab |

---

## 📝 培训总结

**培训完成度：** 0%  
**预计完成时间：** 7 天  
**导师评价：** 待填写

---

_祝工作顺利，快速成长！_
EOF

echo "✅ 培训大纲已创建：$AGENT_DIR/ONBOARDING.md"

# ========== 创建培训任务清单 ==========

cat > "$AGENT_DIR/TASKS.md" << EOF
# 培训任务清单 - ${AGENT_NAME}

## 第一阶段：基础认知 (Day 1)

- [ ] 阅读 IDENTITY.md 了解身份职责
- [ ] 阅读 SOUL.md 理解工作原则
- [ ] 查看 agent.json 了解配置信息
- [ ] 熟悉工作区目录结构
- [ ] 学习基本工具使用

## 第二阶段：技能学习 (Day 2-3)

- [ ] 学习相关 Skill 文档
- [ ] 练习工具调用
- [ ] 完成示例任务

## 第三阶段：通知机制学习 (Day 4) ⭐

- [ ] 阅读 INBOX-GUIDE.md
- [ ] 了解 Inbox 路径：$WORKSPACE_DIR/shared/inbox/
- [ ] 学习如何发送消息给同事
- [ ] 实践：给 HR 发送培训进度通知

## 第四阶段：实战演练 (Day 5-6)

- [ ] 处理真实任务
- [ ] 参与团队协作
- [ ] 接受导师指导

## 第五阶段：独立上岗 (Day 7)

- [ ] 独立完成工作任务
- [ ] 通过上岗考核
- [ ] 正式加入团队

---

**当前阶段：** 第一阶段  
**完成进度：** 0/5
EOF

echo "✅ 任务清单已创建：$AGENT_DIR/TASKS.md"

# ========== 复制培训指南 ==========

SHARED_DIR="$WORKSPACE_DIR/shared"
mkdir -p "$SHARED_DIR"

# 如果 INBOX-GUIDE.md 不存在，创建简化版本
if [ ! -f "$SHARED_DIR/INBOX-GUIDE.md" ]; then
    cat > "$SHARED_DIR/INBOX-GUIDE.md" << 'INBOXGUIDE'
# Inbox 使用指南

## Inbox 路径

**你的 Inbox 位置：** `<WORKSPACE>/shared/inbox/dev-inbox.md`

## 通知格式

```markdown
## [通知 ID] 📢 [优先级] 时间
**状态：** ⏳ 待处理

**内容：**
通知内容...

---
```

## 发送消息给同事

### 方法 1: 追加到 Inbox 文件

```bash
# 追加通知到 Inbox
cat >> <WORKSPACE>/shared/inbox/dev-inbox.md << EOF

## [通知 ID] 📢 [normal] 时间
**状态：** ⏳ 待处理

**内容：**
给同事的消息内容

---
EOF
```

### 方法 2: 使用 openclaw agent

```bash
openclaw agent --to "群 ID" --message "消息内容"
```

## 自动处理机制

- 每 2 分钟自动检查 Inbox
- 待处理通知会被自动处理
- 处理后状态更新为"已处理 ✅"

## 实践练习

1. 查看你的 Inbox 文件
2. 给 HR 发送一条培训完成通知
3. 确认通知被处理

---

_掌握通知机制，高效协作沟通！_
INBOXGUIDE
    echo "📝 创建 Inbox 使用指南：$SHARED_DIR/INBOX-GUIDE.md"
fi

# 如果岗前培训指南不存在，创建简化版本
if [ ! -f "$SHARED_DIR/岗前培训指南.md" ]; then
    cat > "$SHARED_DIR/岗前培训指南.md" << 'TRAININGGUIDE'
# 岗前培训指南

欢迎加入团队！本指南帮助你快速上岗。

## 培训流程

### Day 1: 基础认知
- 阅读 IDENTITY.md 和 SOUL.md
- 了解岗位职责
- 熟悉工作环境

### Day 2-3: 技能学习
- 学习相关 Skill
- 练习工具使用
- 完成示例任务

### Day 4: 通知机制 ⭐
- 学习 INBOX-GUIDE.md
- 掌握消息发送方法
- 完成实践练习

### Day 5-6: 实战演练
- 处理真实任务
- 参与团队协作

### Day 7: 独立上岗
- 通过考核
- 正式加入团队

## 重要文档

- **IDENTITY.md** - 你的身份和职责
- **SOUL.md** - 工作原则和行动准则
- **INBOX-GUIDE.md** - 通知机制使用指南
- **agent.json** - 你的配置信息（会话 ID、群 ID、联系方式）

## 联系方式

遇到问题请及时与导师沟通！

---

_祝工作顺利！_
TRAININGGUIDE
    echo "📝 创建岗前培训指南：$SHARED_DIR/岗前培训指南.md"
fi

echo ""
echo "🎉 岗前培训材料准备完成！"
echo ""
echo "📁 文件位置："
echo "   - 培训大纲：$AGENT_DIR/ONBOARDING.md"
echo "   - 任务清单：$AGENT_DIR/TASKS.md"
echo "   - Inbox 指南：$SHARED_DIR/INBOX-GUIDE.md"
echo "   - 培训指南：$SHARED_DIR/岗前培训指南.md"
echo ""
echo "📋 下一步："
echo "   1. 将培训材料发送给新员工"
echo "   2. 安排导师指导"
echo "   3. 跟踪培训进度"
echo ""
