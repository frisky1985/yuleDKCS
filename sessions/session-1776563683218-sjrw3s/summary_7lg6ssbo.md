## 任务背景
用户希望学习并安装两个工具：飞书 lark-cli 和 draw.io-skills。

## 执行过程
1. 搜索并确认 lark-cli 是飞书官方 Go CLI 工具，非 OpenClaw Skill
2. 从 GitHub 下载 lark-cli v1.0.14，安装到用户目录并配置 PATH
3. 确认用户已通过 Microsoft Store 安装 draw.io Desktop v29.6.6.0
4. 从 GitHub 安装 drawio-skills v2.2.0
5. 发现 draw.io Desktop 的 App Execution Alias 未配置

## 关键结果
- ✅ lark-cli v1.0.14 已安装到 `C:\Users\Lenovo\AppData\Local\lark-cli`
- ✅ drawio-skills v2.2.0 已安装到 `C:\Users\Lenovo\.agents\skills\drawio\`
- ✅ 生成了快速入门文档：larkcli-quickstart_2026-04-19_0954.md、drawio-skill-quickstart_2026-04-19_1906.md
- ⚠️ draw.io Desktop 的 App Execution Alias 未配置

## 结论建议
两个工具均已安装完成。lark-cli 需要用户运行 `lark-cli auth login` 完成飞书账号认证；draw.io Desktop 可通过开始菜单启动，Skill 已支持生成.drawio和.svg格式图表。