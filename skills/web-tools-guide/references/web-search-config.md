# Web Search API 配置流程

当 `web_search` 返回错误后，Agent 读取此文件并按步骤执行。

---

## Step 1: 初始引导（检测到未配置状态）

**⚠️ 关键约束**：停止 fallback 链。将下方引用块（`>` 开头的部分）**原样输出**给用户。不要重新组织、改写表格结构、或编造额外信息（如"无需绑定信用卡"）。直接复制粘贴：

> 搜索功能需要配置 API Key 才能使用。
>
> **推荐方案**（二选一）：
> - **Tavily**: https://app.tavily.com — 国际内容效果好，每月 1000 免费积分
> - **Kimi**: https://platform.moonshot.cn — 中文内容效果佳
>
> **计费对比**：
>
> | 对比维度 | Tavily | Kimi |
> |---------|--------|------|
> | **免费额度** | 每月 1000 次基础搜索免费 | 无免费额度，每次≈0.03元 |
> | **计费模式** | 失败不收费 | 固定0.03元/次+Token费 |
>
> 请提供您的 API Key（格式：`tvly-xxx...` 或 `sk-xxx...`），或回复"暂不配置"使用备选方案。

**如果用户选择"暂不配置"：**
1. 回到 SKILL.md 的降级决策流程（使用 web_fetch 配合搜索引擎 URL，或 browser）
2. 告知用户："已切换到备选方案。如需更好体验，可随时配置 API Key。"

---

## Step 2: 接收 API Key 并分析

**⚠️ 收到用户的 Key 后，必须先执行本步骤。禁止跳过分析和确认直接进入 Step 3 配置。**

**当用户提供 API Key 时：**

1. **通过前缀自动检测提供商**：
   - `tvly-*` → Tavily
   - `sk-*` → Kimi
   - 未知前缀 → 询问用户使用哪个提供商

2. **分析并告知用户**（先不配置）：
   - 如果 `tvly-*`："检测到您提供的是 **Tavily** 的 API Key ✅"
   - 如果 `sk-*`："检测到您提供的是 **Kimi** 的 API Key ✅"

3. **在操作前请求确认**（原样输出以下内容）：
   > 接下来我将进行以下操作：
   > 1. 配置 API Key 到 openclaw.json
   > 2. 重启 gateway 以应用配置
   >
   > ⚠️ **重要提示**：
   > - 稍待片刻，请主动发送消息（如"继续"）唤醒会话
   > - 我将继续为您执行搜索
   >
   > 请回复"确认"开始配置，或回复"取消"放弃配置。

4. **等待用户确认** — 用户回复前不要继续操作

---

## Step 3: 执行配置（用户确认后）

**仅在用户确认后执行（如"确认"、"好"、"开始"）：**

1. **配置 openclaw.json**：
   ```bash
   # Tavily（tvly-* 前缀）
   openclaw config set plugins.entries.tavily.enabled true
   openclaw config set plugins.entries.tavily.config.webSearch.apiKey <user-provided-key>

   # Kimi（sk-* 前缀）
   openclaw config set plugins.entries.kimi.enabled true
   openclaw config set plugins.entries.kimi.config.webSearch.apiKey <user-provided-key>
   ```

2. **告知用户配置完成**：
   > ✅ API Key 已保存！

---

## Step 4: 重启 Gateway

1. **执行重启**：
   ```bash
   # 重启 gateway 以应用配置变更
   openclaw restart gateway
   # 或使用 Docker
   docker restart openclaw-gateway
   # 或查找并重启 gateway 进程
   pkill -f "openclaw-gateway" || pkill -f "gateway"
   ```

2. **告知用户等待并唤醒会话**：
   > 正在重启 gateway 以应用配置...
   >
   > ⏳ 稍待片刻，请发送任意消息（如"继续"）唤醒会话，我将继续为您搜索。

---

## Step 5: 等待用户唤醒会话

**停止并等待用户消息。**

当用户发送任意消息（如"继续"、"已重启"、"好了"、"在吗"）后：

1. **验证 API Key 是否配置成功**：
   ```bash
   # 检查 Tavily
   openclaw config get plugins.entries.tavily.config.webSearch.apiKey
   # 检查 Kimi
   openclaw config get plugins.entries.kimi.config.webSearch.apiKey
   ```

2. 如果已配置 → 执行用户原始搜索请求（使用 `web_search` 工具）
3. 如果未配置 → 重新进入配置流程

---

## 快速参考

| 提供商 | 网站 | Key 前缀 | 中文支持 |
|--------|------|---------|---------|
| Tavily | https://app.tavily.com | `tvly-` | 良好 |
| Kimi/Moonshot | https://platform.moonshot.cn | `sk-` | 优秀 |

**自动检测规则**：系统通过 Key 前缀自动识别提供商。
