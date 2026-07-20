const { initBrowser, createPage, delay, checkCache, saveCache, cleanupCache } = require('../lib/browser');
const path = require('path');

/**
 * Get note details by note ID (requires login)
 */
async function getNoteDetail(noteId) {
  const cacheKey = `note_${noteId}`;
  const cached = checkCache(cacheKey);
  if (cached && cached.note_id) {
    console.log('💾 Using cached result');
    return cached;
  }

  const browser = await initBrowser();
  const pageObj = await createPage(browser);
  
  try {
    // Navigate to note page
    const noteUrl = `https://www.xiaohongshu.com/discovery/item/${noteId}`;
    console.log(`📄 Fetching note: ${noteId}`);
    
    await pageObj.goto(noteUrl, {
      waitUntil: 'networkidle',
      timeout: 30000
    });
    
    // Wait for content to load
    await delay(5000);
    
    // Check if login is required
    const needsLogin = await pageObj.evaluate(() => {
      const text = document.body.textContent || '';
      return text.includes('登录后查看') || text.includes('请登录');
    });
    
    if (needsLogin) {
      await browser.close();
      throw new Error('需要登录后才能查看笔记详情。请运行 "node scripts/get-cookie.js" 获取 Cookie，然后启用 config.json 中的 cookie.enabled');
    }
    
    // Extract detailed data from page
    const note = await pageObj.evaluate(() => {
      // Extract basic info
      const titleEl = document.querySelector('h1') || 
                      document.querySelector('[class*="title"]');
      
      const contentEl = document.querySelector('[class*="content"]') ||
                        document.querySelector('[class*="description"]');
      
      // Extract user info
      const userEl = document.querySelector('[class*="user-info"]') ||
                     document.querySelector('[class*="author"]');
      
      // Extract images
      const images = [];
      document.querySelectorAll('img[src]').forEach(img => {
        const src = img.src;
        if (src && (src.includes('xhscdn') || src.includes('xhs')) && !src.includes('avatar')) {
          images.push(src);
        }
      });
      
      return {
        note_id: noteId,
        title: titleEl?.textContent?.trim() || '无标题',
        content: contentEl?.textContent?.trim() || '',
        user: {
          nickname: userEl?.querySelector('[class*="nickname"]')?.textContent?.trim() || 
                    userEl?.textContent?.trim() || '',
          user_id: userEl?.getAttribute('data-user-id') || '',
          avatar: userEl?.querySelector('img')?.src || ''
        },
        images: images.slice(0, 20), // Limit to 20 images
        url: `https://www.xiaohongshu.com/discovery/item/${noteId}`,
        fetched_at: new Date().toISOString()
      };
    });
    
    await browser.close();
    
    // Save to cache
    saveCache(cacheKey, note);
    
    return note;
    
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
  const noteId = args[0];
  const format = args[1] === 'json' ? 'json' : 'text';
  
  if (!noteId || !noteId.startsWith('6')) {
    console.log(`
📄 获取笔记详情（需要登录）

用法：node scripts/get-note.js <笔记 ID> [格式]

参数:
  笔记 ID   小红书笔记 ID (通常是 13 位数字)
  格式      json|text (默认：text)

注意：需要登录才能查看笔记详情

示例:
  node scripts/get-note.js 64a1b2c3d4e5f
  node scripts/get-note.js 64a1b2c3d4e5f json
    `);
    process.exit(0);
  }
  
  try {
    const note = await getNoteDetail(noteId);
    
    if (format === 'json') {
      console.log(JSON.stringify(note, null, 2));
    } else {
      console.log(`\n📝 ${note.title}`);
      console.log(`   ID: ${note.note_id}`);
      console.log(`   👤 ${note.user.nickname || '未知'}`);
      console.log(`   🖼️ 图片：${note.images.length} 张`);
      console.log(`   \n内容:`);
      console.log(`   ${note.content.substring(0, 200) || '暂无内容'}...`);
      console.log(`   \n🔗 ${note.url}`);
    }
    
  } catch (error) {
    console.error('❌ 获取失败:', error.message);
    process.exit(1);
  }
}

main();
