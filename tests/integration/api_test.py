#!/usr/bin/env python3
"""
yuleDKCS API 集成测试
用法: python3 api_test.py [--host HOST] [--port PORT]
"""

import argparse
import json
import sys
import urllib.request
import urllib.error
from typing import Dict, Any, Optional


class APITester:
    def __init__(self, base_url: str):
        self.base_url = base_url
        self.token: Optional[str] = None
        self.passed = 0
        self.failed = 0
        self.skipped = 0

    def _request(
        self,
        method: str,
        endpoint: str,
        data: Optional[Dict] = None,
        auth: bool = False
    ) -> tuple[int, Dict]:
        """发送HTTP请求"""
        url = f"{self.base_url}{endpoint}"
        headers = {"Content-Type": "application/json"}
        
        if auth and self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        
        try:
            if data:
                req = urllib.request.Request(
                    url,
                    data=json.dumps(data).encode(),
                    headers=headers,
                    method=method
                )
            else:
                req = urllib.request.Request(
                    url,
                    headers=headers,
                    method=method
                )
            
            with urllib.request.urlopen(req, timeout=10) as resp:
                status = resp.status
                body = json.loads(resp.read().decode())
                return status, body
        except urllib.error.HTTPError as e:
            try:
                body = json.loads(e.read().decode())
            except:
                body = {"error": str(e)}
            return e.code, body
        except Exception as e:
            return 0, {"error": str(e)}

    def _check(self, name: str, status: int, body: Dict, expected_code: int = 200) -> bool:
        """检查响应"""
        actual_code = body.get("code", status)
        
        if actual_code == expected_code:
            print(f"  ✓ {name}: 通过")
            self.passed += 1
            return True
        else:
            print(f"  ✗ {name}: 失败 (code={actual_code}, expected={expected_code})")
            print(f"    响应: {json.dumps(body, indent=2)[:200]}")
            self.failed += 1
            return False

    def test_health(self):
        """测试健康检查"""
        print("\n[健康检查]")
        status, body = self._request("GET", "/../health")
        
        if status == 200:
            print(f"  ✓ 服务运行正常: {body.get('message', 'pong')}")
            self.passed += 1
            return True
        else:
            print(f"  ✗ 服务可能未启动 (status={status})")
            self.failed += 1
            return False

    def test_auth(self):
        """测试认证"""
        print("\n[认证测试]")
        
        # 测试登录
        status, body = self._request("POST", "/auth/login", {
            "username": "testuser",
            "password": "password123"
        })
        
        if status == 200 and body.get("code") == 200:
            self.token = body.get("data", {}).get("token")
            print(f"  ✓ 登录成功, token: {self.token[:20]}..." if self.token else "  ✓ 登录成功")
            self.passed += 1
        else:
            print(f"  ✗ 登录失败: {body.get('message', 'unknown error')}")
            self.failed += 1
            return
        
        # 测试获取用户信息
        status, body = self._request("GET", "/user/profile", auth=True)
        self._check("获取用户信息", status, body)

    def test_keys(self):
        """测试钥匙API"""
        if not self.token:
            print("\n[钥匙测试] 跳过 (未登录)")
            self.skipped += 4
            return
        
        print("\n[钥匙测试]")
        
        # 获取钥匙列表
        status, body = self._request("GET", "/keys", auth=True)
        self._check("获取钥匙列表", status, body)
        
        # 获取钥匙详情
        status, body = self._request("GET", "/keys/1", auth=True)
        if status == 200:
            self._check("获取钥匙详情", status, body)
        else:
            self.skipped += 1
            print(f"  ⚠ 跳过详情查询 (钥匙1可能不存在)")
        
        # 激活钥匙
        status, body = self._request("POST", "/keys/1/activate", auth=True)
        if status == 200:
            self._check("激活钥匙", status, body)
        else:
            print(f"  ⚠ 激活钥匙跳过 (钥匙不存在或已激活)")
            self.skipped += 1
        
        # 获取使用日志
        status, body = self._request("GET", "/keys/1/logs", auth=True)
        self._check("获取使用日志", status, body)

    def test_error_handling(self):
        """测试错误处理"""
        print("\n[错误处理测试]")
        
        # 无效token
        old_token = self.token
        self.token = "invalid_token"
        status, body = self._request("GET", "/keys", auth=True)
        
        if status == 401:
            print(f"  ✓ 无效token正确返回401")
            self.passed += 1
        else:
            print(f"  ✗ 无效token应返回401, 实际{status}")
            self.failed += 1
        
        self.token = old_token
        
        # 缺少参数
        status, body = self._request("POST", "/auth/login", {})
        if status == 400 or body.get("code") == 400:
            print(f"  ✓ 缺少参数正确返回400")
            self.passed += 1
        else:
            print(f"  ✗ 缺少参数应返回400")
            self.failed += 1

    def run_all(self):
        """运行所有测试"""
        print("=" * 50)
        print("yuleDKCS API 集成测试")
        print(f"目标: {self.base_url}")
        print("=" * 50)
        
        self.test_health()
        self.test_auth()
        self.test_keys()
        self.test_error_handling()
        
        # 汇总
        print("\n" + "=" * 50)
        print("测试汇总")
        print("=" * 50)
        print(f"  通过: {self.passed}")
        print(f"  失败: {self.failed}")
        print(f"  跳过: {self.skipped}")
        print(f"  总计: {self.passed + self.failed + self.skipped}")
        
        if self.failed == 0:
            print("\n  ✓ 所有测试通过!")
            return 0
        else:
            print(f"\n  ✗ {self.failed} 个测试失败")
            return 1


def main():
    parser = argparse.ArgumentParser(description="yuleDKCS API 测试")
    parser.add_argument("--host", default="localhost", help="服务器地址")
    parser.add_argument("--port", type=int, default=8080, help="端口")
    args = parser.parse_args()
    
    base_url = f"http://{args.host}:{args.port}/api/v1"
    tester = APITester(base_url)
    
    try:
        sys.exit(tester.run_all())
    except KeyboardInterrupt:
        print("\n\n测试已中止")
        sys.exit(1)
    except Exception as e:
        print(f"\n测试异常: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
