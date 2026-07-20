# 小红书爬虫使用示例

## 示例 1：搜索热门关键词

```bash
node scripts/search.js 手机测评 1 20 text
```

**输出示例：**
```
1. iPhone 15 Pro Max 深度测评
   📝 ID: 64a1b2c3d4e5f
   👤 科技测评君
   ❤️ 8520 | 💾 2340 | 💬 456
   🔗 https://www.xiaohongshu.com/discovery/item/64a1b2c3d4e5f

2. 华为 Mate 60 Pro 真实体验
   📝 ID: 65b2c3d4e5f6g
   👤 数码达人
   ❤️ 12300 | 💾 3450 | 💬 678
   🔗 https://www.xiaohongshu.com/discovery/item/65b2c3d4e5f6g
```

## 示例 2：JSON 格式导出

```bash
node scripts/search.js 旅行攻略 1 10 json > search_results.json
```

**输出 JSON：**
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

## 示例 3：获取笔记详情

```bash
node scripts/get-note.js 64a1b2c3d4e5f
```

**输出：**
```
📝 iPhone 15 Pro Max 深度测评
   ID: 64a1b2c3d4e5f
   👤 科技测评君
   ❤️ 8520 | 💾 2340 | 💬 456
   🖼️ 图片：8 张

内容：
   这款手机搭载了 A17 Pro 芯片，性能大幅提升...

🔗 https://www.xiaohongshu.com/discovery/item/64a1b2c3d4e5f
```

## 示例 4：获取用户信息

```bash
node scripts/get-user.js 5f6g7h8i9j0k1l2m3n4o5p6q 10
```

**输出：**
```
👤 科技测评君 ✅
   ID: 5f6g7h8i9j0k1l2m3n4o5p6q
   📊 粉丝：125000 | 关注：350 | 笔记：280 | 获赞：2500000
   📝 10 篇最近笔记

🔗 https://www.xiaohongshu.com/user/profile/5f6g7h8i9j0k1l2m3n4o5p6q
```

## 示例 5：获取热门笔记

```bash
# 获取科技类热门笔记
node scripts/hot-notes.js tech 1 20

# 获取美食类热门笔记
node scripts/hot-notes.js food 1 20 json
```

**输出：**
```
1. 上海必吃美食排行榜
   👤 美食探店
   ❤️ 15000 🏷️ 美食 探店
   🔗 https://www.xiaohongshu.com/discovery/item/64a1b2c3d4e5f

2. 北京胡同里的老字号
   👤 吃货日记
   ❤️ 8900 🏷️ 美食 北京
   🔗 https://www.xiaohongshu.com/discovery/item/65b2c3d4e5f6g
```

## 示例 6：批量搜索并保存

```bash
# 搜索并保存为 JSON
for keyword in "手机测评" "旅行攻略" "美食探店"; do
  echo "Searching for $keyword..."
  node scripts/search.js "$keyword" 1 20 json > "${keyword}.json"
done
```

## 注意事项

1. **API 限制** - 每次搜索建议间隔 2-3 秒
2. **缓存机制** - 相同查询会在 1 小时内使用缓存
3. **数据准确性** - 抓取的是公开数据，仅供参考
4. **合规使用** - 请遵守小红书用户协议
