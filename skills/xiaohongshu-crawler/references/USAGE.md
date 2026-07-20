# 小红书爬虫完整使用指南

## 📋 功能概览

本技能提供完整的小红书内容爬取工具：

- 搜索笔记（需要登录）
- 获取笔记详情
- 获取用户信息
- 获取热门笔记（无需登录）
- 批量深度爬取
- Cookie 获取与管理

## ⚙️ 详细配置

### Cookie 配置（搜索功能必需）

小红书搜索功能需要登录才能使用。有两种获取 Cookie 的方式：

#### 方式一：交互式获取（推荐）

```bash
node scripts/get-cookie.js
```

**步骤：**
1. 脚本会打开浏览器并跳转到小红书登录页
2. 扫码登录你的小红书账号
3. 登录成功后，在终端输入 `save` 并回车
4. Cookie 自动保存到 `config.json`

#### 方式二：手动复制

1. 打开浏览器，访问小红书并扫码登录
2. 按 **F12** 打开开发者工具
3. 切换到 **Network** 标签页
4. 刷新页面，点击任意请求
5. 复制 **Request Headers** 中的 **Cookie** 值
6. 运行 `node scripts/test-cookie.js` 测试有效性

### 编辑配置文件

编辑 `config.json`，确保 Cookie 已启用：

```json
{
  "cookie": {
    "enabled": true,
    "items": [...]
  },
  "anti_crawl": {
    "enabled": true,
    "random_delay": {
      "min": 2000,
      "max": 8000
    },
    "rate_limit": {
      "max_requests_per_minute": 10
    }
  }
}
```

### 测试 Cookie

```bash
node scripts/test-cookie.js
```

如果显示 "✅ Cookie 有效，登录状态正常"，则表示配置成功。

## 📝 详细使用示例

### 快速搜索

```bash
node scripts/quick-search.js "四川旅游" 30
```

**说明：**
- 第一个参数：搜索关键词
- 第二个参数（可选）：最大结果数，默认 20

**输出：**
- 搜索结果保存为 `{keyword}-{timestamp}.json`
- 页面截图保存为 `search-result.png`

### 深度爬取

```bash
node scripts/deep-crawl.js "四川旅游" 20
```

**说明：**
- 爬取指定数量的笔记，并获取每篇笔记的详细内容
- 输出包含标题、作者、点赞数、收藏数、评论数、标签等

**输出文件：**
- `{keyword}-results.json` - 搜索结果列表
- `{keyword}-detailed.json` - 包含详细内容的笔记
- `search-detail.png` - 页面截图
- `reports/{keyword}-report.md` - Markdown 格式的分析报告

### 获取笔记详情

```bash
node scripts/get-note.js "https://www.xiaohongshu.com/discovery/item/65..."
```

### 获取热门笔记

```bash
node scripts/hot-notes.js
```

**注意：** 热门笔记无需登录即可获取。

### 批量搜索并保存

```bash
# 搜索并保存为 JSON
for keyword in "手机测评" "旅行攻略" "美食探店"; do
  echo "Searching for $keyword..."
  node scripts/search.js "$keyword" 1 20 json > "${keyword}.json"
done
```

## 🛠️ 脚本详细说明

| 脚本 | 功能 | 需要登录 | 参数说明 |
|------|------|---------|---------|
| `get-cookie.js` | 交互式获取 Cookie | - | 无参数，按提示操作 |
| `test-cookie.js` | 测试 Cookie 有效性 | - | 无参数 |
| `quick-search.js` | 快速搜索笔记 | ✅ | `关键词` `[数量]` |
| `deep-crawl.js` | 深度爬取笔记详情 | ✅ | `关键词` `[数量]` |
| `get-note.js` | 获取单条笔记详情 | ✅ | `笔记 ID 或 URL` |
| `get-user.js` | 获取用户信息 | ✅ | `用户 ID` `[笔记数量]` |
| `hot-notes.js` | 获取热门笔记 | 可选 | `[分类]` `[页数]` `[数量]` |

## 📊 输出格式

### JSON 格式输出

```json
{
  "keyword": "旅行攻略",
  "page": 1,
  "total": 10,
  "data": [
    {
      "note_id": "64a1b2c3d4e5f",
      "title": "日本旅行 7 天 6 晚攻略",
      "cover": "https://...",
      "user": {
        "nickname": "旅行达人",
        "user_id": "5f6g7h8i9j0k"
      },
      "likes": "5200",
      "collects": "1800",
      "comments": "230",
      "url": "https://www.xiaohongshu.com/discovery/item/64a1b2c3d4e5f"
    }
  ]
}
```

### Markdown 报告格式

深度爬取会生成 Markdown 格式的分析报告，包含：
- 关键词概述
- 热门笔记列表
- 用户分布统计
- 标签云分析
- 内容趋势

## ⚠️ 使用规范

### 合规使用

本工具仅限**个人学习和研究**使用，请遵守：

- ✅ **允许：**
  - 个人学习和研究
  - 公开内容爬取
  - 小批量数据获取（建议每次<50 条）

- ❌ **禁止：**
  - 商业用途
  - 大规模高频次爬取
  - 爬取私人/隐私内容
  - 绕过付费内容
  - 分发或售卖爬取数据

### 反爬保护

- 默认启用随机延迟（2-8 秒）
- 限制每分钟最多 10 个请求
- 模拟人类浏览行为

### 法律责任

使用本工具产生的数据仅供个人学习研究，请遵守：
- 小红书用户协议
- 相关法律法规
- 知识产权规定

## 🔄 Cookie 管理

### Cookie 有效期

- Cookie 有效期较短（1-3 天）
- 建议定期更新
- 登录时勾选"记住我"可延长有效期

### 更新 Cookie

```bash
# 1. 重新获取
node scripts/get-cookie.js

# 2. 测试有效性
node scripts/test-cookie.js
```

### 清除 Cookie

编辑 `config.json`，将 `cookie.items` 清空或设置 `cookie.enabled: false`。

## 📁 文件结构

```
xiaohongshu-crawler/
├── SKILL.md              # 使用文档
├── config.json           # 配置文件
├── config.example.json   # 配置示例
├── lib/
│   └── xhs-crawler.js   # 核心爬虫库
├── scripts/
│   ├── get-cookie.js    # Cookie 获取
│   ├── test-cookie.js   # Cookie 测试
│   ├── quick-search.js  # 快速搜索
│   ├── deep-crawl.js    # 深度爬取
│   ├── get-note.js      # 获取笔记详情
│   ├── get-user.js      # 获取用户信息
│   └── hot-notes.js     # 获取热门笔记
├── references/           # 参考文档
│   └── USAGE.md         # 本文件
└── reports/             # 报告输出目录
```

## 📞 技术支持

遇到问题请检查：
1. Cookie 是否有效（运行 `test-cookie.js`）
2. 网络连接是否正常
3. 反爬设置是否合理

详细故障排查请查看 `references/TROUBLESHOOTING.md`。
