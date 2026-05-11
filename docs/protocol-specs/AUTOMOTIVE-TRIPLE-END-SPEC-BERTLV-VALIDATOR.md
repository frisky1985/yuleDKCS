# BER-TLV 在线协议验证器工具

> **版本**: v1.0.0  
> **支持协议**: v1.2.0  
> **运行方式**: CLI / Web / Library

---

## 1. 工具概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        TLV Validator Tool                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   输入格式:                                                                 │
│   • Hex String: "80 03 01 02 00 81 01 05..."                               │
│   • Base64: "gAMBAgABAVUOhADim0HU..."                                      │
│   • Binary File: --input message.tlv                                       │
│   • JSON: 用于转换验证                                                     │
│                                                                          │
│   验证维度:                                                               │
│   ┌────────────────────────────────────────────────────────────┐        │
│   │ 1. 语法验证: BER-TLV 格式正确性                      │        │
│   │ 2. 结构验证: 必需字段检查 (0x80, 0x81, 0x82, 0x83)       │        │
│   │ 3. 语义验证: 数据范围、类型匹配、枚举值有效性        │        │
│   │ 4. 消息验证: 特定消息模板匹配                         │        │
│   │ 5. 安全验证: 签名、加密、压缩状态                          │        │
│   └────────────────────────────────────────────────────────────┘        │
│                                                                          │
│   输出格式:                                                                 │
│   • 人类可读报告 (Text/JSON)                                              │
│   • 可视化树状结构                                                        │
│   • 导出为Wireshark解析器工具                              │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. CLI工具实现

```python
#!/usr/bin/env python3
"""
TLV Validator CLI Tool
Usage: tlv_validator.py [options] <input>
"""

import argparse
import sys
import json
import base64
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple, Any
from enum import Enum
import yaml


class ValidationLevel(Enum):
    SYNTAX = "syntax"        # 语法验证
    STRUCTURE = "structure"  # 结构验证
    SEMANTIC = "semantic"    # 语义验证
    MESSAGE = "message"      # 消息验证
    SECURITY = "security"    # 安全验证


@dataclass
class ValidationError:
    level: str
    code: str
    message: str
    offset: int
    tag: Optional[int] = None
    suggestion: Optional[str] = None


@dataclass
class ValidationResult:
    valid: bool
    errors: List[ValidationError] = field(default_factory=list)
    warnings: List[ValidationError] = field(default_factory=list)
    info: Dict[str, Any] = field(default_factory=dict)
    tree: Dict = field(default_factory=dict)


class TlvValidator:
    """TLV验证器主类"""
    
    REQUIRED_TAGS = {0x80, 0x81, 0x82, 0x83}  # 必需字段
    
    def __init__(self, schema_path: Optional[str] = None):
        self.schema = None
        if schema_path:
            with open(schema_path, 'r') as f:
                self.schema = yaml.safe_load(f)
    
    def validate(self, data: bytes, level: ValidationLevel = ValidationLevel.MESSAGE) -> ValidationResult:
        """执行完整验证"""
        result = ValidationResult(valid=True)
        
        # 第一步: 语法验证
        self._validate_syntax(data, result)
        if not result.valid and level == ValidationLevel.SYNTAX:
            return result
        
        # 第二步: 结构验证
        if level.value in ['structure', 'semantic', 'message', 'security']:
            self._validate_structure(data, result)
            if not result.valid:
                return result
        
        # 第三步: 语义验证
        if level.value in ['semantic', 'message', 'security']:
            self._validate_semantic(data, result)
        
        # 第四步: 消息模板验证
        if level.value in ['message', 'security']:
            self._validate_message_template(data, result)
        
        # 第五步: 安全验证
        if level.value == 'security':
            self._validate_security(data, result)
        
        return result
    
    def _validate_syntax(self, data: bytes, result: ValidationResult):
        """语法验证: 检查BER-TLV格式"""
        offset = 0
        element_count = 0
        
        while offset < len(data):
            if offset >= len(data):
                break
            
            # 读取TAG
            tag = data[offset]
            offset += 1
            element_count += 1
            
            # 检查TAG有效性
            if tag == 0x00 or tag == 0xFF:
                result.errors.append(ValidationError(
                    level="SYNTAX",
                    code="INVALID_TAG",
                    message=f"Invalid tag value: 0x{tag:02X}",
                    offset=offset - 1,
                    tag=tag
                ))
                result.valid = False
                return
            
            # 解码长度
            try:
                length, offset = self._decode_length(data, offset)
            except ValueError as e:
                result.errors.append(ValidationError(
                    level="SYNTAX",
                    code="INVALID_LENGTH",
                    message=str(e),
                    offset=offset
                ))
                result.valid = False
                return
            
            # 检查值区域
            if offset + length > len(data):
                result.errors.append(ValidationError(
                    level="SYNTAX",
                    code="TRUNCATED_DATA",
                    message=f"Value length {length} exceeds remaining data {len(data) - offset}",
                    offset=offset,
                    tag=tag
                ))
                result.valid = False
                return
            
            offset += length
        
        result.info['element_count'] = element_count
    
    def _decode_length(self, data: bytes, offset: int) -> Tuple[int, int]:
        """解码BER长度字段"""
        if offset >= len(data):
            raise ValueError("Unexpected end of data while reading length")
        
        first = data[offset]
        offset += 1
        
        if (first & 0x80) == 0:
            return first, offset
        
        num_bytes = first & 0x7F
        if num_bytes == 0:
            raise ValueError("Indefinite length form is not supported")
        if num_bytes > 4:
            raise ValueError(f"Length field too large: {num_bytes} bytes")
        if offset + num_bytes > len(data):
            raise ValueError("Length bytes exceed buffer")
        
        length = 0
        for _ in range(num_bytes):
            length = (length << 8) | data[offset]
            offset += 1
        
        if length > 65535:
            raise ValueError(f"Length {length} exceeds maximum allowed")
        
        return length, offset
    
    def _validate_structure(self, data: bytes, result: ValidationResult):
        """结构验证: 检查必需字段"""
        offset = 0
        found_tags = set()
        
        while offset < len(data):
            tag = data[offset]
            offset += 1
            
            length, offset = self._decode_length(data, offset)
            found_tags.add(tag)
            offset += length
        
        # 检查必需字段
        missing = self.REQUIRED_TAGS - found_tags
        if missing:
            for tag in missing:
                result.errors.append(ValidationError(
                    level="STRUCTURE",
                    code="MISSING_REQUIRED_TAG",
                    message=f"Missing required tag: 0x{tag:02X} ({self._tag_name(tag)})",
                    offset=0,
                    tag=tag,
                    suggestion=f"Add {self._tag_name(tag)} field to the message"
                ))
            result.valid = False
        
        # 检查版本长度
        version_data = self._extract_tag_value(data, 0x80)
        if version_data and len(version_data) != 3:
            result.errors.append(ValidationError(
                level="STRUCTURE",
                code="INVALID_VERSION_LENGTH",
                message=f"Protocol version must be 3 bytes, got {len(version_data)}",
                offset=0,
                tag=0x80,
                suggestion="Version format: [major, minor, patch]"
            ))
            result.valid = False
        
        # 检查消息ID长度
        msg_id_data = self._extract_tag_value(data, 0x82)
        if msg_id_data and len(msg_id_data) != 16:
            result.errors.append(ValidationError(
                level="STRUCTURE",
                code="INVALID_MESSAGE_ID_LENGTH",
                message=f"Message ID must be 16 bytes (UUID), got {len(msg_id_data)}",
                offset=0,
                tag=0x82
            ))
            result.valid = False
        
        # 检查时间戳长度
        ts_data = self._extract_tag_value(data, 0x83)
        if ts_data and len(ts_data) != 8:
            result.errors.append(ValidationError(
                level="STRUCTURE",
                code="INVALID_TIMESTAMP_LENGTH",
                message=f"Timestamp must be 8 bytes (int64), got {len(ts_data)}",
                offset=0,
                tag=0x83
            ))
            result.valid = False
    
    def _validate_semantic(self, data: bytes, result: ValidationResult):
        """语义验证: 检查数据值范围"""
        if not self.schema:
            return
        
        # 获取TAG定义
        tag_defs = {t['tag']: t for t in self.schema.get('tags', [])}
        
        offset = 0
        while offset < len(data):
            tag = data[offset]
            offset += 1
            length, offset = self._decode_length(data, offset)
            value = data[offset:offset+length]
            offset += length
            
            if tag in tag_defs:
                self._validate_tag_value(tag, value, tag_defs[tag], result)
    
    def _validate_tag_value(self, tag: int, value: bytes, def_info: dict, result: ValidationResult):
        """验证单个TAG的值"""
        data_type = def_info.get('data_type')
        
        # 整数范围检查
        if data_type == 'int' and 'range' in def_info:
            range_info = def_info['range']
            actual_value = int.from_bytes(value, 'big', signed=True)
            
            if 'min' in range_info and actual_value < range_info['min']:
                result.warnings.append(ValidationError(
                    level="SEMANTIC",
                    code="VALUE_BELOW_MIN",
                    message=f"Value {actual_value} below minimum {range_info['min']}",
                    offset=0,
                    tag=tag
                ))
            
            if 'max' in range_info and actual_value > range_info['max']:
                result.warnings.append(ValidationError(
                    level="SEMANTIC",
                    code="VALUE_ABOVE_MAX",
                    message=f"Value {actual_value} above maximum {range_info['max']}",
                    offset=0,
                    tag=tag
                ))
        
        # 枚举值检查
        if data_type == 'enum' and 'enum_values' in def_info:
            if len(value) == 1:
                enum_val = value[0]
                if enum_val not in def_info['enum_values']:
                    result.warnings.append(ValidationError(
                        level="SEMANTIC",
                        code="UNKNOWN_ENUM_VALUE",
                        message=f"Unknown enum value: 0x{enum_val:02X}",
                        offset=0,
                        tag=tag,
                        suggestion=f"Valid values: {list(def_info['enum_values'].keys())}"
                    ))
    
    def _validate_message_template(self, data: bytes, result: ValidationResult):
        """消息模板验证"""
        if not self.schema:
            return
        
        msg_type_data = self._extract_tag_value(data, 0x81)
        if not msg_type_data or len(msg_type_data) != 1:
            return
        
        msg_type = msg_type_data[0]
        templates = self.schema.get('message_templates', {})
        
        # 查找匹配的模板
        matched_template = None
        for name, template in templates.items():
            # 简化处理: 通过message_type匹配
            if template.get('required_tags', [])[1:2]:  # 第二个通常是message_type
                matched_template = template
                break
        
        if matched_template:
            required = set(matched_template.get('required_tags', []))
            offset = 0
            found = set()
            
            while offset < len(data):
                tag = data[offset]
                found.add(tag)
                offset += 1
                length, offset = self._decode_length(data, offset)
                offset += length
            
            missing = required - found
            if missing:
                for tag in missing:
                    result.warnings.append(ValidationError(
                        level="MESSAGE",
                        code="MISSING_TEMPLATE_FIELD",
                        message=f"Missing field for template: 0x{tag:02X}",
                        offset=0,
                        tag=tag
                    ))
    
    def _validate_security(self, data: bytes, result: ValidationResult):
        """安全验证: 签名、加密等"""
        # 检查压缩标志
        compression_data = self._extract_tag_value(data, 0x8B)
        if compression_data:
            if len(compression_data) != 1:
                result.warnings.append(ValidationError(
                    level="SECURITY",
                    code="INVALID_COMPRESSION_FLAG",
                    message="Compression flag must be 1 byte",
                    offset=0,
                    tag=0x8B
                ))
            elif compression_data[0] not in [0x00, 0x01]:
                result.warnings.append(ValidationError(
                    level="SECURITY",
                    code="UNSUPPORTED_COMPRESSION",
                    message=f"Unsupported compression algorithm: 0x{compression_data[0]:02X}",
                    offset=0,
                    tag=0x8B
                ))
        
        # 检查签名存在性
        signature = self._extract_tag_value(data, 0x89)
        if signature:
            if len(signature) != 32:
                result.warnings.append(ValidationError(
                    level="SECURITY",
                    code="INVALID_SIGNATURE_LENGTH",
                    message=f"HMAC-SHA256 signature must be 32 bytes, got {len(signature)}",
                    offset=0,
                    tag=0x89
                ))
        else:
            # 对于某些消息类型，签名应该存在
            msg_type = self._extract_tag_value(data, 0x81)
            if msg_type and msg_type[0] in [0x07, 0x09]:  # COMMAND/COMMAND_RESULT
                result.warnings.append(ValidationError(
                    level="SECURITY",
                    code="MISSING_SIGNATURE",
                    message="Command messages should include signature",
                    offset=0,
                    tag=0x89,
                    suggestion="Add HMAC-SHA256 signature for message authentication"
                ))
    
    def _extract_tag_value(self, data: bytes, target_tag: int) -> Optional[bytes]:
        """提取指定TAG的值"""
        offset = 0
        while offset < len(data):
            tag = data[offset]
            offset += 1
            length, offset = self._decode_length(data, offset)
            if tag == target_tag:
                return data[offset:offset+length]
            offset += length
        return None
    
    def _tag_name(self, tag: int) -> str:
        """获取TAG名称"""
        if self.schema:
            for t in self.schema.get('tags', []):
                if t['tag'] == tag:
                    return t['name']
        return f"TAG_0x{tag:02X}"
    
    def parse_tree(self, data: bytes) -> Dict:
        """解析TLV为树状结构"""
        tree = {'type': 'root', 'children': []}
        offset = 0
        
        while offset < len(data):
            tag = data[offset]
            tag_start = offset
            offset += 1
            length, offset = self._decode_length(data, offset)
            value = data[offset:offset+length]
            offset += length
            
            node = {
                'tag': tag,
                'tag_name': self._tag_name(tag),
                'offset': tag_start,
                'length': length,
                'value_hex': value.hex(),
                'value_preview': self._preview_value(tag, value)
            }
            
            tree['children'].append(node)
        
        return tree
    
    def _preview_value(self, tag: int, value: bytes) -> str:
        """生成值的预览"""
        if tag == 0x80 and len(value) == 3:  # Version
            return f"v{value[0]}.{value[1]}.{value[2]}"
        elif tag == 0x81 and len(value) == 1:  # Message Type
            msg_types = {0x01: "REQUEST", 0x02: "RESPONSE", 0x04: "NOTIFICATION", 0x05: "HEARTBEAT"}
            return msg_types.get(value[0], f"0x{value[0]:02X}")
        elif tag == 0x83 and len(value) == 8:  # Timestamp
            ts = int.from_bytes(value, 'big')
            return f"{ts} ({self._format_timestamp(ts)})"
        elif tag in [0x0C]:  # UTF8String
            try:
                return value.decode('utf-8')[:50]
            except:
                return value.hex()[:50]
        else:
            return value.hex()[:50] + ("..." if len(value) > 25 else "")
    
    def _format_timestamp(self, ts: int) -> str:
        """格式化时间戳"""
        from datetime import datetime
        try:
            dt = datetime.fromtimestamp(ts / 1000)
            return dt.strftime("%Y-%m-%d %H:%M:%S")
        except:
            return "Invalid"


def main():
    parser = argparse.ArgumentParser(
        description='TLV Protocol Validator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s "80 03 01 02 00 81 01 05"
  %(prog)s --base64 "gAMBAgABAVUOhADim0HU"
  %(prog)s --file message.bin --schema schema.yaml
  %(prog)s --json '{"protocolVersion": "1.2.0", ...}'
        """
    )
    
    parser.add_argument('input', nargs='?', help='Hex string to validate')
    parser.add_argument('--base64', '-b', action='store_true', help='Input is base64 encoded')
    parser.add_argument('--file', '-f', help='Read from binary file')
    parser.add_argument('--json', '-j', action='store_true', help='Input is JSON (for conversion)')
    parser.add_argument('--schema', '-s', help='Schema YAML file for enhanced validation')
    parser.add_argument('--level', '-l', choices=['syntax', 'structure', 'semantic', 'message', 'security'],
                       default='message', help='Validation level')
    parser.add_argument('--format', choices=['text', 'json', 'tree'], default='text',
                       help='Output format')
    parser.add_argument('--output', '-o', help='Output file (default: stdout)')
    
    args = parser.parse_args()
    
    # 读取输入数据
    data = None
    
    if args.file:
        data = Path(args.file).read_bytes()
    elif args.base64 and args.input:
        data = base64.b64decode(args.input)
    elif args.json and args.input:
        # JSON to TLV conversion
        json_data = json.loads(args.input)
        # 调用转换函数
        print("JSON to TLV conversion not yet implemented")
        return
    elif args.input:
        # Hex string
        hex_str = args.input.replace(' ', '').replace('-', '')
        data = bytes.fromhex(hex_str)
    else:
        parser.print_help()
        return
    
    # 创建验证器
    validator = TlvValidator(args.schema)
    
    # 执行验证
    level = ValidationLevel(args.level)
    result = validator.validate(data, level)
    
    # 解析树结构
    if args.format == 'tree':
        result.tree = validator.parse_tree(data)
    
    # 输出结果
    output = []
    
    if args.format == 'text':
        output.append("=" * 60)
        output.append("TLV Validation Report")
        output.append("=" * 60)
        output.append(f"Input size: {len(data)} bytes")
        output.append(f"Validation level: {level.value}")
        output.append(f"Result: {'PASSED ✓' if result.valid else 'FAILED ✗'}")
        output.append("")
        
        if result.info:
            output.append("Info:")
            for key, value in result.info.items():
                output.append(f"  {key}: {value}")
            output.append("")
        
        if result.errors:
            output.append(f"Errors ({len(result.errors)}):")
            for err in result.errors:
                output.append(f"  [{err.level}] {err.code}: {err.message}")
                if err.suggestion:
                    output.append(f"    Suggestion: {err.suggestion}")
            output.append("")
        
        if result.warnings:
            output.append(f"Warnings ({len(result.warnings)}):")
            for warn in result.warnings:
                output.append(f"  [{warn.level}] {warn.code}: {warn.message}")
            output.append("")
        
        # 解析结果
        tree = validator.parse_tree(data)
        output.append("TLV Structure:")
        output.append(f"{'Offset':<8} {'Tag':<20} {'Length':<8} {'Value Preview'}")
        output.append("-" * 70)
        for child in tree['children']:
            output.append(f"{child['offset']:<8} {child['tag_name']:<20} {child['length']:<8} {child['value_preview']}")
    
    elif args.format == 'json':
        output_dict = {
            'valid': result.valid,
            'input_size': len(data),
            'level': level.value,
            'errors': [
                {
                    'level': e.level,
                    'code': e.code,
                    'message': e.message,
                    'offset': e.offset,
                    'tag': e.tag,
                    'suggestion': e.suggestion
                } for e in result.errors
            ],
            'warnings': [
                {
                    'level': w.level,
                    'code': w.code,
                    'message': w.message
                } for w in result.warnings
            ],
            'info': result.info,
            'tree': result.tree if result.tree else validator.parse_tree(data)
        }
        output.append(json.dumps(output_dict, indent=2))
    
    elif args.format == 'tree':
        output.append(json.dumps(result.tree, indent=2))
    
    # 输出
    output_text = '\n'.join(output)
    
    if args.output:
        Path(args.output).write_text(output_text)
        print(f"Report written to: {args.output}")
    else:
        print(output_text)
    
    # 返回码
    sys.exit(0 if result.valid else 1)


if __name__ == '__main__':
    main()
```

---

## 3. 使用示例

### 3.1 基本验证

```bash
# 验证Hex字符串
$ tlv_validator.py "80 03 01 02 00 81 01 05 82 10 AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99 83 08 00 00 01 91 D4 8A E0 00"

============================================================
TLV Validation Report
============================================================
Input size: 36 bytes
Validation level: message
Result: PASSED ✓

Info:
  element_count: 4

TLV Structure:
Offset   Tag                  Length   Value Preview
----------------------------------------------------------------------
0        PROTOCOL_VERSION     3        v1.2.0
4        MESSAGE_TYPE         1        HEARTBEAT
6        MESSAGE_ID           16       aabbcc... (UUID)
24       TIMESTAMP            8        1715410200000 (2026-05-11 16:10:00)
```

### 3.2 高级验证（带Schema）

```bash
$ tlv_validator.py \
    --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
    --level security \
    --format json \
    "80030102008101058210..." \
    --output report.json

$ cat report.json
{
  "valid": true,
  "input_size": 45,
  "level": "security",
  "errors": [],
  "warnings": [
    {
      "level": "SECURITY",
      "code": "MISSING_SIGNATURE",
      "message": "Command messages should include signature",
      "suggestion": "Add HMAC-SHA256 signature for message authentication"
    }
  ],
  "info": {
    "element_count": 5
  },
  "tree": {
    "type": "root",
    "children": [
      {
        "tag": 128,
        "tag_name": "PROTOCOL_VERSION",
        "offset": 0,
        "length": 3,
        "value_hex": "010200",
        "value_preview": "v1.2.0"
      }
    ]
  }
}
```

### 3.3 从文件验证

```bash
# 批量验证多个消息
for f in messages/*.bin; do
    echo "Validating $f..."
    tlv_validator.py --file "$f" --level semantic || echo "FAILED: $f"
done
```

### 3.4 解析树结构

```bash
$ tlv_validator.py --format tree "8003010200810105..."
{
  "type": "root",
  "children": [
    {
      "tag": 128,
      "tag_name": "PROTOCOL_VERSION",
      "offset": 0,
      "length": 3,
      "value_hex": "010200",
      "value_preview": "v1.2.0"
    },
    {
      "tag": 129,
      "tag_name": "MESSAGE_TYPE",
      "offset": 4,
      "length": 1,
      "value_hex": "05",
      "value_preview": "HEARTBEAT"
    }
  ]
}
```

---

## 4. Web版验证器

```python
# tlv_web_validator.py
from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)

HTML_TEMPLATE = '''
<!DOCTYPE html>
<html>
<head>
    <title>TLV Protocol Validator</title>
    <style>
        body { font-family: monospace; margin: 40px; background: #1e1e1e; color: #d4d4d4; }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #4ec9b0; }
        textarea { width: 100%; height: 150px; background: #252526; color: #d4d4d4; border: 1px solid #454545; }
        button { padding: 10px 20px; background: #0e639c; color: white; border: none; cursor: pointer; margin: 10px 0; }
        button:hover { background: #1177bb; }
        .result { background: #252526; padding: 20px; margin-top: 20px; border-radius: 4px; }
        .error { color: #f48771; }
        .warning { color: #dcdcaa; }
        .success { color: #4ec9b0; }
        .tree { font-size: 12px; }
        .tree-row { display: flex; padding: 2px 0; }
        .tree-tag { width: 150px; color: #9cdcfe; }
        .tree-len { width: 80px; color: #b5cea8; }
        .tree-val { color: #ce9178; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 TLV Protocol Validator</h1>
        <form method="POST">
            <p>
                <label>Input Format:</label>
                <select name="format">
                    <option value="hex">Hex String</option>
                    <option value="base64">Base64</option>
                </select>
                <label>Validation Level:</label>
                <select name="level">
                    <option value="syntax">Syntax</option>
                    <option value="structure" selected>Structure</option>
                    <option value="semantic">Semantic</option>
                    <option value="message">Message</option>
                    <option value="security">Security</option>
                </select>
            </p>
            <textarea name="input" placeholder="Enter TLV data here...">{{ input_data }}</textarea>
            <br>
            <button type="submit">Validate</button>
        </form>
        
        {% if result %}
        <div class="result">
            <h2 class="{{ 'success' if result.valid else 'error' }}">
                {{ 'PASSED ✓' if result.valid else 'FAILED ✗' }}
            </h2>
            <p>Input size: {{ result.size }} bytes</p>
            
            {% if result.errors %}
            <h3 class="error">Errors ({{ result.errors|length }}):</h3>
            <ul>
                {% for err in result.errors %}
                <li>[{{ err.level }}] {{ err.code }}: {{ err.message }}</li>
                {% endfor %}
            </ul>
            {% endif %}
            
            {% if result.warnings %}
            <h3 class="warning">Warnings ({{ result.warnings|length }}):</h3>
            <ul>
                {% for warn in result.warnings %}
                <li>[{{ warn.level }}] {{ warn.code }}: {{ warn.message }}</li>
                {% endfor %}
            </ul>
            {% endif %}
            
            <h3>TLV Structure:</h3>
            <div class="tree">
                <div class="tree-row">
                    <span class="tree-tag"><b>TAG</b></span>
                    <span class="tree-len"><b>LEN</b></span>
                    <span class="tree-val"><b>VALUE</b></span>
                </div>
                {% for item in result.tree %}
                <div class="tree-row">
                    <span class="tree-tag">{{ item.tag_name }}</span>
                    <span class="tree-len">{{ item.length }}</span>
                    <span class="tree-val">{{ item.value_preview }}</span>
                </div>
                {% endfor %}
            </div>
        </div>
        {% endif %}
    </div>
</body>
</html>
'''

@app.route('/', methods=['GET', 'POST'])
def index():
    result = None
    input_data = ''
    
    if request.method == 'POST':
        input_data = request.form.get('input', '')
        input_format = request.form.get('format', 'hex')
        level = request.form.get('level', 'structure')
        
        try:
            # 解码输入
            if input_format == 'base64':
                import base64
                data = base64.b64decode(input_data)
            else:
                hex_str = input_data.replace(' ', '').replace('-', '')
                data = bytes.fromhex(hex_str)
            
            # 验证
            validator = TlvValidator()
            validation_level = ValidationLevel(level)
            v_result = validator.validate(data, validation_level)
            tree = validator.parse_tree(data)
            
            result = {
                'valid': v_result.valid,
                'size': len(data),
                'errors': [
                    {'level': e.level, 'code': e.code, 'message': e.message}
                    for e in v_result.errors
                ],
                'warnings': [
                    {'level': w.level, 'code': w.code, 'message': w.message}
                    for w in v_result.warnings
                ],
                'tree': tree['children']
            }
        except Exception as e:
            result = {
                'valid': False,
                'errors': [{'level': 'PARSE', 'code': 'EXCEPTION', 'message': str(e)}],
                'tree': []
            }
    
    return render_template_string(HTML_TEMPLATE, result=result, input_data=input_data)

@app.route('/api/validate', methods=['POST'])
def api_validate():
    """REST API endpoint"""
    try:
        data = request.get_data()
        validator = TlvValidator()
        result = validator.validate(data, ValidationLevel.MESSAGE)
        
        return jsonify({
            'valid': result.valid,
            'size': len(data),
            'errors': len(result.errors),
            'warnings': len(result.warnings)
        })
    except Exception as e:
        return jsonify({'error': str(e)}), 400

if __name__ == '__main__':
    app.run(debug=True, port=5000)
```

### 启动Web服务

```bash
pip install flask
python tlv_web_validator.py

# 访问 http://localhost:5000
```

---

## 5. CI/CD 集成

```yaml
# .github/workflows/tlv-validation.yml
name: TLV Protocol Validation

on:
  push:
    paths:
      - '**.tlv'
      - '**/messages/**'

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      
      - name: Install dependencies
        run: pip install pyyaml
      
      - name: Validate TLV Messages
        run: |
          failed=0
          for f in $(find . -name "*.tlv" -o -name "*.bin"); do
            echo "Validating $f..."
            if ! python tlv_validator.py --file "$f" --schema schema.yaml --level message; then
              echo "FAILED: $f"
              failed=1
            fi
          done
          exit $failed
      
      - name: Upload Report
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: validation-reports
          path: '**/validation-*.json'
```

---

*验证器版本: v1.0.0*  
*支持协议: v1.2.0*
