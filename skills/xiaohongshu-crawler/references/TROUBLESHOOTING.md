# 故障排查指南

## 问题 1: 需要登录才能搜索

**错误：** `❌ 查询失败：需要登录后才能搜索`

**原因：** Cookie 未配置或已过期

**解决：**
1. 运行 `node scripts/get-cookie.js` 重新获取 Cookie
2. 确保 `config.json` 中 `cookie.enabled` 为 `true`
3. Cookie 有效期较短（1-3 天），建议定期更新

## 问题 2: Cookie 已过期

**错误：** `⚠️ Cookie 可能已过期`

**解决：**
- 重新运行 `node scripts/get-cookie.js`
- 登录时勾选"记住我"
- 清除浏览器缓存后重新获取

## 问题 3: 被风控检测

**错误：** `⚠️ 页面状态异常，可能被风控`

**解决：**
1. 降低请求频率（增大 `random_delay`）
   ```json
   "random_delay": {
     "min": 5000,
     "max": 15000
   }
   ```
2. 检查网络连接
3. 使用代理 IP（如需要）
4. 等待一段时间再试

## 问题 4: 无搜索结果

**错误：** `⚠️ 未找到搜索结果`

**可能原因：**
- 搜索关键词过于特殊
- 小红书风控拦截
- 网络连接问题

**解决：**
1. 更换关键词重试
2. 检查 Cookie 是否有效
3. 检查网络代理设置
4. 尝试手动访问小红书搜索相同关键词

## 问题 5: 脚本运行报错

**错误：** `node:internal/modules/cjs/loader:xxx`

**解决：**
```bash
# 重新安装依赖
cd ~/.openclaw/workspace/skills/xiaohongshu-crawler
npm install
```

## 问题 6: 图片无法下载

**错误：** 下载笔记详情时图片无法保存

**解决：**
- 检查网络连接
- 检查磁盘空间
- 可能是小红书临时限制了图片访问

## 问题 7: 搜索速度慢

**现象：** 每次搜索需要较长时间

**原因：** 默认启用反爬保护，有随机延迟

**调整（不推荐）：**
```json
"anti_crawl": {
  "enabled": false
}
```

**注意：** 关闭反爬保护可能增加被风控风险。

## 问题 8: JSON 输出乱码

**现象：** JSON 文件中中文显示为 Unicode 编码

**解决：**
确保 Node.js 运行环境支持 UTF-8 编码，在脚本前添加：
```bash
export NODE_OPTIONS=--max-old-space-size=4096
```

## 问题 9: 无法打开浏览器获取 Cookie

**现象：** 运行 `get-cookie.js` 后浏览器没有打开

**解决：**
1. 检查是否安装了 Chromium/Chrome
2. 手动获取 Cookie（参见 `USAGE.md` 的方式二）
3. 检查系统权限设置

## 问题 10: 批量搜索时程序卡住

**现象：** 批量搜索到某一条时程序停止响应

**解决：**
1. 添加超时机制（脚本已内置）
2. 降低并发数量
3. 检查网络连接稳定性
4. 重启脚本重试

## 通用调试方法

### 查看详细日志

在脚本中添加调试输出：
```javascript
console.log('Debug:', variable);
```

### 测试网络连通性

```bash
curl -I https://www.xiaohongshu.com
```

### 检查 Node.js 版本

```bash
node --version
# 建议使用 Node.js 18+
```

### 清理缓存

```bash
# 删除 node_modules 重新安装
rm -rf node_modules package-lock.json
npm install
```

## 联系支持

如果以上方法都无法解决问题，请提供：
1. 完整的错误信息
2. Node.js 版本
3. 操作系统版本
4. 相关配置（脱敏后）
