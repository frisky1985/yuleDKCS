# BER-TLV 协议 Fuzz 测试

> **版本**: v1.0.0  
> **测试范围**: 协议实现安全性、稳定性、边界情况处理  
> **工具**: AFL++, libFuzzer, 自定义模糊测试

---

## 1. Fuzz 测试策略

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TLV Fuzz Testing Strategy                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   目标:                                                                    │
│   1. 发现内存安全漏洞 (buffer overflow, use-after-free)                      │
│   2. 发现崩溃触发条件 (null pointer, infinite loop)                       │
│   3. 验证实现鲁棒性 (对应该拒绝的输入的处理)                           │
│                                                                          │
│   测试类别:                                                                │
│   ┌────────────────────────────────────────────────────────────┐        │
│   │ 1. 基础结构Fuzz - 随机字节数组                      │        │
│   │ 2. 语法导向Fuzz - 基于语法树的结构化输入            │        │
│   │ 3. 语义Fuzz - 符合语法但违反语义的数据            │        │
│   │ 4. 协议Fuzz - 完整消息交互流程                       │        │
│   └────────────────────────────────────────────────────────────┘        │
│                                                                          │
│   工具链:                                                                  │
│   - 输入生成器 → Fuzz引擎 → 被测代码 → 覆盖率收集 → 崩溃分析              │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 基础结构Fuzz (Structure-Agnostic)

### 2.1 AFL++ 配置

```c
// tlv_fuzz_basic.c
// 编译: afl-clang-fast -o tlv_fuzz tlv_fuzz_basic.c tlv_codec.c -lz

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tlv_codec.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // 测试解码器
    TlvParser parser;
    TlvElement elem;
    
    if (tlv_parser_init(&parser, data, size) != TLV_OK) {
        return 0;
    }
    
    // 循环解码所有元素
    while (tlv_parse_next(&parser, &elem) == TLV_OK) {
        // 访问值数据触发内存访问
        volatile uint8_t dummy = 0;
        for (uint32_t i = 0; i < elem.length && i < 10; i++) {
            dummy ^= elem.value[i];
        }
        (void)dummy;
    }
    
    // 测试验证
    tlv_validate_message(data, size);
    
    // 测试构建器
    uint8_t output[4096];
    TlvBuilder builder;
    if (tlv_builder_init(&builder, output, sizeof(output)) == TLV_OK) {
        // 尝试将解析的内容重新构建
        tlv_parser_init(&parser, data, size);
        while (tlv_parse_next(&parser, &elem) == TLV_OK) {
            tlv_add_primitive(&builder, elem.tag, elem.value, elem.length);
        }
    }
    
    return 0;
}
```

### 2.2 运行命令

```bash
# 安装 AFL++
sudo apt-get install afl++

# 编译
export AFL_USE_ASAN=1
afl-clang-fast -o tlv_fuzz tlv_fuzz_basic.c tlv_codec.c -lz

# 准备种子输入
mkdir -p seeds
echo -n $'\x80\x03\x01\x02\x00\x81\x01\x05' > seeds/heartbeat.bin
echo -n $'\x80\x03\x01\x02\x00\x81\x01\x04' > seeds/notification.bin

# 启动Fuzzing
afl-fuzz -i seeds -o findings -m none -- ./tlv_fuzz

# 设置CPU亲和性以提高速度
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### 2.3 常见发现问题

| 问题类型 | 示例输入 | 影响 | 修复方法 |
|---------|----------|------|---------|
| 缓冲区溢出 | `0x04 0x82 0xFF 0xFF ...` | 高 | 检查长度限制 |
| 空指针解引用 | `0x04 0x00` | 中 | NULL检查 |
| 无限循环 | 嵌套指向自身 | 高 | 嵌套深度限制 |
| 整数溢出 | 超长整数表示 | 低 | 范围检查 |
| 资源耗尽 | 极长长度字段 | 中 | 最大长度限制 |

---

## 3. 语法导向Fuzz (Grammar-Based)

### 3.1 基于语法树的生成

```python
# tlv_grammar_fuzzer.py
import random
import struct
from dataclasses import dataclass
from typing import List, Callable, Optional

@dataclass
class TlvElement:
    tag: int
    value: bytes

class TlvGrammarFuzzer:
    """基于语法的TLV Fuzz生成器"""
    
    def __init__(self):
        self.tag_generators = {
            0x80: self._gen_version,
            0x81: self._gen_message_type,
            0x82: self._gen_uuid,
            0x83: self._gen_timestamp,
            0x84: self._gen_uuid,  # session_id
            0x01: self._gen_boolean,
            0x02: self._gen_integer,
            0x0C: self._gen_string,
            0x04: self._gen_bytes,
        }
    
    def _gen_version(self) -> bytes:
        """生成版本信息"""
        major = random.randint(0, 255)
        minor = random.randint(0, 255)
        patch = random.randint(0, 255)
        return bytes([major, minor, patch])
    
    def _gen_message_type(self) -> bytes:
        """生成消息类型"""
        # 有效值: 0x01-0x0F, 也包含无效值用于测试
        if random.random() < 0.8:
            return bytes([random.randint(1, 15)])
        else:
            return bytes([random.randint(0, 255)])
    
    def _gen_uuid(self) -> bytes:
        """生成UUID"""
        return bytes(random.randint(0, 255) for _ in range(16))
    
    def _gen_timestamp(self) -> bytes:
        """生成时间戳"""
        ts = random.randint(0, 0xFFFFFFFFFFFFFFFF)
        return struct.pack('>Q', ts)
    
    def _gen_boolean(self) -> bytes:
        """生成布尔值"""
        # 含义不明确的值也要测试
        if random.random() < 0.8:
            return bytes([0x00 if random.random() < 0.5 else 0xFF])
        else:
            return bytes([random.randint(0, 255)])
    
    def _gen_integer(self) -> bytes:
        """生成整数"""
        # 各种长度的整数
        length = random.choice([1, 2, 4, 8, 16])
        value = random.randint(-(2**(length*8-1)), 2**(length*8-1)-1)
        
        # 转换为大端序字节
        if value >= 0:
            return value.to_bytes(length, 'big')
        else:
            return (value + 2**(length*8)).to_bytes(length, 'big')
    
    def _gen_string(self) -> bytes:
        """生成UTF-8字符串"""
        # 包含正常和异常字符
        length = random.randint(0, 100)
        
        if random.random() < 0.9:
            # 正常ASCII
            return bytes(random.randint(32, 126) for _ in range(length))
        else:
            # 可能无效的UTF-8序列
            return bytes(random.randint(0, 255) for _ in range(length))
    
    def _gen_bytes(self) -> bytes:
        """生成原始字节"""
        length = random.choice([
            0, 1, 15, 16, 31, 32, 127, 128, 255, 256, 1000
        ])
        return bytes(random.randint(0, 255) for _ in range(length))
    
    def encode_length(self, length: int) -> bytes:
        """编码长度字段"""
        if length < 128:
            return bytes([length])
        elif length < 256:
            return bytes([0x81, length])
        elif length < 65536:
            return bytes([0x82, length >> 8, length & 0xFF])
        else:
            return bytes([0x83, (length >> 16) & 0xFF, (length >> 8) & 0xFF, length & 0xFF])
    
    def generate_valid_message(self) -> bytes:
        """生成完全有效的消息"""
        elements = []
        
        # 必需字段
        elements.append(TlvElement(0x80, self._gen_version()))
        elements.append(TlvElement(0x81, self._gen_message_type()))
        elements.append(TlvElement(0x82, self._gen_uuid()))
        elements.append(TlvElement(0x83, self._gen_timestamp()))
        
        # 随机添加其他字段
        optional_tags = [0x84, 0x01, 0x02, 0x0C, 0x04]
        for tag in random.sample(optional_tags, random.randint(0, len(optional_tags))):
            if tag in self.tag_generators:
                elements.append(TlvElement(tag, self.tag_generators[tag]()))
        
        # 编码
        result = bytearray()
        for elem in elements:
            result.append(elem.tag)
            result.extend(self.encode_length(len(elem.value)))
            result.extend(elem.value)
        
        return bytes(result)
    
    def generate_malformed(self, mutation_type: str) -> bytes:
        """生成故障格式数据"""
        
        if mutation_type == "truncated":
            # 截断的消息
            valid = self.generate_valid_message()
            return valid[:random.randint(1, len(valid)-1)]
        
        elif mutation_type == "oversized_length":
            # 声称超过实际数据的长度
            return bytes([0x80, 0x82, 0xFF, 0xFF])
        
        elif mutation_type == "invalid_tag":
            # 无效TAG
            return bytes([0x00, 0x00])  # 保留TAG
        
        elif mutation_type == "indefinite_length":
            # 未定义长度(如果不支持)
            return bytes([0x80, 0x80, 0x00, 0x00])
        
        elif mutation_type == "nested_overflow":
            # 构造元素长度超出
            return bytes([
                0x30, 0x05,  # SEQUENCE, length=5
                0x01, 0x01, 0xFF,  # BOOLEAN
                0x02, 0x02, 0x00  # INTEGER (但只有1字节空间)
            ])
        
        elif mutation_type == "circular_ref":
            # 尝试创建循环引用(理论上不可能在TLV中)
            # 但可以用于测试某些不完整实现
            return bytes([0x30, 0x02, 0x30, 0x00])  # 空嵌套
        
        return self.generate_valid_message()
    
    def generate_corpus(self, count: int, output_dir: str):
        """生成测试语料库"""
        import os
        os.makedirs(output_dir, exist_ok=True)
        
        for i in range(count):
            if random.random() < 0.7:
                data = self.generate_valid_message()
                prefix = "valid"
            else:
                mutation = random.choice([
                    "truncated", "oversized_length", "invalid_tag",
                    "indefinite_length", "nested_overflow"
                ])
                data = self.generate_malformed(mutation)
                prefix = f"malformed_{mutation}"
            
            with open(f"{output_dir}/{prefix}_{i:04d}.bin", 'wb') as f:
                f.write(data)


# 使用示例
if __name__ == "__main__":
    fuzzer = TlvGrammarFuzzer()
    
    # 生成测试语料库
    fuzzer.generate_corpus(1000, "fuzz_corpus")
    
    # 测试单个用例
    print("Valid message:", fuzzer.generate_valid_message().hex())
    print("Truncated:", fuzzer.generate_malformed("truncated").hex())
```

---

## 4. 协议Fuzz (Protocol State)

### 4.1 状态机Fuzz

```python
# tlv_stateful_fuzz.py
"""有状态协议Fuzz测试 - 测试完整交互流程
"""

import random
from enum import Enum, auto
from dataclasses import dataclass
from typing import List, Optional, Callable

class ConnectionState(Enum):
    INIT = auto()
    VERSION_NEGOTIATED = auto()
    AUTHENTICATED = auto()
    CONNECTED = auto()
    ERROR = auto()

@dataclass
class MessageSequence:
    """消息序列"""
    name: str
    messages: List[bytes]
    expected_states: List[ConnectionState]

class TlvStatefulFuzzer:
    """有状态TLV Fuzzer"""
    
    def __init__(self):
        self.state = ConnectionState.INIT
        self.session_id: Optional[bytes] = None
        self.message_counter = 0
        
    def generate_version_negotiation(self) -> bytes:
        """版本协商消息"""
        return bytes([
            0x80, 0x03, 0x01, 0x02, 0x00,  # version
            0x81, 0x01, 0x0E,  # VERSION_NEGOTIATION
            0x82, 0x10, *([0xAA]*16),  # message_id
            0x83, 0x08, *([0x00]*8),  # timestamp
            0xE0, 0x03, 0x01, 0x02, 0x00,  # client_version
        ])
    
    def generate_auth_request(self, valid: bool = True) -> bytes:
        """认证请求"""
        token = b"valid_token" if valid else b""
        return bytes([
            0x80, 0x03, 0x01, 0x02, 0x00,
            0x81, 0x01, 0x01,  # REQUEST
            0x82, 0x10, *([0xBB]*16),
            0x83, 0x08, *([0x00]*8),
            0x88, len(token), *token,  # auth_token
        ])
    
    def generate_heartbeat(self) -> bytes:
        """心跳消息"""
        msg = bytes([
            0x80, 0x03, 0x01, 0x02, 0x00,
            0x81, 0x01, 0x05,  # HEARTBEAT
            0x82, 0x10, *([0xCC]*16),
            0x83, 0x08, *([0x00]*8),
        ])
        if self.session_id:
            msg += bytes([0x84, 0x10, *self.session_id])
        return msg
    
    def generate_command(self, cmd_type: int = 0x01) -> bytes:
        """命令消息"""
        return bytes([
            0x80, 0x03, 0x01, 0x02, 0x00,
            0x81, 0x01, 0x07,  # COMMAND
            0x82, 0x10, *([0xDD]*16),
            0x83, 0x08, *([0x00]*8),
            0x84, 0x10, *([0x00]*16),  # session_id
            0x90, 0x01, cmd_type,  # request_type
        ])
    
    def generate_sequence(self, sequence_type: str) -> List[bytes]:
        """生成特定类型的消息序列"""
        
        sequences = {
            "normal_flow": [
                self.generate_version_negotiation(),
                self.generate_auth_request(valid=True),
                self.generate_heartbeat(),
                self.generate_heartbeat(),
            ],
            "auth_before_version": [
                self.generate_auth_request(),  # 错误: 先认证
                self.generate_version_negotiation(),
            ],
            "command_without_auth": [
                self.generate_version_negotiation(),
                self.generate_command(),  # 错误: 未认证发送命令
            ],
            "rapid_heartbeat": [
                self.generate_version_negotiation(),
                self.generate_auth_request(),
            ] + [self.generate_heartbeat() for _ in range(1000)],  # 洪水
            "version_mismatch": [
                bytes([
                    0x80, 0x03, 0xFF, 0xFF, 0xFF,  # 无效版本
                    0x81, 0x01, 0x0E,
                    0x82, 0x10, *([0xAA]*16),
                    0x83, 0x08, *([0x00]*8),
                ])
            ],
        }
        
        return sequences.get(sequence_type, [])
    
    def fuzz_sequence(self, handler: Callable[[bytes], bool], iterations: int = 100):
        """
Fuzz测试消息序列
        
        handler: 消息处理函数，返回是否成功
        """
        sequence_types = [
            "normal_flow",
            "auth_before_version",
            "command_without_auth",
            "rapid_heartbeat",
            "version_mismatch"
        ]
        
        for i in range(iterations):
            seq_type = random.choice(sequence_types)
            messages = self.generate_sequence(seq_type)
            
            print(f"\nIteration {i+1}: Testing {seq_type}")
            
            for msg_idx, msg in enumerate(messages):
                # 随机突变
                if random.random() < 0.1:
                    msg = self._mutate_message(msg)
                
                try:
                    success = handler(msg)
                    print(f"  Message {msg_idx}: {'OK' if success else 'REJECTED'}")
                except Exception as e:
                    print(f"  Message {msg_idx}: EXCEPTION - {e}")
                    # 记录崩溃
                    with open(f"crash_{i}_{msg_idx}.bin", 'wb') as f:
                        f.write(msg)
    
    def _mutate_message(self, msg: bytes) -> bytes:
        """简单突变"""
        msg = bytearray(msg)
        
        mutation = random.choice([
            'flip_bit',
            'delete_byte',
            'insert_byte',
            'repeat_byte'
        ])
        
        if mutation == 'flip_bit' and msg:
            pos = random.randint(0, len(msg)-1)
            bit = random.randint(0, 7)
            msg[pos] ^= (1 << bit)
        
        elif mutation == 'delete_byte' and len(msg) > 1:
            pos = random.randint(0, len(msg)-1)
            del msg[pos]
        
        elif mutation == 'insert_byte':
            pos = random.randint(0, len(msg))
            msg.insert(pos, random.randint(0, 255))
        
        elif mutation == 'repeat_byte' and msg:
            pos = random.randint(0, len(msg)-1)
            msg.insert(pos, msg[pos])
        
        return bytes(msg)


# 使用示例
def mock_handler(message: bytes) -> bool:
    """模拟消息处理器"""
    # 这里调用实际的TLV解码库
    if len(message) < 12:
        return False
    return True

if __name__ == "__main__":
    fuzzer = TlvStatefulFuzzer()
    fuzzer.fuzz_sequence(mock_handler, iterations=50)
```

---

## 5. 崩溃分析脚本

```python
#!/usr/bin/env python3
# analyze_crashes.py

import os
import sys
import subprocess
from pathlib import Path
from dataclasses import dataclass

@dataclass
class CrashInfo:
    input_file: str
    exit_code: int
    signal: str
    stack_trace: str
    reproducible: bool

def analyze_crash(input_file: str, target_binary: str) -> CrashInfo:
    """分析单个崩溃输入"""
    
    # 运行目标程序
    result = subprocess.run(
        [target_binary],
        input=Path(input_file).read_bytes(),
        capture_output=True,
        timeout=5
    )
    
    # 检查是否可重复
    result2 = subprocess.run(
        [target_binary],
        input=Path(input_file).read_bytes(),
        capture_output=True,
        timeout=5
    )
    
    return CrashInfo(
        input_file=input_file,
        exit_code=result.returncode,
        signal="SIGSEGV" if result.returncode == -11 else f"{result.returncode}",
        stack_trace=result.stderr.decode('utf-8', errors='ignore')[:1000],
        reproducible=(result.returncode == result2.returncode)
    )

def triage_crashes(findings_dir: str, target_binary: str):
    """分类和分析所有崩溃"""
    
    crashes_dir = Path(findings_dir) / "crashes"
    if not crashes_dir.exists():
        print("No crashes found")
        return
    
    crash_groups = {}
    
    for crash_file in crashes_dir.glob("id:*"):
        print(f"Analyzing {crash_file.name}...")
        
        try:
            info = analyze_crash(str(crash_file), target_binary)
            
            # 按堆栈追踪分组
            key = info.stack_trace.split('\n')[0] if info.stack_trace else "unknown"
            
            if key not in crash_groups:
                crash_groups[key] = []
            crash_groups[key].append(info)
            
        except Exception as e:
            print(f"  Error analyzing: {e}")
    
    # 生成报告
    print("\n" + "="*70)
    print("CRASH TRIAGE REPORT")
    print("="*70)
    
    for key, crashes in crash_groups.items():
        print(f"\nGroup: {key[:80]}...")
        print(f"  Count: {len(crashes)}")
        print(f"  Reproducible: {sum(1 for c in crashes if c.reproducible)}/{len(crashes)}")
        print(f"  Sample: {crashes[0].input_file}")
        
        # 输出样例十六进制
        sample_data = Path(crashes[0].input_file).read_bytes()
        print(f"  Hex: {sample_data[:50].hex()}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <findings_dir> <target_binary>")
        sys.exit(1)
    
    triage_crashes(sys.argv[1], sys.argv[2])
```

---

## 6. CI/CD 集成

```yaml
# .github/workflows/fuzzing.yml
name: TLV Fuzz Testing

on:
  schedule:
    - cron: '0 2 * * *'  # 每天凌晨2点
  workflow_dispatch:

jobs:
  fuzz:
    runs-on: ubuntu-latest
    timeout-minutes: 60
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Install AFL++
        run: sudo apt-get install -y afl++
      
      - name: Build Fuzz Target
        env:
          AFL_USE_ASAN: 1
        run: |
          afl-clang-fast -o tlv_fuzz \
            tests/fuzz/tlv_fuzz_basic.c \
            src/tlv_codec.c \
            -I include -lz
      
      - name: Prepare Seeds
        run: |
          mkdir -p seeds
          python3 tests/fuzz/generate_seeds.py --output seeds --count 100
      
      - name: Run Fuzzing
        timeout-minutes: 50
        run: |
          afl-fuzz -i seeds -o findings \
            -V 3000 -m none -- ./tlv_fuzz || true
      
      - name: Analyze Crashes
        if: always()
        run: |
          if [ -d "findings/crashes" ] && [ "$(ls -A findings/crashes)" ]; then
            echo "CRASHES FOUND!"
            python3 tests/fuzz/analyze_crashes.py findings ./tlv_fuzz
            exit 1
          fi
      
      - name: Upload Findings
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: fuzz-findings
          path: findings/
          retention-days: 7
```

---

## 7. 覆盖率分析

```bash
# 使用gcov收集覆盖率信息

# 编译时加入覆盖率选项
gcc -fprofile-arcs -ftest-coverage -o tlv_fuzz tlv_fuzz.c tlv_codec.c

# 运行Fuzz测试
./tlv_fuzz < seeds/*.bin

# 生成报告
gcov tlv_codec.c
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看报告
firefox coverage_report/index.html
```

---

## 8. 测试用例库

```python
# tests/fuzz/corpus/
# 组织结构

corpus/
├── valid/
│   ├── minimal.bin           # 最小有效消息
│   ├── heartbeat.bin         # 心跳消息
│   ├── status_report.bin     # 状态上报
│   ├── location_update.bin   # 位置更新
│   ├── command.bin           # 命令消息
│   └── ota_request.bin       # OTA请求
├── malformed/
│   ├── truncated/
│   ├── invalid_length/
│   ├── invalid_tag/
│   ├── nested_overflow/
│   └── corrupted_crc/
└── edge_cases/
    ├── max_length.bin        # 最大长度
    ├── zero_length.bin       # 零长度
    ├── max_nesting.bin       # 最大嵌套
    └── all_tags.bin          # 所有TAG类型
```

---

*模糊测试版本: v1.0.0*  
*工具: AFL++, libFuzzer, 自定义生成器*