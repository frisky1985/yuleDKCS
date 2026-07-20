/**
 * 成都旅游笔记抓取工具 - 通过探索页筛选
 */

const { initBrowser, createPage, delay } = require('../lib/browser');
const antiCrawl = require('../lib/anti-crawl');

/**
 * 抓取探索页笔记并筛选成都相关内容
 */
async function scrapeChengduNotes(limit = 50) {
  const browser = await initBrowser();
  const page = await createPage(browser);
  
  try {
    console.log(`🌏 成都旅游笔记抓取工具\n`);
    console.log(`📝 从探索页抓取 ${limit} 篇笔记，筛选成都相关内容`);
    console.log('');
    
    // 访问探索页
    await page.goto('https://www.xiaohongshu.com/explore', {
      waitUntil: 'networkidle',
      timeout: 30000
    });
    
    await antiCrawl.randomDelay(3000, 5000);
    
    // 提取笔记信息
    const allNotes = await page.evaluate((limitVal) => {
      const items = Array.from(document.querySelectorAll('.note-item')).slice(0, limitVal);
      return items.map(el => {
        const link = el.querySelector('a[href^="/explore/"]')?.href || '';
        const idMatch = link.match(/\/(explore|discovery\/item)\/([a-zA-Z0-9]+)/);
        const noteId = idMatch ? idMatch[2] : '';
        
        return {
          note_id: noteId,
          title: el.querySelector('.footer .title span')?.textContent?.trim() || '无标题',
          url: noteId ? `https://www.xiaohongshu.com/explore/${noteId}` : '',
          img: el.querySelector('img')?.src || ''
        };
      });
    }, limit);
    
    await browser.close();
    
    // 筛选成都相关内容
    const chengduNotes = allNotes.filter(note => {
      const title = note.title.toLowerCase();
      return title.includes('成都') || 
             title.includes('四川') || 
             title.includes('火锅') ||
             title.includes('熊猫') ||
             title.includes('春熙路') ||
             title.includes('宽窄巷子');
    });
    
    // 格式化输出
    console.log(`✅ 抓取完成！\n`);
    console.log(`📊 总计：${allNotes.length} 篇`);
    console.log(`🌏 成都相关：${chengduNotes.length} 篇`);
    console.log('');
    
    if (chengduNotes.length === 0) {
      console.log('⚠️  未在探索页找到成都相关笔记');
      console.log('💡 建议：使用搜索功能或配置 Cookie 后使用 search.js\n');
      return [];
    }
    
    chengduNotes.forEach((note, idx) => {
      console.log(`${idx + 1}. ${note.title}`);
      console.log(`   ID: ${note.note_id}`);
      console.log(`   🔗 ${note.url}\n`);
    });
    
    return chengduNotes;
    
  } catch (error) {
    await browser.close();
    console.error('❌ 抓取失败:', error.message);
    throw error;
  }
}

/**
 * 主函数
 */
async function main() {
  const args = process.argv.slice(2);
  const limit = parseInt(args[0]) || 50;
  const format = args[1] === 'json' ? 'json' : 'text';
  
  if (args[0] === '--help' || args[0] === '-h') {
    console.log(`
成都旅游笔记抓取工具

用法：node scripts/chengdu-travel.js [数量] [格式]

参数:
  数量      从探索页抓取数量 (默认：50)
  格式      json|text (默认：text)

说明：
  - 从探索页抓取笔记并筛选成都相关内容
  - 可能无法找到足够的成都相关笔记
  - 建议使用搜索功能或配置 Cookie

示例:
  node scripts/chengdu-travel.js
  node scripts/chengdu-travel.js 100
  node scripts/chengdu-travel.js 50 json
    `);
    process.exit(0);
  }
  
  try {
    const notes = await scrapeChengduNotes(limit);
    
    if (format === 'json') {
      console.log('\n📋 JSON 格式输出:');
      console.log(JSON.stringify(notes, null, 2));
    }
    
    console.log('\n📊 请求统计:');
    const stats = antiCrawl.getRequestStats();
    console.log(`   本分钟：${stats.minute} 次请求`);
    console.log(`   本小时：${stats.hour} 次请求`);
    
  } catch (error) {
    console.error('❌ 执行失败:', error.message);
    process.exit(1);
  }
}

main();
