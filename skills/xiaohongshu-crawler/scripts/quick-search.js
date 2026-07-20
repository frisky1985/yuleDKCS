const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const config = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'config.json'), 'utf8'));

/**
 * 直接搜索小红书笔记
 */
async function main() {
  const keyword = process.argv[2] || '四川旅游';
  const maxResults = parseInt(process.argv[3]) || 30;
  
  console.log('🔍 小红书搜索工具\n');
  console.log('═══════════════════════════════════════════');
  console.log(`搜索关键词：${keyword}`);
  console.log(`最大结果数：${maxResults}\n`);
  
  let browser;
  let results = [];
  
  try {
    browser = await chromium.launch({
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
      console.log(`✅ 已加载 ${cookies.length} 个 Cookie\n`);
    }
    
    const page = await context.newPage();
    
    // 设置反检测
    await page.addInitScript(() => {
      Object.defineProperty(navigator, 'webdriver', { get: () => false });
      Object.defineProperty(navigator, 'plugins', { get: () => [1, 2, 3, 4, 5] });
      Object.defineProperty(navigator, 'languages', { get: () => ['zh-CN', 'zh', 'en'] });
    });
    
    // 构建搜索 URL
    const searchUrl = `https://www.xiaohongshu.com/search?keyword=${encodeURIComponent(keyword)}&source=web_search_result_notes&page=1&search_type=note`;
    
    console.log('🌐 访问搜索页面...');
    await page.goto(searchUrl, { waitUntil: 'networkidle', timeout: 30000 });
    
    // 等待页面加载
    await page.waitForLoadState('domcontentloaded');
    await new Promise(resolve => setTimeout(resolve, 5000));
    
    // 检查登录状态
    const loginStatus = await page.evaluate(() => {
      return {
        url: window.location.href,
        hasSearchResults: !!document.querySelector('.note-item, .search-result-item, .小红薯')
      };
    });
    
    console.log(`📍 当前 URL: ${loginStatus.url}`);
    
    if (loginStatus.url.includes('login') || loginStatus.url.includes('/login')) {
      console.log('\n❌ 需要登录！请先登录小红书');
      await page.screenshot({ path: 'login-required.png' });
      return;
    }
    
    // 提取搜索结果
    console.log('\n📊 正在提取搜索结果...\n');
    console.log('═══════════════════════════════════════════\n');
    
    results = await page.evaluate(() => {
      const items = [];
      
      // 尝试多种选择器
      const selectors = [
        '.note-item',
        '.search-result-item',
        '[class*="note-item"]',
        '[class*="search-result"]',
        '.小红薯',
        'article'
      ];
      
      for (const selector of selectors) {
        const elements = document.querySelectorAll(selector);
        if (elements.length > 0) {
          elements.forEach((el, index) => {
            try {
              const titleEl = el.querySelector('h3, .title, [class*="title"], a');
              const authorEl = el.querySelector('.user-name, .nickname, [class*="user"], [class*="author"]');
              const likeEl = el.querySelector('.like-count, .likes, [class*="like"]');
              const coverEl = el.querySelector('img');
              
              items.push({
                index: items.length + 1,
                title: titleEl?.innerText?.trim() || titleEl?.textContent?.trim() || '无标题',
                author: authorEl?.innerText?.trim() || authorEl?.textContent?.trim() || '未知作者',
                likes: likeEl?.innerText?.trim() || '未知',
                cover: coverEl?.src || coverEl?.getAttribute('data-src') || '',
                url: titleEl?.href || ''
              });
            } catch (e) {
              // 忽略单个项目错误
            }
          });
          break;
        }
      }
      
      return items;
    });
    
    if (results.length === 0) {
      console.log('⚠️  未找到搜索结果，可能原因:');
      console.log('1. 搜索关键词过于特殊');
      console.log('2. 小红书风控拦截');
      console.log('3. 网络连接问题\n');
      
      // 截图保存
      await page.screenshot({ path: 'search-result.png', fullPage: true });
      console.log('📸 页面截图已保存到 search-result.png');
      return;
    }
    
    // 显示结果
    results.slice(0, maxResults).forEach(r => {
      console.log(`[${r.index}] ${r.title}`);
      console.log(`   👤 ${r.author} | ❤️ ${r.likes}`);
      if (r.url) {
        console.log(`   🔗 ${r.url}`);
      }
      console.log('');
    });
    
    console.log(`═══════════════════════════════════════════`);
    console.log(`✅ 共找到 ${results.length} 条结果（显示前 ${Math.min(results.length, maxResults)} 条）\n`);
    
    // 保存结果到文件
    const outputPath = path.join(__dirname, '..', `sichuan-travel-${Date.now()}.json`);
    fs.writeFileSync(outputPath, JSON.stringify(results, null, 2), 'utf8');
    console.log(`💾 结果已保存到：${outputPath}\n`);
    
    // 截图保存
    await page.screenshot({ path: 'search-result.png', fullPage: true });
    console.log('📸 页面截图已保存到 search-result.png\n');
    
  } catch (error) {
    console.error('\n❌ 错误:', error.message);
    if (browser) {
      await page?.screenshot({ path: 'error.png' });
      console.log('📸 错误截图已保存到 error.png');
    }
  } finally {
    if (browser) {
      console.log('\n👀 5 秒后关闭浏览器...');
      await new Promise(resolve => setTimeout(resolve, 5000));
      await browser.close();
    }
  }
}

main().catch(err => {
  console.error('❌ 错误:', err);
  process.exit(1);
});
