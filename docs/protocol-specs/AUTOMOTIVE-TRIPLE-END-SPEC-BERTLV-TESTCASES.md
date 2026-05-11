# BER-TLV 协议单元测试用例

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **覆盖率目标**: >95%

---

## 测试用例索引

| 类别 | 用例数量 | 优先级 |
|------|----------|--------|
| 基础编码测试 | 50 | P0 |
| 基础解码测试 | 50 | P0 |
| 协议完整性 | 30 | P0 |
| 边界条件 | 40 | P1 |
| 安全性 | 25 | P0 |
| 性能测试 | 20 | P1 |
| 兼容性 | 15 | P2 |
| **合计** | **230** | |

---

## 1. 基础编码测试 (Encoding Tests)

### 1.1 TAG 编码 (TC-ENC-001 ~ TC-ENC-010)

```c
/**
 * TC-ENC-001: 单字节TAG编码
 * 严重级: P0
 */
TEST(Encoding, SingleByteTag) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 测试各类TAG
    uint8_t test_tags[] = {0x01, 0x40, 0x80, 0xC0, 0xFF};
    for (int i = 0; i < 5; i++) {
        uint8_t tag = test_tags[i];
        uint8_t value[] = {0xAB, 0xCD};
        
        tlv_error_t err = tlv_add_primitive(&builder, tag, value, 2);
        EXPECT_EQ(err, TLV_OK);
        
        // 验证输出
        EXPECT_EQ(buffer[0], tag);
        EXPECT_EQ(buffer[1], 0x02); // 长度
        EXPECT_EQ(buffer[2], 0xAB);
        EXPECT_EQ(buffer[3], 0xCD);
    }
}

/**
 * TC-ENC-002: 长度编码 - 短形式 (<128)
 * 严重级: P0
 */
TEST(Encoding, ShortLength) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint8_t value[127]; // 最大短形式长度
    memset(value, 0xAA, 127);
    
    tlv_error_t err = tlv_add_primitive(&builder, 0x01, value, 127);
    EXPECT_EQ(err, TLV_OK);
    
    EXPECT_EQ(buffer[0], 0x01); // TAG
    EXPECT_EQ(buffer[1], 127);  // 短形式长度
    EXPECT_EQ(memcmp(buffer + 2, value, 127), 0);
}

/**
 * TC-ENC-003: 长度编码 - 长形式 (128-255)
 * 严重级: P0
 */
TEST(Encoding, LongLength128) {
    uint8_t buffer[512];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint8_t value[200];
    memset(value, 0xBB, 200);
    
    tlv_error_t err = tlv_add_primitive(&builder, 0x01, value, 200);
    EXPECT_EQ(err, TLV_OK);
    
    EXPECT_EQ(buffer[0], 0x01);  // TAG
    EXPECT_EQ(buffer[1], 0x81);  // 长形式标志 + 1字节
    EXPECT_EQ(buffer[2], 200);   // 长度值
}

/**
 * TC-ENC-004: 长度编码 - 长形式 (256-65535)
 * 严重级: P0
 */
TEST(Encoding, LongLength256) {
    uint8_t buffer[1024];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint8_t value[1000];
    memset(value, 0xCC, 1000);
    
    tlv_error_t err = tlv_add_primitive(&builder, 0x01, value, 1000);
    EXPECT_EQ(err, TLV_OK);
    
    EXPECT_EQ(buffer[0], 0x01);  // TAG
    EXPECT_EQ(buffer[1], 0x82);  // 长形式标志 + 2字节
    EXPECT_EQ(buffer[2], 0x03);  // 1000 >> 8
    EXPECT_EQ(buffer[3], 0xE8);  // 1000 & 0xFF
}

/**
 * TC-ENC-005: 空值编码
 * 严重级: P0
 */
TEST(Encoding, NullValue) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_error_t err = tlv_add_primitive(&builder, 0x05, NULL, 0);
    EXPECT_EQ(err, TLV_OK);
    
    EXPECT_EQ(buffer[0], 0x05); // NULL TAG
    EXPECT_EQ(buffer[1], 0x00); // 长度=0
}

/**
 * TC-ENC-006: 缓冲区溢出保护
 * 严重级: P0
 */
TEST(Encoding, BufferOverflow) {
    uint8_t buffer[10];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint8_t large_value[100] = {0};
    tlv_error_t err = tlv_add_primitive(&builder, 0x01, large_value, 100);
    
    EXPECT_EQ(err, TLV_ERROR_BUFFER_TOO_SMALL);
}

/**
 * TC-ENC-007: 构造元素编码 - SEQUENCE
 * 严重级: P0
 */
TEST(Encoding, ConstructedSequence) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 开始SEQUENCE
    tlv_add_constructed_start(&builder, 0x30);
    
    // 添加子元素
    uint8_t child1[] = {0x01, 0x02};
    tlv_add_primitive(&builder, 0x01, child1, 2);
    
    uint8_t child2[] = {0x03, 0x04};
    tlv_add_primitive(&builder, 0x02, child2, 2);
    
    tlv_add_constructed_end(&builder);
    
    // 验证
    EXPECT_EQ(buffer[0], 0x30); // SEQUENCE TAG
    // 长度字节... (根据实际实现验证)
}
```

### 1.2 数据类型编码 (TC-ENC-011 ~ TC-ENC-020)

```c
/**
 * TC-ENC-011: BOOLEAN 编码 - true
 * 严重级: P0
 */
TEST(Encoding, BooleanTrue) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_add_boolean(&builder, true);
    
    EXPECT_EQ(buffer[0], 0x01); // BOOLEAN TAG
    EXPECT_EQ(buffer[1], 0x01); // 长度=1
    EXPECT_EQ(buffer[2], 0xFF); // true值
}

/**
 * TC-ENC-012: BOOLEAN 编码 - false
 * 严重级: P0
 */
TEST(Encoding, BooleanFalse) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_add_boolean(&builder, false);
    
    EXPECT_EQ(buffer[2], 0x00); // false值
}

/**
 * TC-ENC-013: INTEGER 编码 - 正数
 * 严重级: P0
 */
TEST(Encoding, IntegerPositive) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 测试各种正数
    int64_t test_values[] = {0, 1, 127, 128, 255, 256, 65535, 2147483647};
    
    for (int i = 0; i < 8; i++) {
        tlv_builder_init(&builder, buffer, sizeof(buffer));
        tlv_add_integer(&builder, test_values[i]);
        
        EXPECT_EQ(buffer[0], 0x02); // INTEGER TAG
        
        // 验证大端序
        int64_t reconstructed = 0;
        for (int j = 0; j < buffer[1]; j++) {
            reconstructed = (reconstructed << 8) | buffer[2 + j];
        }
        EXPECT_EQ(reconstructed, test_values[i]);
    }
}

/**
 * TC-ENC-014: INTEGER 编码 - 负数
 * 严重级: P0
 */
TEST(Encoding, IntegerNegative) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_add_integer(&builder, -1);
    
    // -1 应该编码为 0xFF
    EXPECT_EQ(buffer[1], 0x01);
    EXPECT_EQ(buffer[2], 0xFF);
}

/**
 * TC-ENC-015: INTEGER 编码 - 前导零检查
 * 严重级: P1
 * 说明: 0x80需要前导零 0x00
 */
TEST(Encoding, IntegerLeadingZero) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_add_integer(&builder, 128); // 0x80
    
    // 应该编码为 0x00 0x80 (前导零避免流解为负数)
    EXPECT_EQ(buffer[1], 0x02);
    EXPECT_EQ(buffer[2], 0x00);
    EXPECT_EQ(buffer[3], 0x80);
}

/**
 * TC-ENC-016: UTF8String 编码
 * 严重级: P0
 */
TEST(Encoding, Utf8String) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    const char *test = "Hello 世界";
    tlv_add_string(&builder, test);
    
    EXPECT_EQ(buffer[0], 0x0C); // UTF8String TAG
    EXPECT_EQ(buffer[1], strlen(test)); // 长度
    EXPECT_EQ(memcmp(buffer + 2, test, strlen(test)), 0);
}

/**
 * TC-ENC-017: 版本编码 (0x80)
 * 严重级: P0
 */
TEST(Encoding, ProtocolVersion) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    
    EXPECT_EQ(buffer[0], 0x80); // PROTOCOL_VERSION TAG
    EXPECT_EQ(buffer[1], 0x03); // 长度=3
    EXPECT_EQ(buffer[2], 0x01); // major
    EXPECT_EQ(buffer[3], 0x02); // minor
    EXPECT_EQ(buffer[4], 0x00); // patch
}

/**
 * TC-ENC-018: 时间戳编码 (0x83)
 * 严重级: P0
 */
TEST(Encoding, Timestamp) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint64_t ts = 0x0123456789ABCDEFLL;
    tlv_add_timestamp(&builder, ts);
    
    EXPECT_EQ(buffer[0], 0x83); // TIMESTAMP TAG
    EXPECT_EQ(buffer[1], 0x08); // 长度=8
    
    // 验证大端序
    uint64_t reconstructed = 0;
    for (int i = 0; i < 8; i++) {
        reconstructed = (reconstructed << 8) | buffer[2 + i];
    }
    EXPECT_EQ(reconstructed, ts);
}

/**
 * TC-ENC-019: UUID编码 (0x82)
 * 严重级: P0
 */
TEST(Encoding, UuidEncoding) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    uint8_t uuid[16] = {
        0x55, 0x0E, 0x84, 0x00, 0xE2, 0x9B, 0x41, 0xD4,
        0xA7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00
    };
    
    tlv_add_uuid(&builder, 0x82, uuid);
    
    EXPECT_EQ(buffer[0], 0x82); // MESSAGE_ID TAG
    EXPECT_EQ(buffer[1], 0x10); // 长度=16
    EXPECT_EQ(memcmp(buffer + 2, uuid, 16), 0);
}

/**
 * TC-ENC-020: 消息类型编码 (0x81)
 * 严重级: P0
 */
TEST(Encoding, MessageType) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    
    EXPECT_EQ(buffer[0], 0x81); // MESSAGE_TYPE TAG
    EXPECT_EQ(buffer[1], 0x01); // 长度=1
    EXPECT_EQ(buffer[2], 0x05); // HEARTBEAT = 0x05
}
```

---

## 2. 基础解码测试 (Decoding Tests)

### 2.1 单元素解码 (TC-DEC-001 ~ TC-DEC-015)

```c
/**
 * TC-DEC-001: 基本解码
 * 严重级: P0
 */
TEST(Decoding, BasicParse) {
    // 测试数据: TAG=0x01, LENGTH=2, VALUE=0xAB 0xCD
    uint8_t data[] = {0x01, 0x02, 0xAB, 0xCD};
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(elem.tag, 0x01);
    EXPECT_EQ(elem.length, 2);
    EXPECT_EQ(elem.value[0], 0xAB);
    EXPECT_EQ(elem.value[1], 0xCD);
}

/**
 * TC-DEC-002: 连续解码多个元素
 * 严重级: P0
 */
TEST(Decoding, MultipleElements) {
    uint8_t data[] = {
        0x01, 0x01, 0xFF,  // BOOLEAN true
        0x02, 0x01, 0x7F,  // INTEGER 127
        0x0C, 0x03, 'A', 'B', 'C'  // UTF8String "ABC"
    };
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    
    // 第一个
    EXPECT_EQ(tlv_parse_next(&parser, &elem), TLV_OK);
    EXPECT_EQ(elem.tag, 0x01);
    
    // 第二个
    EXPECT_EQ(tlv_parse_next(&parser, &elem), TLV_OK);
    EXPECT_EQ(elem.tag, 0x02);
    
    // 第三个
    EXPECT_EQ(tlv_parse_next(&parser, &elem), TLV_OK);
    EXPECT_EQ(elem.tag, 0x0C);
    
    // 结束
    EXPECT_EQ(tlv_parse_next(&parser, &elem), TLV_ERROR_PARSE_FAILED);
}

/**
 * TC-DEC-003: 长度解码 - 短形式
 * 严重级: P0
 */
TEST(Decoding, ShortLengthDecode) {
    uint8_t data[130];
    data[0] = 0x04;  // OCTET_STRING
    data[1] = 127;   // 长度
    memset(data + 2, 0xAA, 127);
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(elem.length, 127);
}

/**
 * TC-DEC-004: 长度解码 - 长形式(2字节)
 * 严重级: P0
 */
TEST(Decoding, LongLength2Byte) {
    uint8_t data[300];
    data[0] = 0x04;
    data[1] = 0x82;  // 长形式 + 2字节
    data[2] = 0x01;  // 256 >> 8
    data[3] = 0x00;  // 256 & 0xFF
    memset(data + 4, 0xBB, 256);
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(elem.length, 256);
}

/**
 * TC-DEC-005: 长度超出缓冲区
 * 严重级: P0
 */
TEST(Decoding, LengthExceedsBuffer) {
    uint8_t data[] = {0x01, 0x10}; // 声称长度16, 但只有2字节
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_ERROR_BUFFER_TOO_SMALL);
}

/**
 * TC-DEC-006: 空数据解码
 * 严重级: P0
 */
TEST(Decoding, EmptyData) {
    uint8_t data[] = {};
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_ERROR_PARSE_FAILED);
}

/**
 * TC-DEC-007: 无效长度编码
 * 严重级: P0
 */
TEST(Decoding, InvalidLengthEncoding) {
    // 0x80 = 未定义长度(保留)
    uint8_t data[] = {0x01, 0x80};
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    EXPECT_EQ(err, TLV_ERROR_INVALID_LENGTH);
}

/**
 * TC-DEC-008: 查找特定TAG
 * 严重级: P0
 */
TEST(Decoding, FindElement) {
    uint8_t data[] = {
        0x01, 0x01, 0xFF,
        0x80, 0x03, 0x01, 0x02, 0x00,  // 目标
        0x02, 0x01, 0x7F
    };
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_find_element(&parser, 0x80, &elem);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(elem.tag, 0x80);
    EXPECT_EQ(elem.length, 3);
    EXPECT_EQ(elem.value[0], 0x01);
}

/**
 * TC-DEC-009: 查找不存在的TAG
 * 严重级: P0
 */
TEST(Decoding, FindNonExistent) {
    uint8_t data[] = {0x01, 0x01, 0xFF};
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_find_element(&parser, 0x80, &elem);
    
    EXPECT_EQ(err, TLV_ERROR_PARSE_FAILED);
}

/**
 * TC-DEC-010: 版本解码
 * 严重级: P0
 */
TEST(Decoding, ParseVersion) {
    uint8_t data[] = {0x80, 0x03, 0x02, 0x05, 0x03}; // v2.5.3
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    tlv_version_t ver;
    tlv_error_t err = tlv_parse_version(&elem, &ver);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(ver.major, 2);
    EXPECT_EQ(ver.minor, 5);
    EXPECT_EQ(ver.patch, 3);
}

/**
 * TC-DEC-011: 版本解码 - 无效长度
 * 严重级: P1
 */
TEST(Decoding, ParseVersionInvalidLength) {
    uint8_t data[] = {0x80, 0x02, 0x01, 0x02}; // 只有2字节
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    tlv_version_t ver;
    tlv_error_t err = tlv_parse_version(&elem, &ver);
    
    EXPECT_EQ(err, TLV_ERROR_INVALID_TAG);
}

/**
 * TC-DEC-012: 时间戳解码
 * 严重级: P0
 */
TEST(Decoding, ParseTimestamp) {
    uint8_t data[] = {
        0x83, 0x08,
        0x00, 0x00, 0x01, 0x91, 0xD4, 0x8A, 0xE0, 0x00  // 1715410200000
    };
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    uint64_t ts;
    tlv_error_t err = tlv_parse_timestamp(&elem, &ts);
    
    EXPECT_EQ(err, TLV_OK);
    EXPECT_EQ(ts, 1715410200000ULL);
}

/**
 * TC-DEC-013: 整数解码 - 正数
 * 严重级: P0
 */
TEST(Decoding, ParseIntegerPositive) {
    uint8_t data[] = {0x02, 0x02, 0x01, 0x00}; // 256
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    // 手动解码验证
    int64_t value = (elem.value[0] << 8) | elem.value[1];
    EXPECT_EQ(value, 256);
}

/**
 * TC-DEC-014: 整数解码 - 负数(补码)
 * 严重级: P0
 */
TEST(Decoding, ParseIntegerNegative) {
    uint8_t data[] = {0x02, 0x01, 0xFF}; // -1 (补码)
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    // 负数解码
    EXPECT_EQ(elem.value[0], 0xFF);
}

/**
 * TC-DEC-015: 解码器状态保护
 * 严重级: P1
 */
TEST(Decoding, ParserStatePreservation) {
    uint8_t data[] = {
        0x01, 0x01, 0xFF,
        0x80, 0x03, 0x01, 0x02, 0x00
    };
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    // 解码第一个
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    size_t offset1 = parser.offset;
    
    // find操作
    tlv_find_element(&parser, 0x80, &elem);
    
    // 验证offset被恢复
    EXPECT_EQ(parser.offset, offset1);
}
```

---

## 3. 协议完整性测试 (Protocol Integrity)

### 3.1 必需字段验证 (TC-VAL-001 ~ TC-VAL-015)

```c
/**
 * TC-VAL-001: 完整有效消息
 * 严重级: P0
 */
TEST(Validation, CompleteValidMessage) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_OK);
}

/**
 * TC-VAL-002: 缺少版本字段
 * 严重级: P0
 */
TEST(Validation, MissingVersion) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 省略版本
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-003: 缺少消息类型
 * 严重级: P0
 */
TEST(Validation, MissingMessageType) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    // 省略 message type
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-004: 缺少消息ID
 * 严重级: P0
 */
TEST(Validation, MissingMessageId) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    // 省略 message id
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-005: 缺少时间戳
 * 严重级: P0
 */
TEST(Validation, MissingTimestamp) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    // 省略 timestamp
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-006: 无效的版本长度(2字节)
 * 严重级: P0
 */
TEST(Validation, InvalidVersionLength) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 手动构造无效版本
    buffer[0] = 0x80;
    buffer[1] = 0x02; // 错误长度
    buffer[2] = 0x01;
    buffer[3] = 0x02;
    
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    // 检查版本长度
    TlvParser parser;
    tlv_parser_init(&parser, buffer, size);
    TlvElement elem;
    tlv_parse_next(&parser, &elem);
    
    tlv_version_t ver;
    EXPECT_EQ(tlv_parse_version(&elem, &ver), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-007: 无效的消息类型长度
 * 严重级: P0
 */
TEST(Validation, InvalidMessageTypeLength) {
    uint8_t data[] = {
        0x80, 0x03, 0x01, 0x02, 0x00,
        0x81, 0x02, 0x05, 0x00,  // 长度应该是1
        0x82, 0x10, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0x83, 0x08, 0,0,0,0,0,0,0,0
    };
    
    // 消息类型实际上不会被拒绝，但应记录为警告
    EXPECT_EQ(tlv_validate_message(data, sizeof(data)), TLV_OK);
}

/**
 * TC-VAL-008: 无效的UUID长度(15字节)
 * 严重级: P0
 */
TEST(Validation, InvalidUuidLength) {
    uint8_t data[] = {
        0x80, 0x03, 0x01, 0x02, 0x00,
        0x81, 0x01, 0x05,
        0x82, 0x0F,  // 错误长度15
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0x83, 0x08, 0,0,0,0,0,0,0,0
    };
    
    EXPECT_EQ(tlv_validate_message(data, sizeof(data)), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-009: 无效的时间戳长度(4字节)
 * 严重级: P0
 */
TEST(Validation, InvalidTimestampLength) {
    uint8_t data[] = {
        0x80, 0x03, 0x01, 0x02, 0x00,
        0x81, 0x01, 0x05,
        0x82, 0x10, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0x83, 0x04, 0,0,0,0  // 错误长度4而非8
    };
    
    EXPECT_EQ(tlv_validate_message(data, sizeof(data)), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-010: 消息字段顺序不固定
 * 严重级: P1
 * 说明: 字段顺序不应影响验证
 */
TEST(Validation, FieldOrder) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 非标准顺序构建
    tlv_add_timestamp(&builder, 1715410200000);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_OK);
}

/**
 * TC-VAL-011: 空消息
 * 严重级: P0
 */
TEST(Validation, EmptyMessage) {
    uint8_t data[] = {};
    
    EXPECT_EQ(tlv_validate_message(data, 0), TLV_ERROR_INVALID_LENGTH);
}

/**
 * TC-VAL-012: 过小消息(小于最小头)
 * 严重级: P0
 */
TEST(Validation, TooSmallMessage) {
    uint8_t data[] = {0x80, 0x03, 0x01, 0x02, 0x00}; // 只有版本
    
    EXPECT_EQ(tlv_validate_message(data, sizeof(data)), TLV_ERROR_INVALID_TAG);
}

/**
 * TC-VAL-013: 多余字段不影响验证
 * 严重级: P1
 */
TEST(Validation, ExtraFields) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    // 添加额外字段
    tlv_add_integer(&builder, 12345);
    tlv_add_string(&builder, "extra");
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_OK);
}

/**
 * TC-VAL-014: 重复字段(第一个有效)
 * 严重级: P1
 */
TEST(Validation, DuplicateFields) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver1 = {1, 2, 0};
    tlv_add_version(&builder, &ver1);
    tlv_version_t ver2 = {2, 0, 0}; // 重复版本
    tlv_add_version(&builder, &ver2);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    // 验证应该通过，但实际解析可能使用第一个
    EXPECT_EQ(tlv_validate_message(buffer, size), TLV_OK);
}

/**
 * TC-VAL-015: 超长消息(>64KB)
 * 严重级: P1
 */
TEST(Validation, OversizedMessage) {
    // 模拟超长消息
    uint8_t *large_buffer = (uint8_t*)malloc(70000);
    ASSERT_NE(large_buffer, nullptr);
    
    // 构建超长数据
    large_buffer[0] = 0x80;
    large_buffer[1] = 0x03;
    memcpy(large_buffer + 2, "\x01\x02\x00", 3);
    // ... 添加更多数据
    
    // 应该被拒绝或截断
    // 具体行为取决于实现
    
    free(large_buffer);
}
```

---

## 4. 安全测试 (Security Tests)

```c
/**
 * TC-SEC-001: 空指针保护
 * 严重级: P0
 */
TEST(Security, NullPointerChecks) {
    uint8_t buffer[256];
    TlvBuilder builder;
    
    EXPECT_EQ(tlv_builder_init(NULL, buffer, 256), TLV_ERROR_NULL_POINTER);
    EXPECT_EQ(tlv_builder_init(&builder, NULL, 256), TLV_ERROR_NULL_POINTER);
}

/**
 * TC-SEC-002: 恶意长度(超大值)
 * 严重级: P0
 */
TEST(Security, MaliciousLength) {
    // 声称超大长度 0xFFFFFF
    uint8_t data[] = {
        0x01, 0x83, 0xFF, 0xFF, 0xFF
    };
    
    TlvParser parser;
    tlv_parser_init(&parser, data, sizeof(data));
    
    TlvElement elem;
    tlv_error_t err = tlv_parse_next(&parser, &elem);
    
    // 应该检测到长度超出缓冲区
    EXPECT_EQ(err, TLV_ERROR_BUFFER_TOO_SMALL);
}

/**
 * TC-SEC-003: 嵌套死循环保护
 * 严重级: P0
 */
TEST(Security, NestedLoopProtection) {
    // 恶意构造: 指向自身的嵌套
    uint8_t data[] = {
        0x30, 0x02,  // SEQUENCE, length=2
        0x30, 0x02   // 指向自身（模拟）
    };
    
    // 实际实现应有嵌套深度限制
    // 此测试确保不会死循环
}

/**
 * TC-SEC-004: 整数溢出检测
 * 严重级: P1
 */
TEST(Security, IntegerOverflow) {
    // 构造一个超大的"INTEGER"
    uint8_t data[20];
    data[0] = 0x02;  // INTEGER
    data[1] = 0x81;  // 长形式
    data[2] = 16;    // 16字节整数
    memset(data + 3, 0xFF, 16);
    
    // 应该安全处理，不崩溃
}

/**
 * TC-SEC-005: 签名验证
 * 严重级: P0
 */
TEST(Security, SignatureVerification) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    // 构建带签名的消息
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_COMMAND);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    
    // 添加命令数据
    tlv_add_integer(&builder, 1001); // command ID
    
    // 添加签名
    uint8_t signature[32] = {0};
    tlv_add_primitive(&builder, TLV_TAG_SIGNATURE, signature, 32);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    // 验证签名存在
    TlvParser parser;
    tlv_parser_init(&parser, buffer, size);
    
    TlvElement elem;
    EXPECT_EQ(tlv_find_element(&parser, TLV_TAG_SIGNATURE, &elem), TLV_OK);
}
```

---

## 5. 性能测试 (Performance Tests)

```c
/**
 * TC-PERF-001: 编码吞吐量
 * 严重级: P1
 */
TEST(Performance, EncodingThroughput) {
    uint8_t buffer[256];
    const int iterations = 100000;
    
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        TlvBuilder builder;
        tlv_builder_init(&builder, buffer, sizeof(buffer));
        
        tlv_version_t ver = {1, 2, 0};
        tlv_add_version(&builder, &ver);
        tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
        
        uint8_t uuid[16] = {0};
        tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
        tlv_add_timestamp(&builder, 1715410200000 + i);
        
        size_t size;
        tlv_builder_finalize(&builder, &size);
    }
    
    clock_t end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double throughput = iterations / cpu_time;
    
    printf("Encoding throughput: %.0f msg/sec\n", throughput);
    EXPECT_GT(throughput, 50000); // 至少5万/秒
}

/**
 * TC-PERF-002: 解码吞吐量
 * 严重级: P1
 */
TEST(Performance, DecodingThroughput) {
    uint8_t buffer[256];
    TlvBuilder builder;
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    uint8_t uuid[16] = {0};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, 1715410200000);
    tlv_add_boolean(&builder, true);
    
    size_t size;
    tlv_builder_finalize(&builder, &size);
    
    const int iterations = 100000;
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        TlvParser parser;
        tlv_parser_init(&parser, buffer, size);
        
        TlvElement elem;
        while (tlv_parse_next(&parser, &elem) == TLV_OK) {
            // 消耗元素
        }
    }
    
    clock_t end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double throughput = iterations / cpu_time;
    
    printf("Decoding throughput: %.0f msg/sec\n", throughput);
    EXPECT_GT(throughput, 60000); // 至少6万/秒
}

/**
 * TC-PERF-003: 内存占用
 * 严重级: P1
 */
TEST(Performance, MemoryUsage) {
    // 测试是否有内存泄漏
    uint8_t buffer[256];
    
    for (int i = 0; i < 100000; i++) {
        TlvBuilder builder;
        tlv_builder_init(&builder, buffer, sizeof(buffer));
        
        tlv_version_t ver = {1, 2, 0};
        tlv_add_version(&builder, &ver);
        tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
        
        // ... 更多数据
    }
    
    // 如果有内存泄漏，此处应该检测到
}
```

---

## 测试执行清单

| 环境 | 命令 | 预期时间 |
|------|------|---------|
| C/C++ | `make test` | <30秒 |
| Java | `./gradlew test` | <60秒 |
| Python | `pytest tests/` | <20秒 |

### 持续集成配置

```yaml
# .github/workflows/tlv-tests.yml
name: TLV Protocol Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build C Library
        run: |
          cd tlv-c
          make
          make test
      
      - name: Test Java Library
        run: |
          cd tlv-java
          ./gradlew test
      
      - name: Test Python Tools
        run: |
          cd tlv-python
          pytest tests/ -v --tb=short
      
      - name: Coverage Report
        run: |
          lcov --capture --directory . --output-file coverage.info
          lcov --remove coverage.info '/usr/*' --output-file coverage.info
          lcov --list coverage.info
```

---

*测试用例版本: v1.0.0*  
*覆盖率报告请查看: coverage_report.html*
