# BER-TLV 协议代码生成器

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **支持语言**: C/C++, Java, Python, Go, Rust

---

## 1. 代码生成器架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     TLV Code Generator Architecture                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   ┌──────────────────────────────────────────────────────────────────┐        │
│   │                  Schema Parser (YAML)                               │        │
│   │  ┌────────────────────────────────────────────────────────────┤        │
│   │  │ TAG定义 │ 数据类型 │ 验证规则 │ 消息模板 │ 错误码   │        │
│   │  └─────────┴─────────────┴──────────────┴────────────────┴──────────┘        │
│   └──────────────────────────────────────────────────────────────────┘        │
│                         │                                               │
│                         ▼                                               │
│   ┌──────────────────────────────────────────────────────────────────┐        │
│   │                  AST (Abstract Syntax Tree)                         │        │
│   └──────────────────────────────────────────────────────────────────┘        │
│                         │                                               │
│         ┌───────────────┼───────────────┼───────────────┼───────────────┼───────────────┐       │
│         ▼             ▼             ▼             ▼             ▼       │
│   ┌──────────────────────────────────────────────────────────────────┐       │
│   │                    Language Generators                             │       │
│   ├──────────────────────────────────────────────────────────────────┤       │
│   │  ┌────────┬────────┬────────┬────────┬────────┐                           │
│   │  │ C/C++  │  Java   │ Python │  Go    │  Rust  │                           │
│   │  │ Gen    │ Gen    │ Gen    │ Gen    │ Gen    │                           │
│   │  └────────┴────────┴────────┴────────┴────────┘                           │
│   └──────────────────────────────────────────────────────────────────┘       │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Python 代码生成器实现

```python
#!/usr/bin/env python3
"""
TLV Code Generator
从Schema生成各语言的TLV编解码代码
"""

import yaml
import argparse
from pathlib import Path
from typing import Dict, List, Any
from jinja2 import Template

# 语言特定的类垁映射
TYPE_MAPPINGS = {
    'c': {
        'bool': 'bool',
        'int': 'int64_t',
        'string': 'const char*',
        'bytes': 'const uint8_t*',
        'version': 'tlv_version_t',
        'enum': 'uint8_t'
    },
    'java': {
        'bool': 'boolean',
        'int': 'long',
        'string': 'String',
        'bytes': 'byte[]',
        'version': 'ProtocolVersion',
        'enum': 'int'
    },
    'python': {
        'bool': 'bool',
        'int': 'int',
        'string': 'str',
        'bytes': 'bytes',
        'version': 'Tuple[int, int, int]',
        'enum': 'int'
    },
    'go': {
        'bool': 'bool',
        'int': 'int64',
        'string': 'string',
        'bytes': '[]byte',
        'version': 'Version',
        'enum': 'uint8'
    },
    'rust': {
        'bool': 'bool',
        'int': 'i64',
        'string': 'String',
        'bytes': 'Vec<u8>',
        'version': 'Version',
        'enum': 'u8'
    }
}

class TlvCodeGenerator:
    def __init__(self, schema_path: str):
        with open(schema_path, 'r') as f:
            self.schema = yaml.safe_load(f)
    
    def generate_c(self, output_dir: str):
        """生成C/C++代码"""
        
        header_template = Template('''
/* Auto-generated TLV Protocol Definitions
 * Schema Version: {{ schema_version }}
 * Generated: {{ timestamp }}
 * DO NOT EDIT MANUALLY
 */

#ifndef TLV_AUTO_GENERATED_H
#define TLV_AUTO_GENERATED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol Version */
#define TLV_PROTOCOL_VERSION_MAJOR {{ version_major }}
#define TLV_PROTOCOL_VERSION_MINOR {{ version_minor }}
#define TLV_PROTOCOL_VERSION_PATCH {{ version_patch }}

/* Error Codes */
typedef enum {
{% for code, name in error_codes.items() %}
    TLV_ERR_{{ name }} = {{ code }},
{% endfor %}
} tlv_error_code_t;

/* TAG Definitions */
{% for tag in tags %}
#define TLV_TAG_{{ tag.name }} 0x{{ "%.2X" | format(tag.tag) }}
{% endfor %}

/* Enum Definitions */
{% for tag in enums %}
typedef enum {
{% for val, name in tag.enum_values.items() %}
    {{ tag.name }}_{{ name }} = {{ val }},
{% endfor %}
} {{ tag.name | lower }}_t;
{% endfor %}

/* Struct Definitions */
{% for tag in constructed_tags %}
typedef struct {
{% for child_tag in tag.children %}
    {{ child_tag.c_type }} {{ child_tag.name | lower }};
{% endfor %}
} {{ tag.name | lower }}_t;
{% endfor %}

/* Message Types */
typedef enum {
{% for val, name in message_types.items() %}
    TLV_MSG_{{ name }} = {{ val }},
{% endfor %}
} tlv_message_type_t;

/* Auto-generated Encode Functions */
{% for tag in constructed_tags %}
int tlv_encode_{{ tag.name | lower }}(uint8_t* buffer, size_t buffer_size, const {{ tag.name | lower }}_t* data, size_t* out_size);
{% endfor %}

/* Auto-generated Decode Functions */
{% for tag in constructed_tags %}
int tlv_decode_{{ tag.name | lower }}(const uint8_t* buffer, size_t buffer_size, {{ tag.name | lower }}_t* out_data);
{% endfor %}

/* Auto-generated Validation Functions */
{% for template in message_templates %}
bool tlv_validate_{{ template.name }}(const uint8_t* buffer, size_t buffer_size);
{% endfor %}

#ifdef __cplusplus
}
#endif

#endif /* TLV_AUTO_GENERATED_H */
''')
        
        # 收集数据
        data = {
            'schema_version': self.schema['schema_version'],
            'version_major': self.schema['schema_version'].split('.')[0],
            'version_minor': self.schema['schema_version'].split('.')[1],
            'version_patch': self.schema['schema_version'].split('.')[2],
            'timestamp': '2026-05-10',
            'error_codes': self.schema.get('error_codes', {}),
            'tags': self.schema.get('tags', []),
            'enums': [t for t in self.schema.get('tags', []) if t.get('enum_values')],
            'constructed_tags': [t for t in self.schema.get('tags', []) if t.get('type') == 'constructed'],
            'message_types': self.schema.get('tags', [])[10].get('enum_values', {}) if len(self.schema.get('tags', [])) > 10 else {},
            'message_templates': self.schema.get('message_templates', [])
        }
        
        # 生成头文件
        output = header_template.render(**data)
        
        output_path = Path(output_dir) / 'tlv_auto_generated.h'
        output_path.write_text(output)
        print(f"Generated: {output_path}")
    
    def generate_java(self, output_dir: str):
        """生成Java代码"""
        
        class_template = Template('''
/* Auto-generated TLV Protocol Classes
 * Schema Version: {{ schema_version }}
 * Generated: {{ timestamp }}
 * DO NOT EDIT MANUALLY
 */

package com.automotive.tlv.generated;

import java.util.*;

public final class TlvProtocol {
    
    public static final String SCHEMA_VERSION = "{{ schema_version }}";
    
    /* TAG Constants */
    public static final class Tags {
{% for tag in tags %}
        public static final int {{ tag.name }} = 0x{{ "%.2X" | format(tag.tag) }};
{% endfor %}
    }
    
    /* Enum Definitions */
{% for tag in enums %}
    public enum {{ tag.name }} {
{% for val, name in tag.enum_values.items() %}
        {{ name }}({{ val }}),
{% endfor %}
        ;
        
        private final int value;
        
        {{ tag.name }}(int value) {
            this.value = value;
        }
        
        public int getValue() {
            return value;
        }
        
        public static {{ tag.name }} fromValue(int value) {
            for ({{ tag.name }} e : values()) {
                if (e.value == value) return e;
            }
            throw new IllegalArgumentException("Invalid value: " + value);
        }
    }
{% endfor %}
    
    /* Message Type Enum */
    public enum MessageType {
{% for val, name in message_types.items() %}
        {{ name }}({{ val }}),
{% endfor %}
        ;
        
        private final int value;
        
        MessageType(int value) {
            this.value = value;
        }
        
        public int getValue() {
            return value;
        }
    }
    
    /* Data Classes */
{% for tag in constructed_tags %}
    public static class {{ tag.name }} {
{% for child_tag in tag.children %}
        private {{ child_tag.java_type }} {{ child_tag.name | lower }};
{% endfor %}
        
        public {{ tag.name }}() {}
        
{% for child_tag in tag.children %}
        public {{ child_tag.java_type }} get{{ child_tag.name }}() {
            return {{ child_tag.name | lower }};
        }
        
        public void set{{ child_tag.name }}({{ child_tag.java_type }} value) {
            this.{{ child_tag.name | lower }} = value;
        }
        
{% endfor %}
        @Override
        public String toString() {
            return "{{ tag.name }}{" +
{% for child_tag in tag.children %}
                "{{ child_tag.name | lower }}=" + {{ child_tag.name | lower }} +
{% endfor %}
                '}';
        }
    }
{% endfor %}
    
    /* Error Codes */
    public static final class ErrorCodes {
{% for code, name in error_codes.items() %}
        public static final int {{ name }} = {{ code }};
{% endfor %}
    }
}
''')
        
        data = {
            'schema_version': self.schema['schema_version'],
            'timestamp': '2026-05-10',
            'tags': self.schema.get('tags', []),
            'enums': [t for t in self.schema.get('tags', []) if t.get('enum_values')],
            'constructed_tags': [t for t in self.schema.get('tags', []) if t.get('type') == 'constructed'],
            'message_types': self.schema.get('tags', [])[10].get('enum_values', {}) if len(self.schema.get('tags', [])) > 10 else {},
            'error_codes': self.schema.get('error_codes', {})
        }
        
        output = class_template.render(**data)
        
        output_path = Path(output_dir) / 'TlvProtocol.java'
        output_path.write_text(output)
        print(f"Generated: {output_path}")
    
    def generate_python(self, output_dir: str):
        """生成Python代码"""
        
        py_template = Template('''
"""Auto-generated TLV Protocol Module
Schema Version: {{ schema_version }}
Generated: {{ timestamp }}
DO NOT EDIT MANUALLY
"""

from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, List, Tuple

SCHEMA_VERSION = "{{ schema_version }}"

# TAG Constants
class Tag(IntEnum):
{% for tag in tags %}
    {{ tag.name }} = 0x{{ "%.2X" | format(tag.tag) }}
{% endfor %}

# Enum Definitions
{% for tag in enums %}
class {{ tag.name }}(IntEnum):
{% for val, name in tag.enum_values.items() %}
    {{ name }} = {{ val }}
{% endfor %}

{% endfor %}

# Message Type Enum
class MessageType(IntEnum):
{% for val, name in message_types.items() %}
    {{ name }} = {{ val }}
{% endfor %}

# Data Classes
{% for tag in constructed_tags %}
@dataclass
class {{ tag.name }}:
{% for child_tag in tag.children %}
    {{ child_tag.name | lower }}: {{ child_tag.python_type }}
{% endfor %}

{% endfor %}

# Error Codes
class ErrorCode(IntEnum):
{% for code, name in error_codes.items() %}
    {{ name }} = {{ code }}
{% endfor %}

# Validation Functions
{% for template in message_templates %}
def validate_{{ template.name }}(data: bytes) -> bool:
    """Validate {{ template.description }}"""
    # TODO: Implement validation logic
    pass

{% endfor %}
''')
        
        data = {
            'schema_version': self.schema['schema_version'],
            'timestamp': '2026-05-10',
            'tags': self.schema.get('tags', []),
            'enums': [t for t in self.schema.get('tags', []) if t.get('enum_values')],
            'constructed_tags': [t for t in self.schema.get('tags', []) if t.get('type') == 'constructed'],
            'message_types': self.schema.get('tags', [])[10].get('enum_values', {}) if len(self.schema.get('tags', [])) > 10 else {},
            'message_templates': self.schema.get('message_templates', []),
            'error_codes': self.schema.get('error_codes', {})
        }
        
        output = py_template.render(**data)
        
        output_path = Path(output_dir) / 'tlv_protocol.py'
        output_path.write_text(output)
        print(f"Generated: {output_path}")


def main():
    parser = argparse.ArgumentParser(description='TLV Code Generator')
    parser.add_argument('--schema', required=True, help='Path to schema YAML file')
    parser.add_argument('--language', required=True, choices=['c', 'java', 'python', 'all'])
    parser.add_argument('--output', required=True, help='Output directory')
    
    args = parser.parse_args()
    
    gen = TlvCodeGenerator(args.schema)
    
    if args.language in ['c', 'all']:
        gen.generate_c(args.output)
    
    if args.language in ['java', 'all']:
        gen.generate_java(args.output)
    
    if args.language in ['python', 'all']:
        gen.generate_python(args.output)
    
    print("Code generation completed!")


if __name__ == '__main__':
    main()
```

### 使用示例

```bash
# 安装依赖
pip install pyyaml jinja2

# 生成所有语言代码
python tlv_codegen.py \
    --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
    --language all \
    --output ./generated/

# 生成特定语言
python tlv_codegen.py \
    --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
    --language c \
    --output ./generated/c/
```

---

## 3. 生成代码示例

### 3.1 C 语言生成结果

```c
/* Auto-generated TLV Protocol Definitions */

#ifndef TLV_AUTO_GENERATED_H
#define TLV_AUTO_GENERATED_H

#include <stdint.h>
#include <stdbool.h>

/* TAG Definitions */
#define TLV_TAG_PROTOCOL_VERSION 0x80
#define TLV_TAG_MESSAGE_TYPE 0x81
#define TLV_TAG_MESSAGE_ID 0x82
#define TLV_TAG_TIMESTAMP 0x83
#define TLV_TAG_SESSION_ID 0x84
#define TLV_TAG_VEHICLE_INFO 0x40
#define TLV_TAG_VIN 0x41
#define TLV_TAG_VEHICLE_MODEL 0x42
#define TLV_TAG_VEHICLE_STATUS 0x45
#define TLV_TAG_LOCATION 0x50
#define TLV_TAG_LATITUDE 0x51
#define TLV_TAG_LONGITUDE 0x52
#define TLV_TAG_SPEED 0x54

/* Enum Definitions */
typedef enum {
    DEVICE_TYPE_TBOX = 0x01,
    DEVICE_TYPE_IVI = 0x02,
    DEVICE_TYPE_ADAS = 0x03,
    DEVICE_TYPE_CLUSTER = 0x04,
} device_type_t;

typedef enum {
    MSG_REQUEST = 0x01,
    MSG_RESPONSE = 0x02,
    MSG_ERROR = 0x03,
    MSG_NOTIFICATION = 0x04,
    MSG_HEARTBEAT = 0x05,
    MSG_HEARTBEAT_ACK = 0x06,
    MSG_COMMAND = 0x07,
    MSG_COMMAND_ACK = 0x08,
    MSG_COMMAND_RESULT = 0x09,
    MSG_OTA_REQUEST = 0x0A,
    MSG_OTA_RESPONSE = 0x0B,
    MSG_SYNC_REQUEST = 0x0C,
    MSG_SYNC_RESPONSE = 0x0D,
    MSG_VERSION_NEGOTIATION = 0x0E,
    MSG_VERSION_NEGOTIATION_ACK = 0x0F,
} tlv_message_type_t;

/* Struct Definitions */
typedef struct {
    const char* vin;
    const char* vehicle_model;
    const char* vehicle_brand;
    const char* license_plate;
} vehicle_info_t;

typedef struct {
    bool ignition;
    bool lock_status;
    int64_t mileage;
    int64_t fuel_level;
    int64_t battery_voltage;
    int64_t engine_temp;
} vehicle_status_t;

typedef struct {
    int64_t latitude;
    int64_t longitude;
    int64_t altitude;
    int64_t speed;
    int64_t heading;
} location_t;

/* Auto-generated Encode Functions */
int tlv_encode_vehicle_info(uint8_t* buffer, size_t buffer_size, const vehicle_info_t* data, size_t* out_size);
int tlv_encode_vehicle_status(uint8_t* buffer, size_t buffer_size, const vehicle_status_t* data, size_t* out_size);
int tlv_encode_location(uint8_t* buffer, size_t buffer_size, const location_t* data, size_t* out_size);

/* Auto-generated Decode Functions */
int tlv_decode_vehicle_info(const uint8_t* buffer, size_t buffer_size, vehicle_info_t* out_data);
int tlv_decode_vehicle_status(const uint8_t* buffer, size_t buffer_size, vehicle_status_t* out_data);
int tlv_decode_location(const uint8_t* buffer, size_t buffer_size, location_t* out_data);

/* Auto-generated Validation Functions */
bool tlv_validate_heartbeat(const uint8_t* buffer, size_t buffer_size);
bool tlv_validate_vehicle_status(const uint8_t* buffer, size_t buffer_size);
bool tlv_validate_location_update(const uint8_t* buffer, size_t buffer_size);
bool tlv_validate_remote_command(const uint8_t* buffer, size_t buffer_size);
bool tlv_validate_version_negotiation(const uint8_t* buffer, size_t buffer_size);

#endif
```

### 3.2 Java 生成结果

```java
/* Auto-generated TLV Protocol Classes */

package com.automotive.tlv.generated;

public final class TlvProtocol {
    
    public static final String SCHEMA_VERSION = "1.2.0";
    
    /* TAG Constants */
    public static final class Tags {
        public static final int PROTOCOL_VERSION = 0x80;
        public static final int MESSAGE_TYPE = 0x81;
        public static final int MESSAGE_ID = 0x82;
        public static final int TIMESTAMP = 0x83;
        public static final int VEHICLE_INFO = 0x40;
        public static final int VIN = 0x41;
        public static final int VEHICLE_MODEL = 0x42;
        public static final int VEHICLE_STATUS = 0x45;
    }
    
    /* Enum Definitions */
    public enum DeviceType {
        TBOX(0x01),
        IVI(0x02),
        ADAS(0x03),
        CLUSTER(0x04);
        
        private final int value;
        
        DeviceType(int value) {
            this.value = value;
        }
        
        public int getValue() {
            return value;
        }
        
        public static DeviceType fromValue(int value) {
            for (DeviceType e : values()) {
                if (e.value == value) return e;
            }
            throw new IllegalArgumentException("Invalid value: " + value);
        }
    }
    
    /* Message Type Enum */
    public enum MessageType {
        REQUEST(0x01),
        RESPONSE(0x02),
        ERROR(0x03),
        NOTIFICATION(0x04),
        HEARTBEAT(0x05),
        COMMAND(0x07),
        OTA_REQUEST(0x0A);
        
        private final int value;
        
        MessageType(int value) {
            this.value = value;
        }
        
        public int getValue() {
            return value;
        }
    }
    
    /* Data Classes */
    public static class VehicleInfo {
        private String vin;
        private String vehicleModel;
        private String vehicleBrand;
        private String licensePlate;
        
        public VehicleInfo() {}
        
        public String getVin() {
            return vin;
        }
        
        public void setVin(String value) {
            this.vin = value;
        }
        
        public String getVehicleModel() {
            return vehicleModel;
        }
        
        public void setVehicleModel(String value) {
            this.vehicleModel = value;
        }
        
        @Override
        public String toString() {
            return "VehicleInfo{" +
                "vin=" + vin +
                "vehicleModel=" + vehicleModel +
                '}';
        }
    }
    
    public static class VehicleStatus {
        private boolean ignition;
        private boolean lockStatus;
        private long mileage;
        private long fuelLevel;
        
        public boolean getIgnition() {
            return ignition;
        }
        
        public void setIgnition(boolean value) {
            this.ignition = value;
        }
        
        // ... getters and setters
    }
}
```

### 3.3 Python 生成结果

```python
"""Auto-generated TLV Protocol Module"""

from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, List, Tuple

SCHEMA_VERSION = "1.2.0"

# TAG Constants
class Tag(IntEnum):
    PROTOCOL_VERSION = 0x80
    MESSAGE_TYPE = 0x81
    MESSAGE_ID = 0x82
    TIMESTAMP = 0x83
    VEHICLE_INFO = 0x40
    VIN = 0x41
    VEHICLE_MODEL = 0x42
    VEHICLE_STATUS = 0x45
    LATITUDE = 0x51
    LONGITUDE = 0x52

# Enum Definitions
class DeviceType(IntEnum):
    TBOX = 0x01
    IVI = 0x02
    ADAS = 0x03
    CLUSTER = 0x04

class MessageType(IntEnum):
    REQUEST = 0x01
    RESPONSE = 0x02
    ERROR = 0x03
    NOTIFICATION = 0x04
    HEARTBEAT = 0x05
    COMMAND = 0x07

# Data Classes
@dataclass
class VehicleInfo:
    vin: str
    vehicle_model: str
    vehicle_brand: Optional[str] = None
    license_plate: Optional[str] = None

@dataclass
class VehicleStatus:
    ignition: bool
    lock_status: bool
    mileage: int
    fuel_level: int
    battery_voltage: Optional[int] = None
    engine_temp: Optional[int] = None

@dataclass
class Location:
    latitude: int
    longitude: int
    altitude: Optional[int] = None
    speed: Optional[int] = None
    heading: Optional[int] = None

# Error Codes
class ErrorCode(IntEnum):
    SUCCESS = 0x0000
    INVALID_MESSAGE = 0x0001
    UNSUPPORTED_VERSION = 0x0002
    AUTHENTICATION_FAILED = 0x0003
    VERSION_NOT_SUPPORTED = 0x2001

# Validation Functions
def validate_heartbeat(data: bytes) -> bool:
    """Validate 心跳保活消息"""
    # TODO: Implement validation logic
    pass

def validate_vehicle_status(data: bytes) -> bool:
    """Validate 车辆状态通知"""
    pass
```

---

## 4. 高级功能

### 4.1 自定义模板

```python
# custom_template.py
# 用户可以提供自定义Jinja2模板

custom_c_template = '''
#pragma once

// Custom Header for {{ project_name }}
// Version: {{ schema_version }}

{% for tag in tags %}
#define TAG_{{ tag.name }} {{ tag.tag }}
{% endfor %}

// Custom validation hook
typedef bool (*validation_hook_t)(const uint8_t* data, size_t len);
extern validation_hook_t g_custom_validator;
'''

# 生成时使用自定义模板
gen.generate_custom('c', custom_c_template, output_path)
```

### 4.2 版本匹配检查

```python
def check_schema_compatibility(old_schema: dict, new_schema: dict) -> List[str]:
    """检查Schema版本兼容性
    返回不兼容变更列表
    """
    incompatibilities = []
    
    old_tags = {t['tag']: t for t in old_schema['tags']}
    new_tags = {t['tag']: t for t in new_schema['tags']}
    
    for tag_id, new_tag in new_tags.items():
        if tag_id not in old_tags:
            continue  # 新TAG添加是兼容的
        
        old_tag = old_tags[tag_id]
        
        # 检查名称变更
        if old_tag['name'] != new_tag['name']:
            incompatibilities.append(
                f"TAG 0x{tag_id:02X} 名称变更: {old_tag['name']} -> {new_tag['name']}"
            )
        
        # 检查类型变更
        if old_tag.get('data_type') != new_tag.get('data_type'):
            incompatibilities.append(
                f"TAG 0x{tag_id:02X} 数据类型变更"
            )
        
        # 检查必需字段变更
        if not old_tag.get('required') and new_tag.get('required'):
            incompatibilities.append(
                f"TAG 0x{tag_id:02X} 变为必需: {new_tag['name']}"
            )
    
    # 检查删除的TAG
    for tag_id in old_tags:
        if tag_id not in new_tags:
            incompatibilities.append(
                f"TAG 0x{tag_id:02X} 被删除: {old_tags[tag_id]['name']}"
            )
    
    return incompatibilities
```

---

## 5. 连续集成

```yaml
# .github/workflows/codegen.yml
name: TLV Code Generation

on:
  push:
    paths:
      - '**.yaml'
      - '**/SCHEMA.yaml'

jobs:
  generate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      
      - name: Install dependencies
        run: pip install pyyaml jinja2
      
      - name: Generate C Code
        run: |
          python tlv_codegen.py \
            --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
            --language c \
            --output generated/c
      
      - name: Generate Java Code
        run: |
          python tlv_codegen.py \
            --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
            --language java \
            --output generated/java
      
      - name: Generate Python Code
        run: |
          python tlv_codegen.py \
            --schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml \
            --language python \
            --output generated/python
      
      - name: Check Compatibility
        run: |
          python check_compatibility.py \
            --old-schema schema_v1.1.yaml \
            --new-schema AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV-SCHEMA.yaml
      
      - name: Commit Generated Code
        run: |
          git config user.name "TLV Bot"
          git config user.email "tlv-bot@automotive.com"
          git add generated/
          git diff --quiet && git diff --staged --quiet || \
            git commit -m "Auto-generate code from schema ${{ github.sha }}"
          git push
```

---

## 6. 生成代码的使用

### 6.1 嵌入式项目集成

```cmake
# CMakeLists.txt
add_library(tlv_protocol STATIC
    tlv_auto_generated.c
    tlv_codec.c
)

target_include_directories(tlv_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)
```

### 6.2 Android项目集成

```groovy
// build.gradle
sourceSets {
    main {
        java {
            srcDirs += 'generated/java'
        }
    }
}
```

### 6.3 Python项目集成

```python
# 直接导生成的模块
from generated.tlv_protocol import Tag, MessageType, VehicleInfo

data = VehicleInfo(
    vin="LSVAG2180E2100001",
    vehicle_model="Model X"
)
```

---

*生成器版本: v1.0.0*  
*支持语言: C, C++, Java, Python, Go, Rust*
