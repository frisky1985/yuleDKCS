const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const config = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'config.json'), 'utf8'));

/**
 * 爬取笔记详细内容
 */
async function getNoteDetail(noteUrl) {
  if (!noteUrl) return null;
  
  const browser = await chromium.launch({
    headless: false,
    args: ['--no-sandbox', '--disable-setuid-sandbox']
  });
  
  const context = await browser.newContext({
    userAgent: config.playwright?.user_agent || 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    viewport: { width: 1280, height: 720 }
  });
  
  // 添加 Cookie
  if (config.cookie?.enabled && config.cookie.items) {
    const cookies = config.cookie.items.map(c => ({
      name: c.name,
      value: c.value,
      domain: c.domain || '.xiaohongshu.com',
      path: c.path || '/',
      secure: true,
      sameSite: 'None'
    }));
    await context.addCookies(cookies);
  }
  
  const page = await context.newPage();
  
  try {
    await page.goto(noteUrl, { waitUntil: 'networkidle', timeout: 30000 });
    await new Promise(resolve => setTimeout(resolve, 3000));
    
    const detail = await page.evaluate(() => {
      const titleEl = document.querySelector('h1, .note-title, [class*="note-title"]');
      const authorEl = document.querySelector('.user-info .nickname, .author-name, [class*="author"]');
      const likeEl = document.querySelector('.like-btn .like-count, .like-number, [class*="like-count"]');
      const collectEl = document.querySelector('.collect-btn, .collect-number, [class*="collect"]');
      const contentEl = document.querySelector('.note-content, .content, [class*="content"]');
      const images = Array.from(document.querySelectorAll('.note-image img, .content img')).map(img => img.src).slice(0, 5);
      
      return {
        title: titleEl?.innerText?.trim() || '',
        author: authorEl?.innerText?.trim() || '',
        likes: likeEl?.innerText?.trim() || '未知',
        collects: collectEl?.innerText?.trim() || '未知',
        content: contentEl?.innerText?.trim()?.slice(0, 500) || '',
        images: images
      };
    });
    
    await browser.close();
    return detail;
    
  } catch (error) {
    console.error(`❌ 爬取失败: ${noteUrl}`);
    await browser.close();
    return null;
  }
}

async function main() {
  const keyword = process.argv[2] || '四川旅游';
  const maxResults = parseInt(process.argv[3]) || 20;
  
  console.log('🔍 小红书深度爬取工具\n');
  console.log('═══════════════════════════════════════════');
  console.log(`搜索关键词：${keyword}`);
  console.log(`最大结果数：${maxResults}\n`);
  
  // 先搜索
  const browser = await chromium.launch({
    headless: false,
    args: ['--no-sandbox', '--disable-setuid-sandbox']
  });
  
  const context = await browser.newContext({
    userAgent: config.playwright?.user_agent || 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    viewport: { width: 1280, height: 720 }
  });
  
  if (config.cookie?.enabled && config.cookie.items) {
    const cookies = config.cookie.items.map(c => ({
      name: c.name,
      value: c.value,
      domain: c.domain || '.xiaohongshu.com',
      path: c.path || '/',
      secure: true,
      sameSite: 'None'
    }));
    await context.addCookies(cookies);
  }
  
  const page = await context.newPage();
  
  // 设置反检测
  await page.addInitScript(() => {
    Object.defineProperty(navigator, 'webdriver', { get: () => false });
    Object.defineProperty(navigator, 'plugins', { get: () => [1, 2, 3, 4, 5] });
    Object.defineProperty(navigator, 'languages', { get: () => ['zh-CN', 'zh', 'en'] });
  });
  
  const searchUrl = `https://www.xiaohongshu.com/search?keyword=${encodeURIComponent(keyword)}&source=web_search_result_notes&page=1&search_type=note`;
  
  console.log('🌐 访问搜索页面...');
  await page.goto(searchUrl, { waitUntil: 'networkidle', timeout: 30000 });
  await new Promise(resolve => setTimeout(resolve, 5000));
  
  console.log('📊 提取搜索结果...');
  
  // 提取搜索结果
  const results = await page.evaluate(() => {
    const items = [];
    const elements = document.querySelectorAll('.note-item, .search-result-item, [class*="note-item"]');
    
    elements.forEach((el, index) => {
      try {
        const titleEl = el.querySelector('h3, .title, [class*="title"], a');
        const authorEl = el.querySelector('.user-name, .nickname, [class*="user"], [class*="author"]');
        const likeEl = el.querySelector('.like-count, .likes, [class*="like"]');
        const coverEl = el.querySelector('img');
        
        let url = titleEl?.href || '';
        if (url && !url.startsWith('http')) {
          url = 'https://www.xiaohongshu.com' + url;
        }
        
        items.push({
          index: items.length + 1,
          title: titleEl?.innerText?.trim() || titleEl?.textContent?.trim() || '无标题',
          author: authorEl?.innerText?.trim() || authorEl?.textContent?.trim() || '未知作者',
          likes: likeEl?.innerText?.trim() || '未知',
          cover: coverEl?.src || coverEl?.getAttribute('data-src') || '',
          url: url
        });
      } catch (e) {
        // 忽略
      }
    });
    
    return items;
  });
  
  console.log(`✅ 找到 ${results.length} 条笔记\n`);
  
  // 保存原始搜索结果
  const resultsPath = path.join(__dirname, '..', `sichuan-travel-results.json`);
  fs.writeFileSync(resultsPath, JSON.stringify(results, null, 2), 'utf8');
  console.log(`💾 搜索结果已保存到：${resultsPath}\n`);
  
  // 爬取详细内容
  console.log('═══════════════════════════════════════════');
  console.log('📖 开始爬取详细内容...\n');
  
  const detailedResults = [];
  
  for (let i = 0; i < Math.min(results.length, maxResults); i++) {
    const note = results[i];
    console.log(`[${i + 1}/${Math.min(results.length, maxResults)}] 爬取：${note.title.slice(0, 30)}...`);
    
    // 延迟
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    if (note.url) {
      const detail = await getNoteDetail(note.url);
      if (detail) {
        detailedResults.push({
          ...note,
          ...detail
        });
        console.log(`  ✅ 成功`);
      } else {
        console.log(`  ❌ 失败`);
      }
    } else {
      detailedResults.push(note);
      console.log(`  ⚠️  无 URL`);
    }
  }
  
  // 保存到文件
  const detailPath = path.join(__dirname, '..', `sichuan-travel-detailed.json`);
  fs.writeFileSync(detailPath, JSON.stringify(detailedResults, null, 2), 'utf8');
  console.log(`\n💾 详细内容已保存到：${detailPath}\n`);
  
  // 截图
  await page.screenshot({ path: 'search-detail.png', fullPage: true });
  console.log('📸 截图已保存到 search-detail.png\n');
  
  await browser.close();
  
  console.log('═══════════════════════════════════════════');
  console.log('✅ 爬取完成！');
  console.log(`📊 成功爬取 ${detailedResults.length} 条笔记的详细信息\n`);
}

main().catch(err => {
  console.error('❌ 错误:', err);
  process.exit(1);
});
