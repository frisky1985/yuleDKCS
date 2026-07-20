const { initBrowser, createPage, delay, checkCache, saveCache, cleanupCache } = require('../lib/browser');
const axios = require('axios');
const cheerio = require('cheerio');
const path = require('path');

/**
 * Search Xiaohongshu notes (requires login)
 */
async function searchNotes(keyword, pageNum = 1, limit = 20) {
  const cacheKey = `search_${keyword}_${pageNum}`;
  const cached = checkCache(cacheKey);
  if (cached && cached.length > 0) {
    console.log('💾 Using cached result');
    return cached;
  }

  const browser = await initBrowser();
  const pageObj = await createPage(browser);
  
  try {
    // Navigate to search page
    const searchUrl = `https://www.xiaohongshu.com/search_result?keyword=${encodeURIComponent(keyword)}&source=web_search_result_notes`;
    console.log(`🔍 Searching: ${keyword} (page ${pageNum})`);
    
    await pageObj.goto(searchUrl, {
      waitUntil: 'networkidle',
      timeout: 30000
    });
    
    // Wait for content to load
    await delay(5000);
    
    // Check if login is required
    const needsLogin = await pageObj.evaluate(() => {
      const text = document.body.textContent || '';
      return text.includes('登录后查看搜索结果') || text.includes('请登录');
    });
    
    if (needsLogin) {
      await browser.close();
      throw new Error('需要登录后才能搜索。请运行 "node scripts/get-cookie.js" 获取 Cookie，然后启用 config.json 中的 cookie.enabled');
    }
    
    // Extract data from page
    const notes = await pageObj.evaluate((limitVal) => {
      const noteElements = Array.from(document.querySelectorAll('.note-item'));
      const results = [];
      
      noteElements.slice(0, limitVal).forEach((el) => {
        // 获取 href 中的 noteId
        const link = el.querySelector('a[href^="/explore/"]') || el.querySelector('a[href^="/discovery/item/"]');
        const href = link?.href || '';
        
        // 尝试多种格式提取 ID
        let noteId = null;
        const idMatch = href.match(/\/(explore|discovery\/item)\/([a-zA-Z0-9]+)/);
        if (idMatch) {
          noteId = idMatch[2];
        }
        
        results.push({
          note_id: noteId,
          title: el.querySelector('.footer .title span')?.textContent?.trim() || el.querySelector('[class*="title"]')?.textContent?.trim() || '无标题',
          cover: el.querySelector('img')?.src || '',
          user: {
            nickname: el.querySelector('.user-info')?.textContent?.trim() || el.querySelector('[class*="user"]')?.textContent?.trim() || '',
            user_id: el.getAttribute('data-user-id') || ''
          },
          likes: el.querySelector('[class*="like"]')?.textContent?.trim() || '0',
          collects: el.querySelector('[class*="collect"]')?.textContent?.trim() || '0',
          comments: el.querySelector('[class*="comment"]')?.textContent?.trim() || '0',
          url: noteId ? `https://www.xiaohongshu.com/explore/${noteId}` : ''
        });
      });
      
      return results;
    }, limit);
    
    await browser.close();
    
    // Save to cache
    saveCache(cacheKey, notes);
    
    return notes;
    
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
  const keyword = args[0];
  const page = parseInt(args[1]) || 1;
  const limit = parseInt(args[2]) || 20;
  const format = args[3] === 'json' ? 'json' : 'text';
  
  if (!keyword || keyword === '--help' || keyword === '-h') {
    console.log(`
🔍 小红书搜索（需要登录）

用法：node scripts/search.js <关键词> [页码] [数量] [格式]

参数:
  关键词    搜索关键词
  页码      第几页 (默认：1)
  数量      返回数量 (默认：20)
  格式      json|text (默认：text)

注意：搜索功能需要登录，请先运行 "node scripts/get-cookie.js" 获取 Cookie

示例:
  node scripts/search.js 手机测评
  node scripts/search.js iPhone 15 2 10 json
    `);
    process.exit(0);
  }
  
  try {
    const notes = await searchNotes(keyword, page, limit);
    
    if (notes.length === 0) {
      console.log('⚠️  未找到搜索结果');
      console.log('💡 提示：请确保已登录并启用 Cookie\n');
      process.exit(0);
    }
    
    if (format === 'json') {
      console.log(JSON.stringify({
        keyword,
        page,
        total: notes.length,
        data: notes
      }, null, 2));
    } else {
      notes.forEach((note, idx) => {
        console.log(`\n${idx + 1}. ${note.title}`);
        console.log(`   ID: ${note.note_id || '未知'}`);
        console.log(`   👤 ${note.user.nickname || '未知'}`);
        console.log(`   ❤️ ${note.likes} | 💾 ${note.collects} | 💬 ${note.comments}`);
        console.log(`   🔗 ${note.url}`);
      });
    }
    
  } catch (error) {
    console.error('❌ 查询失败:', error.message);
    process.exit(1);
  }
}

main();
