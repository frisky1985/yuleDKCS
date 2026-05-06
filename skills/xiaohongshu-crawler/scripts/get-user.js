const { initBrowser, createPage, delay, checkCache, saveCache, cleanupCache } = require('../lib/browser');
const path = require('path');

/**
 * Get user info (requires login)
 */
async function getUserInfo(userId, limit = 10) {
  const cacheKey = `user_${userId}`;
  const cached = checkCache(cacheKey);
  if (cached && cached.user_id) {
    console.log('💾 Using cached result');
    return cached;
  }

  const browser = await initBrowser();
  const pageObj = await createPage(browser);
  
  try {
    // Navigate to user profile page
    const userProfileUrl = `https://www.xiaohongshu.com/user/profile/${userId}`;
    console.log(`👤 Fetching user: ${userId}`);
    
    await pageObj.goto(userProfileUrl, {
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
      throw new Error('需要登录后才能查看用户信息。请运行 "node scripts/get-cookie.js" 获取 Cookie，然后启用 config.json 中的 cookie.enabled');
    }
    
    // Extract user data from page
    const userData = await pageObj.evaluate((limitVal) => {
      const userEl = document.querySelector('[class*="user-info"]') || document.querySelector('[data-e2e="user-card"]');
      
      // Extract basic user info
      const basicInfo = {
        user_id: userId,
        nickname: userEl?.querySelector('[class*="nickname"]')?.textContent?.trim() || '',
        avatar: userEl?.querySelector('img')?.src || '',
        desc: userEl?.querySelector('[class*="desc"]')?.textContent?.trim() || '',
        verified: !!userEl?.querySelector('[class*="verified"]')
      };
      
      // Extract stats
      const statsEl = document.querySelectorAll('[class*="stats"]');
      const stats = {
        followers: 0,
        following: 0,
        notes: 0,
        likes: 0
      };
      
      statsEl.forEach(el => {
        const count = parseInt(el.textContent?.trim().replace(/,/g, '')) || 0;
        if (el.textContent?.includes('关注')) stats.following = count;
        else if (el.textContent?.includes('粉丝')) stats.followers = count;
        else if (el.textContent?.includes('笔记')) stats.notes = count;
        else if (el.textContent?.includes('获赞')) stats.likes = count;
      });
      
      // Extract user's recent notes
      const notes = [];
      const noteCards = document.querySelectorAll('[class*="note-card"]');
      
      noteCards.slice(0, limitVal).forEach(card => {
        notes.push({
          note_id: card.getAttribute('data-note-id') || card.querySelector('[class*="note-id"]')?.textContent?.trim(),
          title: card.querySelector('[class*="title"]')?.textContent?.trim() || '无标题',
          cover: card.querySelector('img')?.src || '',
          likes: card.querySelector('[class*="like-count"]')?.textContent?.trim() || '0',
          comments: card.querySelector('[class*="comment-count"]')?.textContent?.trim() || '0'
        });
      });
      
      return {
        ...basicInfo,
        stats,
        recent_notes: notes,
        url: `https://www.xiaohongshu.com/user/profile/${userId}`,
        fetched_at: new Date().toISOString()
      };
    }, limit);
    
    await browser.close();
    
    // Save to cache
    saveCache(cacheKey, userData);
    
    return userData;
    
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
  const userId = args[0];
  const limit = parseInt(args[1]) || 10;
  const format = args[2] === 'json' ? 'json' : 'text';
  
  if (!userId) {
    console.log(`
👤 获取用户信息（需要登录）

用法：node scripts/get-user.js <用户 ID> [数量] [格式]

参数:
  用户 ID   小红书用户 ID
  数量      返回最近笔记数量 (默认：10)
  格式      json|text (默认：text)

注意：需要登录才能查看用户信息

示例:
  node scripts/get-user.js 5f6g7h8i9j0k1l2m3n4o5p6q
  node scripts/get-user.js 5f6g7h8i9j0k1l2m3n4o5p6q 20 json
    `);
    process.exit(0);
  }
  
  try {
    const userData = await getUserInfo(userId, limit);
    
    if (format === 'json') {
      console.log(JSON.stringify(userData, null, 2));
    } else {
      console.log(`\n👤 ${userData.nickname} ${userData.verified ? '✅' : ''}`);
      console.log(`   ID: ${userData.user_id}`);
      console.log(`   📊 粉丝：${userData.stats.followers} | 关注：${userData.stats.following} | 笔记：${userData.stats.notes} | 获赞：${userData.stats.likes}`);
      console.log(`   📝 ${userData.recent_notes.length} 篇最近笔记`);
      
      if (userData.desc) {
        console.log(`   💬 简介：${userData.desc}`);
      }
      
      console.log(`   \n🔗 ${userData.url}`);
    }
    
  } catch (error) {
    console.error('❌ 获取失败:', error.message);
    process.exit(1);
  }
}

main();
