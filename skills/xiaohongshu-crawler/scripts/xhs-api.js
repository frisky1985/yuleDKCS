/**
 * 小红书 API 直接调用（不依赖浏览器）
 * 使用搜索 API 直接获取数据
 */

const axios = require('axios');

/**
 * 小红书搜索 API
 */
async function searchNotes(keyword, page = 1, limit = 20) {
  const baseUrl = 'https://www.xiaohongshu.com';
  
  // 搜索 API 端点
  const apiUrl = 'https://www.xiaohongshu.com/api/sns/web/v1/search/notes';
  
  const params = {
    keyword: keyword,
    page: page,
    page_size: limit,
    sort: 'general',
    note_type: 0
  };
  
  console.log(`🔍 搜索：${keyword} (API 模式)`);
  
  try {
    const response = await axios.get(apiUrl, {
      baseURL: baseUrl,
      params: params,
      headers: {
        'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
        'Accept': 'application/json, text/plain, */*',
        'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
        'Referer': 'https://www.xiaohongshu.com/',
        'x-net-type': 'wifi',
        'x-t': '1710000000000',
        'x-s': '', // 需要动态生成
        'x-ss-stamp': '' // 需要动态生成
      }
    });
    
    console.log('✅ 搜索成功');
    console.log('响应数据:', JSON.stringify(response.data, null, 2));
    
    return response.data;
    
  } catch (error) {
    console.error('❌ 搜索失败:', error.message);
    if (error.response) {
      console.error('状态码:', error.response.status);
      console.error('响应:', JSON.stringify(error.response.data, null, 2));
    }
    throw error;
  }
}

/**
 * 获取笔记详情（API 方式）
 */
async function getNoteDetail(noteId, xsecToken = '') {
  const apiUrl = `https://www.xiaohongshu.com/explore/${noteId}`;
  
  try {
    const response = await axios.get(apiUrl, {
      headers: {
        'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
      }
    });
    
    console.log('✅ 笔记详情获取成功');
    
    return response.data;
    
  } catch (error) {
    console.error('❌ 获取笔记详情失败:', error.message);
    throw error;
  }
}

module.exports = {
  searchNotes,
  getNoteDetail
};
