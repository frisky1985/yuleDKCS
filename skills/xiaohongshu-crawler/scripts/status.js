/**
 * Cookie 管理和反爬状态检查工具
 */

const fs = require('fs');
const path = require('path');
const antiCrawl = require('../lib/anti-crawl');

const configPath = path.join(__dirname, '..', 'config.json');

function main() {
  console.log('🔧 Cookie 和反爬状态检查\n');
  
  // 读取配置
  let config;
  if (fs.existsSync(configPath)) {
    config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
    console.log('✅ 配置加载成功');
  } else {
    console.log('❌ 配置文件不存在，请运行：');
    console.log('   cp config.example.json config.json');
    process.exit(1);
  }
  
  console.log('\n--- Cookie 状态 ---');
  if (config.cookie?.enabled) {
    console.log('✅ Cookie 已启用');
    console.log(`📝 配置了 ${config.cookie.items.length} 个 Cookie`);
    
    if (config.cookie.items.length > 0) {
      const firstCookie = config.cookie.items[0];
      console.log(`   - ${firstCookie.name}: ${firstCookie.value ? '✅ 已设置' : '❌ 未设置'}`);
    }
  } else {
    console.log('⚠️  Cookie 未启用');
    console.log('💡 提示：运行 "node scripts/get-cookie.js" 获取 Cookie');
  }
  
  console.log('\n--- 反爬机制状态 ---');
  if (config.anti_crawl?.enabled) {
    console.log('✅ 反爬机制已启用');
    console.log('\n配置详情:');
    console.log(`   随机延迟：${config.anti_crawl.random_delay?.min}ms - ${config.anti_crawl.random_delay?.max}ms`);
    console.log(`   人类行为：${config.anti_crawl.human_behavior?.enabled ? '✅' : '❌'}`);
    console.log(`   请求限制：${config.anti_crawl.rate_limit?.max_requests_per_minute}/分`);
    console.log(`   请求限制：${config.anti_crawl.rate_limit?.max_requests_per_hour}/小时`);
  } else {
    console.log('⚠️  反爬机制未启用');
  }
  
  console.log('\n--- 当前请求统计 ---');
  const stats = antiCrawl.getRequestStats();
  console.log(`   本分钟：${stats.minute} 次请求`);
  console.log(`   本小时：${stats.hour} 次请求`);
  console.log(`   下次重置：${stats.minuteReset}`);
  
  console.log('\n--- 建议 ---');
  if (!config.cookie?.enabled) {
    console.log('1. 请先获取并配置 Cookie');
  }
  console.log('2. 建议每周检查一次 Cookie 是否有效');
  console.log('3. 如果被封禁，请检查请求频率和 IP');
  console.log('4. 可以启用代理轮换增加安全性');
  
  console.log('\n--- 快速命令 ---');
  console.log('获取 Cookie:      node scripts/get-cookie.js');
  console.log('测试 Cookie:      node scripts/test-cookie.js');
  console.log('查看统计:         node scripts/status.js');
  console.log('重置计数:         node scripts/reset-counters.js');
  
  process.exit(0);
}

main();
