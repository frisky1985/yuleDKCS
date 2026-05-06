const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

/**
 * 小红书 Cookie 获取工具 (最终版)
 * 
 * 使用方法:
 * 1. node scripts/get-cookie.js
 * 2. 在打开的浏览器中登录小红书
 * 3. 登录成功后，在终端输入 "save" 并回车
 * 4. Cookie 会自动保存到 config.json
 */

async function main() {
  console.log('🔑 小红书 Cookie 获取工具\n');
  console.log('═══════════════════════════════════════════');
  console.log('使用说明:');
  console.log('1. 浏览器会打开小红书登录页');
  console.log('2. 请手动登录你的账号');
  console.log('3. 登录成功后，在终端输入 "save" 并回车');
  console.log('4. Cookie 将自动保存到 config.json\n');
  
  const browser = await chromium.launch({
    headless: false,
    args: ['--no-sandbox', '--disable-setuid-sandbox']
  });
  
  const context = await browser.newContext({
    userAgent: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    viewport: { width: 1280, height: 720 }
  });
  
  const page = await context.newPage();
  
  await page.goto('https://www.xiaohongshu.com', {
    waitUntil: 'networkidle',
    timeout: 30000
  });
  
  console.log('✅ 已打开小红书主页');
  console.log('👉 请在浏览器中登录账号...\n');
  console.log('💡 登录完成后，在终端输入 "save" 并回车\n');
  
  // 监听终端输入
  process.stdin.setEncoding('utf8');
  process.stdin.resume();
  
  const exitPromise = new Promise((resolve) => {
    process.on('SIGINT', () => {
      console.log('\n\n👋 已取消，退出。');
      resolve(false);
    });
  });
  
  const savePromise = new Promise(async (resolve) => {
    process.stdin.on('data', async (input) => {
      const cmd = input.trim().toLowerCase();
      if (cmd === 'save' || cmd === 'y' || cmd === 'yes') {
        resolve(true);
      }
    });
  });
  
  const shouldSave = await Promise.race([
    savePromise,
    exitPromise
  ]);
  
  if (shouldSave) {
    try {
      console.log('\n📋 正在导出 Cookie...\n');
      
      // 获取所有 Cookie
      const cookies = await context.cookies();
      
      // 过滤出关键 Cookie
      const importantCookies = cookies.filter(c => 
        c.name.includes('web_session') ||
        c.name.includes('XSRF-TOKEN') ||
        c.name.includes('id_token') ||
        c.name.includes('login_token') ||
        c.name.includes('acw_tc') ||
        c.name.includes('webId') ||
        c.name.includes('websectiga') ||
        c.name.includes('gid') ||
        c.name.includes('abRequestId') ||
        c.name.includes('sec_poison_id') ||
        c.name.includes('loadts') ||
        c.name.includes('webBuild') ||
        c.name.includes('xsecappid') ||
        c.name.includes('unread') ||
        c.name.includes('a1')
      );
      
      if (importantCookies.length === 0) {
        console.log('⚠️  未找到关键 Cookie，请先登录小红书');
        await browser.close();
        process.exit(1);
      }
      
      // 构建配置文件内容
      const configPath = path.join(__dirname, '..', 'config.json');
      const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
      
      config.cookie = {
        enabled: true,
        items: importantCookies.map(c => ({
          name: c.name,
          value: c.value,
          domain: c.domain,
          path: c.path
        }))
      };
      
      // 保存配置
      fs.writeFileSync(configPath, JSON.stringify(config, null, 2), 'utf8');
      
      console.log('🎉 成功保存 Cookie!\n');
      console.log('📝 保存了以下 Cookie:');
      importantCookies.forEach(c => {
        console.log(`  ✓ ${c.name}`);
      });
      
      console.log('\n💾 已自动更新 config.json\n');
      console.log('现在可以运行测试脚本来验证 Cookie 是否有效：');
      console.log('  node scripts/test-cookie.js\n');
      
    } catch (error) {
      console.error('❌ 保存失败:', error.message);
      console.error(error.stack);
    } finally {
      await browser.close();
      process.exit(0);
    }
  } else {
    await browser.close();
    process.exit(0);
  }
}

main().catch(err => {
  console.error('❌ 错误:', err.message);
  console.error(err.stack);
  process.exit(1);
});
