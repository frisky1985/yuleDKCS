# lark-cli 快速入门指南

## 📦 安装信息

- **版本**: v1.0.14
- **安装位置**: `C:\Users\Lenovo\AppData\Local\lark-cli`
- **可执行文件**: `lark-cli.exe`
- **PATH**: 已添加到用户 PATH 环境变量

## 🔑 第一步：认证登录

lark-cli 需要先登录才能使用飞书 API。有两种认证方式：

### 方式1：设备流认证（推荐，个人使用）

```bash
lark-cli auth login
```

这将打开浏览器进行 OAuth 认证，完成后 token 会保存在本地。

### 方式2：应用凭证认证（企业/开发者）

1. 在飞书开放平台创建应用：https://open.feishu.cn/app
2. 获取 App ID 和 App Secret
3. 配置凭证：

```bash
# 设置应用凭证
lark-cli config set app_id YOUR_APP_ID
lark-cli config set app_secret YOUR_APP_SECRET

# 登录
lark-cli auth login
```

### 验证认证状态

```bash
lark-cli auth status
lark-cli auth list
```

## 📚 核心功能

### 三层命令架构

lark-cli 采用三层命令架构，满足不同使用场景：

#### Layer 1: 快捷命令 (Shortcuts) - `+` 前缀
最简单易用，智能默认参数，适合人类和 AI Agent 快速操作

```bash
# 查看今日日程
lark-cli calendar +agenda

# 创建日程
lark-cli calendar +create --title "项目会议" --start "2026-04-20 14:00"

# 搜索联系人
lark-cli contact +search-user --query "张三"

# 创建文档
lark-cli docs +create --title "会议纪要"
```

#### Layer 2: 资源命令 - `<服务> <资源> <方法>`
面向资源操作，参数更灵活，适合批量操作

```bash
# 列出日历事件
lark-cli calendar events list --params '{"calendar_id":"primary"}'

# 发送消息
lark-cli im messages create --params '{"receive_id":"ou_xxx","receive_id_type":"open_id"}' --data '{"content":"你好"}'

# 读取文档内容
lark-cli docs documents read --params '{"document_id":"doxcnxxx"}'
```

#### Layer 3: API 命令 - `api <method> <path>`
直接调用飞书 API，完全控制，适合高级用户

```bash
# 通用 API 调用
lark-cli api GET /open-apis/calendar/v4/calendars

# POST 请求
lark-cli api POST /open-apis/im/v1/messages --params '{"receive_id":"ou_xxx","receive_id_type":"open_id"}' --data '{"content":"Hello"}'
```

## 📋 常用命令速查

### 日历 (Calendar)

```bash
# 查看日程
lark-cli calendar +agenda                    # 今日日程
lark-cli calendar +agenda --date tomorrow    # 明日日程
lark-cli calendar +agenda --date 2026-04-20  # 指定日期

# 创建会议
lark-cli calendar +create --title "周会" --start "2026-04-20 10:00" --duration 1h

# 查询忙闲
lark-cli calendar +freebusy --user "ou_xxx" --start "2026-04-20" --end "2026-04-21"
```

### 消息 (IM)

```bash
# 发送消息
lark-cli im messages create --params '{"receive_id":"ou_xxx","receive_id_type":"open_id"}' --data '{"msg_type":"text","content":"{\"text\":\"Hello\"}"}'

# 创建群聊
lark-cli im chats create --data '{"name":"项目组"}'
```

### 文档 (Docs)

```bash
# 创建文档
lark-cli docs +create --title "新文档"

# 读取文档
lark-cli docs documents read --params '{"document_id":"doxcnxxx"}'

# 更新文档
lark-cli docs documents update --params '{"document_id":"doxcnxxx"}' --data '{"content":"..."}'
```

### 电子表格 (Sheets)

```bash
# 创建表格
lark-cli sheets +create --title "数据表"

# 读取单元格
lark-cli sheets spreadsheets.values.get --params '{"spreadsheetToken":"xxx","range":"Sheet1!A1:B10"}'

# 写入数据
lark-cli sheets spreadsheets.values.update --params '{"spreadsheetToken":"xxx","range":"Sheet1!A1"}' --data '{"values":[["A1","B1"],["A2","B2"]]}'
```

### 多维表格 (Base)

```bash
# 列出表格
lark-cli base tables list --params '{"app_token":"xxx"}'

# 创建记录
lark-cli base records create --params '{"app_token":"xxx","table_id":"tblxxx"}' --data '{"fields":{"字段名":"值"}}'
```

### 文件管理 (Drive)

```bash
# 上传文件
lark-cli drive files upload --params '{"parent_node":"fldxxx"}' --data '{"file":"path/to/file.pdf"}'

# 下载文件
lark-cli drive files download --params '{"file_token":"filxxx"}' --output ./downloaded.pdf
```

### 任务 (Task)

```bash
# 创建任务
lark-cli task +create --title "完成任务" --due "2026-04-20"

# 查看任务列表
lark-cli task tasks list
```

## 🤖 AI Agent Skills

lark-cli 内置 19 个 AI Agent Skills，可以让 Claude Code、Cursor 等 AI 工具直接操作飞书：

### 安装 Skills

```bash
# 安装全部 Skills
npx skills add larksuite/cli -g -y

# 安装特定域 Skills
npx skills add larksuite/cli -s lark-calendar -y  # 日历
npx skills add larksuite/cli -s lark-im -y        # 消息
npx skills add larksuite/cli -s lark-docs -y      # 文档
```

### 可用 Skills 列表

1. **lark-calendar** - 日历管理
2. **lark-im** - 即时消息
3. **lark-docs** - 云文档
4. **lark-sheets** - 电子表格
5. **lark-base** - 多维表格
6. **lark-drive** - 云空间
7. **lark-wiki** - 知识库
8. **lark-contact** - 通讯录
9. **lark-mail** - 邮箱
10. **lark-task** - 任务
11. **lark-approval** - 审批
12. **lark-vc** - 视频会议
13. **lark-attendance** - 考勤
14. **lark-okr** - OKR
15. **lark-event** - 事件订阅
16. **lark-whiteboard** - 白板
17. **lark-slides** - 幻灯片
18. **lark-minutes** - 会议纪要
19. **lark-bitable** - 多维表格（增强版）

## 🔧 高级用法

### 输出格式

```bash
# JSON 格式（默认）
lark-cli calendar +agenda

# 表格格式
lark-cli calendar +agenda --format table

# CSV 格式
lark-cli calendar +agenda --format csv

# 格式化输出
lark-cli calendar +agenda --format pretty
```

### 数据过滤 (jq)

```bash
# 使用 jq 过滤结果
lark-cli calendar events list --jq '.events[] | {summary, start: .start.time}'

# 简写
lark-cli calendar events list -q '.events[].summary'
```

### 分页控制

```bash
# 自动获取所有页
lark-cli base records list --params '{"app_token":"xxx","table_id":"tblxxx"}' --page-all

# 限制页数
lark-cli base records list --page-all --page-limit 5

# 设置每页数量
lark-cli base records list --page-size 100
```

### 多配置文件

```bash
# 创建新配置
lark-cli profile create work

# 切换配置
lark-cli profile use work

# 使用特定配置运行命令
lark-cli calendar +agenda --profile work
```

## 🔍 调试技巧

```bash
# 健康检查
lark-cli doctor

# 查看当前配置
lark-cli config view

# 查看认证状态
lark-cli auth status

# 查看权限范围
lark-cli auth scopes

# 预览请求（不执行）
lark-cli calendar +create --title "测试" --dry-run

# 查看 API schema
lark-cli schema calendar.events.list
```

## 📚 资源链接

- **GitHub 仓库**: https://github.com/larksuite/cli
- **官方文档**: https://open.feishu.cn/document/
- **API 文档**: https://open.feishu.cn/document/server-docs
- **问题反馈**: https://github.com/larksuite/cli/issues

## 📝 注意事项

1. **首次使用**：需要先运行 `lark-cli auth login` 完成认证
2. **权限要求**：不同命令需要不同的权限范围（scopes），在飞书开放平台配置
3. **速率限制**：飞书 API 有速率限制，建议使用 `--page-delay` 参数控制请求频率
4. **数据安全**：Token 保存在本地，请勿泄露配置文件
5. **更新**：运行 `lark-cli update` 更新到最新版本

## 🎯 快速测试

完成认证后，运行以下命令测试：

```bash
# 查看认证状态
lark-cli auth status

# 查看今日日程
lark-cli calendar +agenda

# 搜索自己的用户信息
lark-cli contact +search-user --query "me"

# 健康检查
lark-cli doctor
```
