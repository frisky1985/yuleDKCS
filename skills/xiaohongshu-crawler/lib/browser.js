const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const antiCrawl = require('./anti-crawl');

// Load configuration
const configPath = path.join(__dirname, 'config.json');
let config = {};

if (fs.existsSync(configPath)) {
  config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
}

// Cache storage
const cacheDir = path.join(__dirname, '.cache');
if (!fs.existsSync(cacheDir)) {
  fs.mkdirSync(cacheDir, { recursive: true });
}

/**
 * Initialize Playwright browser
 */
async function initBrowser() {
  const browser = await chromium.launch({
    headless: config.playwright?.headless !== false,
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--disable-dev-shm-usage',
      '--disable-gpu',
      '--disable-blink-features=AutomationControlled'
    ]
  });
  return browser;
}

/**
 * Create a new page with consistent settings
 */
async function createPage(browser, options = {}) {
  // Prepare cookies if enabled
  let cookies = [];
  if (config.cookie?.enabled && Array.isArray(config.cookie.items)) {
    cookies = config.cookie.items.map(cookie => ({
      name: cookie.name,
      value: cookie.value,
      domain: cookie.domain || '.xiaohongshu.com',
      path: cookie.path || '/'
    }));
  }
  
  // Get random User-Agent
  const userAgent = antiCrawl.getRandomUserAgent();
  
  // Get proxy if enabled
  const proxy = await antiCrawl.getRandomProxy();
  
  const contextOptions = {
    userAgent: userAgent,
    viewport: { width: 1280, height: 720 },
    cookies: cookies.length > 0 ? cookies : undefined
  };
  
  if (proxy) {
    contextOptions.proxy = {
      server: proxy,
      username: proxy.username || undefined,
      password: proxy.password || undefined
    };
  }
  
  const context = await browser.newContext(contextOptions);
  
  const browserPage = await context.newPage();
  
  // Add stealth scripts to avoid detection
  await browserPage.addInitScript(() => {
    // 隐藏 WebDriver 特征
    Object.defineProperty(navigator, 'webdriver', {
      get: () => false
    });
    
    // 隐藏 plugins
    Object.defineProperty(navigator, 'plugins', {
      get: () => [1, 2, 3, 4, 5]
    });
    
    // 隐藏 languages
    Object.defineProperty(navigator, 'languages', {
      get: () => ['zh-CN', 'zh', 'en']
    });
  });
  
  // Set headers
  await browserPage.route('**/*', route => {
    route.continue();
  });
  
  return browserPage;
}

/**
 * Wait for API delay
 */
function delay(ms = 2000) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Check cache
 */
function checkCache(key) {
  const cacheFile = path.join(cacheDir, `${key}.json`);
  if (fs.existsSync(cacheFile)) {
    const data = JSON.parse(fs.readFileSync(cacheFile, 'utf8'));
    const age = Date.now() - data.timestamp;
    const maxAge = config.xhs?.cache_duration || 3600000;
    
    if (age < maxAge) {
      return data.data;
    }
  }
  return null;
}

/**
 * Save to cache
 */
function saveCache(key, data) {
  const cacheFile = path.join(cacheDir, `${key}.json`);
  fs.writeFileSync(cacheFile, JSON.stringify({
    timestamp: Date.now(),
    data: data
  }));
}

/**
 * Clean old cache files
 */
function cleanupCache() {
  const cacheFiles = fs.readdirSync(cacheDir);
  const maxAge = config.xhs?.cache_duration || 3600000;
  const now = Date.now();
  
  cacheFiles.forEach(file => {
    const filePath = path.join(cacheDir, file);
    const stat = fs.statSync(filePath);
    if (now - stat.mtimeMs > maxAge * 2) {
      fs.unlinkSync(filePath);
    }
  });
}

module.exports = {
  initBrowser,
  createPage,
  delay,
  checkCache,
  saveCache,
  cleanupCache,
  config
};
