/**
 * 反爬机制核心模块
 * 
 * 功能：
 * 1. 随机延迟 - 模拟人类浏览速度
 * 2. 人类行为 - 随机滚动、鼠标移动
 * 3. 请求频率限制 - 防止过快请求
 * 4. 用户代理轮换 - 模拟不同浏览器
 * 5. 代理轮换 - 避免 IP 被封
 */

const config = require('../config.json');

// 请求计数
let requestCount = {
  minute: 0,
  hour: 0,
  minuteReset: Date.now(),
  hourReset: Date.now()
};

/**
 * 生成随机数
 */
function random(min, max) {
  return Math.random() * (max - min) + min;
}

/**
 * 生成随机整数
 */
function randomInt(min, max) {
  return Math.floor(random(min, max));
}

/**
 * 随机延迟（模拟人类操作）
 */
async function randomDelay(min = null, max = null) {
  const antiCrawl = config.anti_crawl;
  if (!antiCrawl?.enabled) return;
  
  const delays = antiCrawl.random_delay;
  const delayMs = random(min || delays.min, max || delays.max);
  
  await new Promise(resolve => setTimeout(resolve, delayMs));
}

/**
 * 模拟人类滚动行为
 */
async function simulateScroll(page) {
  if (!config.anti_crawl?.human_behavior?.enabled) return;
  if (!config.anti_crawl?.human_behavior?.random_scroll) return;
  
  const scrollTypes = ['top', 'middle', 'bottom', 'random'];
  const scrollType = scrollTypes[randomInt(0, scrollTypes.length)];
  
  switch (scrollType) {
    case 'top':
      await page.evaluate(() => window.scrollTo(0, 0));
      break;
    case 'bottom':
      await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight));
      break;
    case 'middle':
      await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight / 2));
      break;
    case 'random':
      const randomPos = randomInt(0, document.body.scrollHeight);
      await page.evaluate((pos) => window.scrollTo(0, pos), randomPos);
      break;
  }
  
  await randomDelay(500, 1500);
}

/**
 * 模拟鼠标移动（增加真实感）
 */
async function simulateMouseMovement(page) {
  if (!config.anti_crawl?.human_behavior?.enabled) return;
  if (!config.anti_crawl?.human_behavior?.mouse_movement) return;
  
  const startX = 100;
  const startY = 100;
  const endX = randomInt(100, 1200);
  const endY = randomInt(100, 800);
  const steps = randomInt(5, 10);
  
  let currentX = startX;
  let currentY = startY;
  
  for (let i = 0; i < steps; i++) {
    const nextX = randomInt(startX, endX);
    const nextY = randomInt(startY, endY);
    
    await page.mouse.move(nextX, nextY, { steps: 1 });
    await randomDelay(10, 50);
  }
}

/**
 * 检查请求频率限制
 */
function checkRateLimit() {
  const antiCrawl = config.anti_crawl;
  if (!antiCrawl?.enabled) return true;
  
  const now = Date.now();
  
  // 重置分钟计数
  if (now - requestCount.minuteReset > 60000) {
    requestCount.minute = 0;
    requestCount.minuteReset = now;
  }
  
  // 重置小时计数
  if (now - requestCount.hourReset > 3600000) {
    requestCount.hour = 0;
    requestCount.hourReset = now;
  }
  
  const rateLimit = antiCrawl.rate_limit;
  
  // 检查每分钟限制
  if (rateLimit.max_requests_per_minute && requestCount.minute >= rateLimit.max_requests_per_minute) {
    console.log(`⚠️  已达到每分钟请求限制 (${requestCount.minute}/${rateLimit.max_requests_per_minute})`);
    return false;
  }
  
  // 检查每小时限制
  if (rateLimit.max_requests_per_hour && requestCount.hour >= rateLimit.max_requests_per_hour) {
    console.log(`⚠️  已达到每小时请求限制 (${requestCount.hour}/${rateLimit.max_requests_per_hour})`);
    return false;
  }
  
  return true;
}

/**
 * 增加请求计数
 */
function incrementRequestCount() {
  const antiCrawl = config.anti_crawl;
  if (!antiCrawl?.enabled) return;
  
  requestCount.minute++;
  requestCount.hour++;
  
  const rateLimit = antiCrawl.rate_limit;
  console.log(`📊 请求计数：${requestCount.minute}/分 | ${requestCount.hour}/小时`);
}

/**
 * 生成随机 User-Agent
 */
function getRandomUserAgent() {
  const antiCrawl = config.anti_crawl;
  if (!antiCrawl?.user_agent_rotation?.enabled) {
    return config.playwright?.user_agent;
  }
  
  const userAgents = antiCrawl.user_agent_rotation.user_agents;
  if (userAgents && userAgents.length > 0) {
    return userAgents[randomInt(0, userAgents.length)];
  }
  
  // 默认 User-Agent 列表
  const defaultUAs = [
    'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15',
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0',
    'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36'
  ];
  
  return defaultUAs[randomInt(0, defaultUAs.length)];
}

/**
 * 获取随机代理
 */
async function getRandomProxy() {
  const antiCrawl = config.anti_crawl;
  if (!antiCrawl?.proxy_rotation?.enabled) return null;
  
  const proxies = antiCrawl.proxy_rotation.proxies;
  if (proxies && proxies.length > 0) {
    return proxies[randomInt(0, proxies.length)];
  }
  
  return null;
}

/**
 * 执行带反爬的请求
 */
async function makeRequest(page, url, options = {}) {
  const antiCrawl = config.anti_crawl;
  
  // 检查频率限制
  if (!checkRateLimit()) {
    throw new Error('请求频率超限，请稍后再试');
  }
  
  // 增加计数
  incrementRequestCount();
  
  // 随机延迟
  await randomDelay();
  
  // 模拟滚动
  await simulateScroll(page);
  
  // 模拟鼠标移动
  await simulateMouseMovement(page);
  
  // 访问页面
  const response = await page.goto(url, {
    waitUntil: 'networkidle',
    timeout: options.timeout || 30000
  });
  
  return response;
}

/**
 * 获取请求统计
 */
function getRequestStats() {
  return {
    minute: requestCount.minute,
    hour: requestCount.hour,
    minuteReset: new Date(requestCount.minuteReset).toISOString(),
    hourReset: new Date(requestCount.hourReset).toISOString()
  };
}

/**
 * 重置计数
 */
function resetCounters() {
  const now = Date.now();
  requestCount.minute = 0;
  requestCount.hour = 0;
  requestCount.minuteReset = now;
  requestCount.hourReset = now;
}

module.exports = {
  randomDelay,
  simulateScroll,
  simulateMouseMovement,
  checkRateLimit,
  incrementRequestCount,
  getRandomUserAgent,
  getRandomProxy,
  makeRequest,
  getRequestStats,
  resetCounters
};
