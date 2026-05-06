/**
 * 重置请求计数器
 */

const antiCrawl = require('../lib/anti-crawl');

console.log('🔄 重置请求计数器...\n');

antiCrawl.resetCounters();

console.log('✅ 计数器已重置');
console.log('\n--- 当前统计 ---');
const stats = antiCrawl.getRequestStats();
console.log(`   本分钟：${stats.minute} 次请求`);
console.log(`   本小时：${stats.hour} 次请求`);
console.log(`   下次重置：${stats.minuteReset}\n`);

console.log('💡 提示：请求计数器会在每次请求时自动更新\n');
