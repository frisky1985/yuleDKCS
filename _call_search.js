// Build JSON manually to avoid shell escaping issues
const http = require('http');

const PROXY_PORT = process.env.AUTH_GATEWAY_PORT || '19000';
const PROXY_HOST = '127.0.0.1';
const API_PATH = '/proxy/prosearch/search';
const REQUEST_TIMEOUT = 10000;

const body = JSON.stringify({
  keyword: 'DeepSeek distillation arXiv 2026',
  cnt: 10
});

const req = http.request(
  {
    host: PROXY_HOST,
    port: Number(PROXY_PORT),
    path: API_PATH,
    method: 'POST',
    timeout: REQUEST_TIMEOUT,
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(body),
    },
  },
  (res) => {
    let data = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => { data += chunk; });
    res.on('end', () => { console.log(data); });
  }
);

req.on('timeout', () => {
  req.destroy();
  console.log(JSON.stringify({ success: false, message: 'Timeout' }));
  process.exit(1);
});
req.on('error', (err) => {
  console.log(JSON.stringify({ success: false, message: err.message }));
  process.exit(1);
});

req.write(body);
req.end();
