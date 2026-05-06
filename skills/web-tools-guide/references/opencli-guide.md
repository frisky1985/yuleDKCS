# opencli 使用指引

`opencli` 是一个命令行工具，将 70+ 主流网站封装为 CLI 命令。相比 browser 自动化，它更快（秒级）、更精准（结构化数据）、更稳定（不依赖 DOM 变化）。

## 核心理念

不需要记住任何命令。通过 `--help` 逐层探索即可：

```bash
opencli --help                         # 我能操作哪些网站？
opencli <site> --help                  # 这个网站有哪些操作？
opencli <site> <command> --help        # 这个操作怎么用？
```

## 典型场景映射

以下是常见用户需求到 opencli 命令的映射示例（不完整，遇到新场景先 `--help` 探索）：

| 用户需求 | opencli 命令 |
|---------|-------------|
| 看微博热搜 | `opencli weibo hot` |
| 搜索小红书笔记 | `opencli xiaohongshu search <关键词>` |
| 看知乎热榜 | `opencli zhihu hot` |
| B站热门视频 | `opencli bilibili hot` |
| 搜索 Twitter | `opencli twitter search <关键词>` |
| 看 HackerNews 头条 | `opencli hackernews top` |
| GitHub trending | `opencli gh trending` |
| 获取笔记详情/评论 | `opencli xiaohongshu note <note-id>` |
| 获取 YouTube 字幕 | `opencli youtube transcript <video-id>` |
| 读取任意网页为 Markdown | `opencli web read --url <URL>` |

## 输出格式

所有命令支持 `-f` / `--format` 选项：

| 格式 | 用途 |
|------|------|
| `table` | 默认，终端可读 |
| `json` | 程序化处理、需要完整结构化数据 |
| `yaml` | 人类可读的结构化数据 |
| `md` | Markdown 格式 |
| `csv` | 表格导出 |
| `plain` | 纯文本 |

推荐：给用户展示时用 `table` 或 `md`，需要后续处理时用 `-f json`。

## 常见参数

- `--limit <N>`：限制返回条数
- `-f <format>`：输出格式
- `-v` / `--verbose`：调试输出

## 覆盖的网站类别

以下是 opencli 覆盖的主要网站类别（可通过 `opencli --help` 查看完整列表）：

- **中文社交**：微博、小红书、知乎、豆瓣、即刻、V2EX、贴吧
- **中文视频**：B站、抖音
- **中文资讯**：36氪、雪球、什么值得买
- **阅读**：微信读书、知识星球
- **国际社交**：Twitter/X、Reddit、LinkedIn、Instagram、Facebook、Bluesky
- **国际视频**：YouTube、TikTok
- **国际资讯**：HackerNews、ProductHunt、Medium、Substack、BBC、Bloomberg、Reuters
- **开发者**：GitHub (gh)、StackOverflow、Dev.to、ArXiv、HuggingFace
- **AI 工具**：ChatGPT、Gemini、Grok、Doubao
- **购物**：JD、Amazon、Coupang、Steam
- **播客**：Apple Podcasts、小宇宙、Spotify
- **通用**：`opencli web read` — 任意网页转 Markdown

## 写操作注意

部分命令支持写操作（发帖、关注、点赞等），执行前必须告知用户并获取确认：

- `opencli twitter post <text>` — 发推文
- `opencli xiaohongshu publish <content>` — 发小红书笔记
- `opencli twitter follow/block/like` — 关注/拉黑/点赞
- 其他涉及修改数据的操作

## 故障排查

如果 opencli 命令执行失败：

1. **检查 daemon 状态**：`opencli doctor` — 诊断浏览器桥接连接
2. **查看详细错误**：加 `-v` 参数获取调试信息
3. **降级处理**：告知用户 opencli 失败原因，降级到 web_search/web_fetch/browser
