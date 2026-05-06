# BER-TLV 编码规范

**版本**: 1.0.0
**日期**: 2026-05-06
**参考**: ISO 7816-4, EMVco Book 3

---

## 1. BERTLV概述

BERTLV = Basic Encoding Rules / Tag-Length-Value

```
┌─────┬─────┬─────────────────────────┐
│ Tag │ Len │ Value                   │
│ 1-2 │ 1-3 │ Length                  │
│Byte │Byte │ Variable                │
└─────┴─────┴─────────────────────────┘
```

---

## 2. Tag编码

### 2.1 Tag结构

| 位 | 说明 |
|----|------|
| b8-b7 | 类别 (00=universal, 01=application, 10=context-specific, 11=private) |
| b6 | 结构 (0=primitive, 1=constructed) |
| b5-b1 | Tag编号 |

### 2.2 Tag示例

| Tag | 二进制 | 说明 |
|-----|--------|------|
| 01 | 00000001 | Universal, Primitive, Tag=1 |
| 02 | 00000010 | Universal, Primitive, Tag=2 |
| 30 | 00110000 | Universal, Constructed, Tag=16 |
| 4F | 01001111 | Application, Primitive, Tag=15 |
| 61 | 01100001 | Application, Constructed, Tag=17 |
| A0 | 10100000 | Context-specific, Constructed, Tag=0 |
| E1 | 11100001 | Private, Primitive, Tag=1 |

### 2.3 多字节Tag

当Tag编号>31时，使用多字节编码:

```
Tag = 0x1F + 后续字节
```

| Tag | 编码 | 说明 |
|-----|------|------|
| 1F | 1F | 后续1字节 |
| 20 | 1F 20 | Tag=32 |
| FF | 1F FF | Tag=158 |
| 100 | 1F 81 00 | Tag=256 |
| 1FFF | 1F FF 7F | Tag=8191 |

---

## 3. Length编码

### 3.1 短格式 (00-7F)

```
┌─────────────────────────┐
│ b7=0                    │
│ b6-b0 = 长度值          │
└─────────────────────────┘
```

| Length值 | 编码 |
|----------|------|
| 0 | 00 |
| 1 | 01 |
| 127 | 7F |

### 3.2 长格式 (80-FF)

```
┌─────────┬───────────────┐
│ 第一个字节 │ 后续字节          │
│ b7=1    │ 表示长度值          │
│ b6-b0=后续字节数 │
└─────────┴─────────────────┘
```

| 实际长度 | 编码 |
|---------|------|
| 128 | 81 80 |
| 255 | 81 FF |
| 256 | 82 01 00 |
| 1000 | 82 03 E8 |
| 65535 | 83 FF FF |

---

## 4. Value编码

### 4.1 数据类型

| 类型 | Tag | 格式 | 说明 |
|------|-----|------|------|
| NULL | 02 | 空 | 空值 |
| INTEGER | 02 | B | 有符号整数 |
| BIT STRING | 03 | B | 位串 |
| OCTET STRING | 04 | B | 字节串 |
| UTF8String | 0C | UTF8 | UTF-8字符串 |
| NUMERIC | 12 | N | 数字字符串 |
| ALPHANUMERIC | 13 | AN | 字母数字字符串 |

### 4.2 构造类型 (Constructed)

Value由多个BERTLV组成:

```
30 0A           // Constructed, Length=10
   01 01 30     // Tag=1, Len=1, Value=0x30
   02 01 2B     // Tag=2, Len=1, Value=0x2B
   04 03 01 02 03 // Tag=4, Len=3, Value=0x01 0x02 0x03
```

---

## 5. 编码规则详解

### 5.1 数字字符串 (NUMERIC, Tag=12)

```
// 手机号 "13812345678"
12 0B 31 33 38 31 32 33 34 35 36 37 38
```

### 5.2 带符号数字 (NUMERIC with Sign)

```
// 温度 "-12.3°C"
NS 04 2D 31 32 33  // "-" + "123" = -123 (表示0.1°C)

// 负数编码: 2D为负号
```

### 5.3 BCD编码

```
// 时间 "20260506085000"
BCD 0E 20 26 05 06 08 50 00
```

### 5.4 位掩码

```
// AccessLevel = 0x0F (二进制 00001111)
B 01 0F  // 1字节位掩码

// DoorStatus = 左前门+右前门开 = 0x03
B 01 03
```

### 5.5 嵌套结构

```
// PairingInfo 结构
A0 10          // Tag=A0(Constructed), Len=16
   01 06 41 42 43 44 45 46 // BleMac = "ABCDEF"
   02 04 01 02 03 04      // UwbChannel = 4字节
```

---

## 6. 编码示例

### 6.1 简单消息

```
// KeyBind请求
A0 01 11 56 48 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35
   // Tag=A0(Constructed), Len=17, VehicleId="VH123456789012345"
A0 07 01 30 31  // KeyType=01
A0 08 02 30 30 46  // AccessLevel=0x0046

// 完整编码:
A0 17
   A0 11 56 48 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35
   A0 01 01 30 31
   A0 02 02 30 30 46
```

### 6.2 完整消息封装

```
// Header
E1 01 04 01 30 00        // Version = 1.0
E1 02 0E 32 30 32 36 30 35 30 36 30 38 35 30 30 30 // Timestamp
E1 03 02 31 30 30 31     // MessageType = 1001
E1 04 04 30 30 30 30 31   // SequenceNo = 00001

// Body
A0 01 10 4B 45 59 31 32 33 34 35 36 37 38 39 30 31 32 33 34 // KeyId
A0 02 01 30 30           // Status = 00

// Trailer
E1 FF 01 20 [HMAC-SHA256-32字节] // Signature

// 完整消息:
E1 01 04 01 30 00
E1 02 0E 32 30 32 36 30 35 30 36 30 38 35 30 30 30
E1 03 02 31 30 30 31
E1 04 04 30 30 30 30 31
A0 01 10 4B 45 59 31 32 33 34 35 36 37 38 39 30 31 32 33 34
A0 02 01 30 30
E1 FF 01 20 [32字节签名]
```

---

## 7. 解码规则

### 7.1 解码流程

```
1. 读取Tag (1-3字节)
2. 读取Length (1-3字节)
3. 根据Tag判断Value类型
4. 读取Value
5. 如果是Constructed, 递归解码子元素
```

### 7.2 解码伪代码

```python
def decode_tlv(data):
    pos = 0
    results = []
    
    while pos < len(data):
        # 1. Tag
        tag = data[pos]
        pos += 1
        if tag & 0x1F == 0x1F:
            while data[pos] & 0x80:
                tag = (tag << 8) | data[pos]
                pos += 1
        
        # 2. Length
        length = data[pos]
        pos += 1
        if length & 0x80:
            num_bytes = length & 0x7F
            length = 0
            for i in range(num_bytes):
                length = (length << 8) | data[pos]
                pos += 1
        
        # 3. Value
        value = data[pos:pos+length]
        pos += length
        
        # 4. Check if constructed
        if tag & 0x20:
            # Recursive decode
            value = decode_tlv(value)
        
        results.append((tag, length, value))
    
    return results
```

---

## 8. 错误处理

### 8.1 无效编码

| 错误码 | 说明 |
|--------|------|
| E001 | 无效Tag |
| E002 | 无效Length |
| E003 | 数据截断 |
| E004 | 嵌套深度超限 |
| E005 | 不支持的Tag |

---

## 9. 版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
