# BER-TLV 三端兼容性测试套件

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **测试范围**: C车端 ↔ Java云端 ↔ Swift/TS手机端

---

## 1. 测试架构

```
┌────────────────────────────────────────────────────────────────────────┐
│              TLV Interoperability Test Architecture               │
├────────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐      │
│  │                    测试协调器                                │      │
│  ├───────────────────────────────────────────────────────────────────────┤      │
│  │  • 测试用例生成 (金丝集覆盖)                              │      │
│  │  • 消息路由 (A编码→B解码)                                  │      │
│  │  • 结果验证 (位级对比)                                        │      │
│  │  • 报告生成 (HTML/JSON)                                        │      │
│  └───────────────────────────────────────────────────────────────────────┘      │
│                           │                                         │
│         ┌────────────────´───├────────────────´───├────────────────┐        │
│           ▼                 ▼                 ▼                 │
│  ┌───────────────────────────────────────────────────────────────────────┐      │
│  │   C Encoder/Decoder (Vehicle T-Box)    │      │                   │      │
│  ├───────────────────────────────────────────────────────────────────────┤      │
│  │  • ARM Cortex-A53 (64-bit)                                     │      │
│  │  • GCC 11.3 + O2优化                                           │      │
│  │  • 固件版本: v2.1.0                                              │      │
│  └───────────────────────────────────────────────────────────────────────┘      │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐      │
│  │  Java Encoder/Decoder (Cloud/Backend)   │      │                   │      │
│  ├───────────────────────────────────────────────────────────────────────┤      │
│  │  • OpenJDK 17 LTS                                              │      │
│  │  • Spring Boot 3.2                                             │      │
│  │  • 服务版本: v1.2.0                                               │      │
│  └───────────────────────────────────────────────────────────────────────┘      │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────────────┐      │
│  │  Swift/TS Encoder/Decoder (Mobile SDK)  │      │                   │      │
│  ├───────────────────────────────────────────────────────────────────────┤      │
│  │  • Swift 5.9 / TypeScript 5.0                                  │      │
│  │  • iOS 16+ / Android 14+                                       │      │
│  │  • SDK版本: v1.2.0                                               │      │
│  └───────────────────────────────────────────────────────────────────────┘      │
│                                                                      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 测试用例设计

### 2.1 金丝集覆盖策略

```python
#!/usr/bin/env python3
"""
TLV互操作性测试套件
验证所有端之间的消息互通
"""

import json
import subprocess
import tempfile
import os
from dataclasses import dataclass
from typing import List, Tuple, Optional
from enum import Enum


class Platform(Enum):
    """平台类型"""
    VEHICLE_C = "vehicle_c"
    CLOUD_JAVA = "cloud_java"
    MOBILE_SWIFT = "mobile_swift"
    MOBILE_TYPESCRIPT = "mobile_typescript"


@dataclass
class TestCase:
    """测试用例定义"""
    id: str
    name: str
    description: str
    message_type: str
    source_platform: Platform
    target_platforms: List[Platform]
    input_data: dict
    expected_fields: List[str]
    priority: str  # P0, P1, P2


# 核心测试用例库
INTEROP_TEST_CASES = [
    # ========== P0: 关键路径 ==========
    TestCase(
        id="INT-001",
        name="车端心跳→云端解码",
        description="验证C编码的心跳消息能被Java正确解码",
        message_type="HEARTBEAT",
        source_platform=Platform.VEHICLE_C,
        target_platforms=[Platform.CLOUD_JAVA],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "HEARTBEAT",
            "message_id": "a1b2c3d4e5f678901234567890123456",
            "timestamp": 1715432100123,
            "uptime": 86400000,
            "signal_strength": 85,
            "battery_level": 78
        },
        expected_fields=["protocol_version", "message_type", "message_id", "timestamp"],
        priority="P0"
    ),
    
    TestCase(
        id="INT-002",
        name="云端指令→车端解码",
        description="验证Java编码的控制指令能被C正确执行",
        message_type="COMMAND",
        source_platform=Platform.CLOUD_JAVA,
        target_platforms=[Platform.VEHICLE_C],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "COMMAND",
            "command_type": "DOOR_LOCK",
            "target_vin": "LSVNV2182E2100001",
            "parameters": {"lock_all": True}
        },
        expected_fields=["command_type", "target_vin", "parameters"],
        priority="P0"
    ),
    
    TestCase(
        id="INT-003",
        name="手机请求→云端→车端",
        description="验证Swift/TS消息经云端转发到车端",
        message_type="REQUEST",
        source_platform=Platform.MOBILE_SWIFT,
        target_platforms=[Platform.CLOUD_JAVA, Platform.VEHICLE_C],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "REQUEST",
            "request_type": "VEHICLE_STATUS",
            "user_id": "user_12345",
            "auth_token": "Bearer eyJhbGciOiJSUzI1Ni..."
        },
        expected_fields=["request_type", "user_id"],
        priority="P0"
    ),
    
    # ========== P1: 边界条件 ==========
    TestCase(
        id="INT-010",
        name="最大消息大小",
        description="验证4KB边界消息的传输",
        message_type="OTA_PACKAGE",
        source_platform=Platform.CLOUD_JAVA,
        target_platforms=[Platform.VEHICLE_C],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "OTA_RESPONSE",
            "package_data": "x" * 4090  # 接近4KB限制
        },
        expected_fields=["message_type", "package_data"],
        priority="P1"
    ),
    
    TestCase(
        id="INT-011",
        name="空字符串传输",
        description="验证各端对空字符串的处理一致性",
        message_type="NOTIFICATION",
        source_platform=Platform.VEHICLE_C,
        target_platforms=[Platform.CLOUD_JAVA, Platform.MOBILE_TYPESCRIPT],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "NOTIFICATION",
            "title": "",
            "content": "Test\x00Null\x00Byte",  # 包含null字节
            "empty_field": ""
        },
        expected_fields=["title", "content"],
        priority="P1"
    ),
    
    TestCase(
        id="INT-012",
        name="Unicode字符编码",
        description="验证UTF-8编码一致性 (中文、emoji、特殊符号)",
        message_type="NOTIFICATION",
        source_platform=Platform.CLOUD_JAVA,
        target_platforms=[Platform.MOBILE_SWIFT],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "NOTIFICATION",
            "title": "车辆报警 🚨",
            "content": "发动机温度过高！Temperature: 120°C ⚠️",
            "unicode_test": "あいうえお हिन्दी العربية"
        },
        expected_fields=["title", "content"],
        priority="P1"
    ),
    
    # ========== P2: 容错场景 ==========
    TestCase(
        id="INT-020",
        name="版本协商降级",
        description="验证新版客户端与旧版服务端的兼容性",
        message_type="VERSION_NEGOTIATION",
        source_platform=Platform.MOBILE_TYPESCRIPT,
        target_platforms=[Platform.CLOUD_JAVA],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "supported_versions": ["1.2.0", "1.1.0", "1.0.0"],
            "preferred_version": "1.2.0"
        },
        expected_fields=["negotiated_version"],
        priority="P2"
    ),
    
    TestCase(
        id="INT-021",
        name="时间戳时区处理",
        description="验证各端对UTC和本地时间的处理",
        message_type="HEARTBEAT",
        source_platform=Platform.VEHICLE_C,
        target_platforms=[Platform.CLOUD_JAVA],
        input_data={
            "protocol_version": {"major": 1, "minor": 2, "patch": 0},
            "message_type": "HEARTBEAT",
            "timestamp": 1715432100123,  # UTC时间戳
            "timezone_offset": 480  # 东八区
        },
        expected_fields=["timestamp", "timezone_offset"],
        priority="P2"
    ),
]


class InteropTestRunner:
    """互操作性测试执行器"""
    
    def __init__(self, implementations_dir: str):
        self.implementations_dir = implementations_dir
        self.results = []
    
    def run_test(self, test_case: TestCase) -> dict:
        """执行单个测试用例"""
        print(f"\n─── Running {test_case.id}: {test_case.name} ───")
        
        result = {
            "test_id": test_case.id,
            "name": test_case.name,
            "priority": test_case.priority,
            "source": test_case.source_platform.value,
            "targets": [p.value for p in test_case.target_platforms],
            "steps": [],
            "passed": True,
            "errors": []
        }
        
        try:
            # 步骤1: 在源平台编码
            encoded = self._encode_on_platform(
                test_case.source_platform,
                test_case.input_data
            )
            result["steps"].append({
                "step": 1,
                "action": f"Encode on {test_case.source_platform.value}",
                "status": "PASS",
                "hex_preview": encoded[:64] + "..." if len(encoded) > 64 else encoded
            })
            
            # 步骤2: 在目标平台解码验证
            for target in test_case.target_platforms:
                decoded = self._decode_on_platform(target, encoded)
                validation = self._validate_decoded(decoded, test_case.expected_fields)
                
                step_result = {
                    "step": 2,
                    "action": f"Decode on {target.value}",
                    "status": "PASS" if validation["valid"] else "FAIL",
                    "details": validation
                }
                result["steps"].append(step_result)
                
                if not validation["valid"]:
                    result["passed"] = False
                    result["errors"].append(f"{target.value}: {validation['errors']}")
            
            # 步骤3: 字段值完全一致性验证
            if result["passed"]:
                consistency = self._verify_field_consistency(
                    test_case.source_platform,
                    test_case.target_platforms[-1],
                    test_case.input_data
                )
                result["steps"].append({
                    "step": 3,
                    "action": "Field consistency check",
                    "status": "PASS" if consistency else "FAIL"
                })
                if not consistency:
                    result["passed"] = False
        
        except Exception as e:
            result["passed"] = False
            result["errors"].append(str(e))
        
        self.results.append(result)
        return result
    
    def _encode_on_platform(self, platform: Platform, data: dict) -> str:
        """在指定平台编码"""
        # 写入测试数据
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(data, f)
            input_file = f.name
        
        # 调用平台编码器
        encoder_path = self._get_encoder_path(platform)
        result = subprocess.run(
            [encoder_path, input_file],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        os.unlink(input_file)
        
        if result.returncode != 0:
            raise RuntimeError(f"Encoding failed on {platform}: {result.stderr}")
        
        return result.stdout.strip()
    
    def _decode_on_platform(self, platform: Platform, hex_data: str) -> dict:
        """在指定平台解码"""
        decoder_path = self._get_decoder_path(platform)
        result = subprocess.run(
            [decoder_path, hex_data],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        if result.returncode != 0:
            raise RuntimeError(f"Decoding failed on {platform}: {result.stderr}")
        
        return json.loads(result.stdout)
    
    def _validate_decoded(self, decoded: dict, expected_fields: List[str]) -> dict:
        """验证解码结果"""
        errors = []
        
        for field in expected_fields:
            if field not in decoded:
                errors.append(f"Missing field: {field}")
        
        # 验证必需TAG
        if "protocol_version" not in decoded:
            errors.append("Missing protocol_version")
        
        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "decoded_fields": list(decoded.keys())
        }
    
    def _verify_field_consistency(self, source: Platform, target: Platform, 
                                   original: dict) -> bool:
        """验证字段值在编码解码后保持一致"""
        # 重新编码并解码，验证数据完整性
        encoded = self._encode_on_platform(source, original)
        decoded = self._decode_on_platform(target, encoded)
        
        # 对比关键字段
        for key in original:
            if key in decoded:
                if original[key] != decoded[key]:
                    return False
        
        return True
    
    def _get_encoder_path(self, platform: Platform) -> str:
        """获取编码器路径"""
        paths = {
            Platform.VEHICLE_C: f"{self.implementations_dir}/c/encoder",
            Platform.CLOUD_JAVA: f"{self.implementations_dir}/java/encoder.sh",
            Platform.MOBILE_SWIFT: f"{self.implementations_dir}/swift/encoder",
            Platform.MOBILE_TYPESCRIPT: f"{self.implementations_dir}/ts/encoder.sh"
        }
        return paths[platform]
    
    def _get_decoder_path(self, platform: Platform) -> str:
        """获取解码器路径"""
        paths = {
            Platform.VEHICLE_C: f"{self.implementations_dir}/c/decoder",
            Platform.CLOUD_JAVA: f"{self.implementations_dir}/java/decoder.sh",
            Platform.MOBILE_SWIFT: f"{self.implementations_dir}/swift/decoder",
            Platform.MOBILE_TYPESCRIPT: f"{self.implementations_dir}/ts/decoder.sh"
        }
        return paths[platform]
    
    def generate_report(self) -> dict:
        """生成测试报告"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r["passed"])
        failed = total - passed
        
        by_priority = {"P0": {"total": 0, "passed": 0}, 
                       "P1": {"total": 0, "passed": 0},
                       "P2": {"total": 0, "passed": 0}}
        
        for r in self.results:
            p = r["priority"]
            by_priority[p]["total"] += 1
            if r["passed"]:
                by_priority[p]["passed"] += 1
        
        return {
            "summary": {
                "total_tests": total,
                "passed": passed,
                "failed": failed,
                "pass_rate": f"{passed/total*100:.1f}%"
            },
            "by_priority": by_priority,
            "details": self.results
        }


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='TLV Interop Test Suite')
    parser.add_argument('--impl-dir', required=True, help='Implementations directory')
    parser.add_argument('--priority', default='all', help='Test priority (P0/P1/P2/all)')
    parser.add_argument('--output', default='interop_report.json', help='Output report file')
    
    args = parser.parse_args()
    
    # 初始化测试运行器
    runner = InteropTestRunner(args.impl_dir)
    
    # 过滤测试用例
    test_cases = INTEROP_TEST_CASES
    if args.priority != 'all':
        test_cases = [tc for tc in test_cases if tc.priority == args.priority]
    
    # 执行测试
    for test_case in test_cases:
        runner.run_test(test_case)
    
    # 生成报告
    report = runner.generate_report()
    
    with open(args.output, 'w') as f:
        json.dump(report, f, indent=2)
    
    print("\n" + "="*60)
    print(f"测试完成: {report['summary']['passed']}/{report['summary']['total_tests']} 通过")
    print(f"通过率: {report['summary']['pass_rate']}")
    print(f"报告已保存到: {args.output}")
    
    # 返回码
    return 0 if report['summary']['failed'] == 0 else 1


if __name__ == "__main__":
    exit(main())
