const http = require('http');
const PROXY_PORT = process.env.AUTH_GATEWAY_PORT || '19000';
const API_PATH = '/proxy/prosearch/search';
const REQUEST_TIMEOUT = 10000;

const params = { keyword: "最新科技新闻", cnt: 15 };
const body = JSON.stringify(params);

const req = http.request(
  {
    host: '127.0.0.1',
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

req.on('timeout', () => { req.destroy(); console.log(JSON.stringify({success:false,message:'timeout'})); });
req.on('error', (err) => { console.log(JSON.stringify({success:false,message:err.message})); });
req.write(body);
req.end();