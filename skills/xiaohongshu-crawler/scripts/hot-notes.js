const { initBrowser, createPage, delay, checkCache, saveCache, cleanupCache } = require('../lib/browser');
const antiCrawl = require('../lib/anti-crawl');
const path = require('path');

/**
 * Get hot/recommended notes
 */
async function getHotNotes(category = 'explore', page = 1, limit = 20) {
  const cacheKey = `hot_${category}_${page}`;
  const cached = checkCache(cacheKey);
  if (cached && cached.length > 0) {
    console.log('💾 Using cached result');
    return cached;
  }

  const browser = await initBrowser();
  const pageObj = await createPage(browser);
  
  try {
    // Navigate to hot notes page
    let hotUrl = 'https://www.xiaohongshu.com/explore';
    if (category && category !== 'explore') {
      hotUrl = `https://www.xiaohongshu.com/explore/${encodeURIComponent(category)}`;
    }
    
    console.log(`🔥 Fetching hot notes: ${category} (page ${page})`);
    
    await antiCrawl.makeRequest(pageObj, hotUrl, { timeout: 30000 });
    
    // Wait for content to load with anti-crawl delay
    await antiCrawl.randomDelay(3000, 6000);
    
    // Extract hot notes from page
    const hotNotes = await pageObj.evaluate((limitVal) => {
      const noteElements = Array.from(document.querySelectorAll('.note-item'));
      const results = [];
      
      noteElements.slice(0, limitVal).forEach((el) => {
        // 获取 href 中的 noteId
        const link = el.querySelector('a[href^="/explore/"]');
        const href = link?.href || '';
        const match = href.match(/\/explore\/([a-zA-Z0-9]+)/);
        const noteId = match ? match[1] : null;
        
        // 获取标题
        const titleEl = el.querySelector('.footer .title span');
        const title = titleEl?.textContent?.trim() || '无标题';
        
        // 获取图片
        const imgEl = el.querySelector('img');
        const cover = imgEl?.src || '';
        
        results.push({
          note_id: noteId,
          title: title,
          cover: cover,
          user: {
            nickname: '',  // 需要登录才能获取
            user_id: ''
          },
          likes: '',  // 需要深入分析
          collects: '',
          comments: '',
          url: noteId ? `https://www.xiaohongshu.com/explore/${noteId}` : ''
        });
      });
      
      return results;
    }, limit);
    
    await browser.close();
    
    // Save to cache
    saveCache(cacheKey, hotNotes);
    
    return hotNotes;
    
  } catch (error) {
    await browser.close();
    throw error;
  }
}

/**
 * Main CLI function
 */
async function main() {
  cleanupCache();
  
  const args = process.argv.slice(2);
  const category = args[0] || 'explore';
  const page = parseInt(args[1]) || 1;
  const limit = parseInt(args[2]) || 20;
  const format = args[3] === 'json' ? 'json' : 'text';
  
  if (args[0] === '--help' || args[0] === '-h') {
    console.log(`
🔥 获取热门笔记

用法：node scripts/hot-notes.js [分类] [页码] [数量] [格式]

参数:
  分类      笔记分类 (默认：explore)
  页码      第几页 (默认：1)
  数量      返回数量 (默认：20)
  格式      json|text (默认：text)

常用分类:
  tech      科技
  fashion   时尚
  food      美食
  travel    旅行
  beauty    美妆
  fitness   健身
  
示例:
  node scripts/hot-notes.js
  node scripts/hot-notes.js tech 1 10
  node scripts/hot-notes.js fashion 1 20 json
    `);
    process.exit(0);
  }
  
  try {
    const hotNotes = await getHotNotes(category, page, limit);
    
    if (format === 'json') {
      console.log(JSON.stringify({
        category,
        page,
        total: hotNotes.length,
        data: hotNotes
      }, null, 2));
    } else {
      hotNotes.forEach((note, idx) => {
        console.log(`\n${idx + 1}. ${note.title}`);
        console.log(`   ID: ${note.note_id}`);
        console.log(`   👤 ${note.user.nickname || '待登录获取'}`);
        console.log(`   ❤️ ${note.likes || '?'} | 💾 ${note.collects || '?'} | 💬 ${note.comments || '?'}`);
        console.log(`   🔗 ${note.url}`);
      });
    }
    
  } catch (error) {
    console.error('❌ 获取失败:', error.message);
    process.exit(1);
  }
}

main();
