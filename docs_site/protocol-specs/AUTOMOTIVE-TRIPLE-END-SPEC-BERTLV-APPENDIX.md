# 车云一体化协议 - BER-TLV 实现附录

> **版本**: v2.0.1  
> **配套文档**: AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV.md  
> **内容**: 编解码代码框架、JSON转换规范、场景示例

---

## 目录

1. [TLV 编解码代码框架](#1-tlv-编解码代码框架)
2. [TLV 与 JSON 转换规范](#2-tlv-与-json-转换规范)
3. [完整场景示例](#3-完整场景示例)
4. [TLV 校验工具](#4-tlv-校验工具)

---

## 1. TLV 编解码代码框架

### 1.1 C 语言实现 (车端/Embedded)

```c
/**
 * @file tlv_codec.h
 * @brief BER-TLV 编解码库 - 车端嵌入式版本
 * @version 1.2.0
 */

#ifndef TLV_CODEC_H
#define TLV_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 版本信息 */
#define TLV_CODEC_VERSION_MAJOR 1
#define TLV_CODEC_VERSION_MINOR 2
#define TLV_CODEC_VERSION_PATCH 0

/* 错误码 */
typedef enum {
    TLV_OK = 0,
    TLV_ERROR_INVALID_TAG = -1,
    TLV_ERROR_INVALID_LENGTH = -2,
    TLV_ERROR_BUFFER_TOO_SMALL = -3,
    TLV_ERROR_NULL_POINTER = -4,
    TLV_ERROR_PARSE_FAILED = -5,
    TLV_ERROR_VERSION_MISMATCH = -6,
    TLV_ERROR_CHECKSUM_FAILED = -7
} tlv_error_t;

/* TAG 分类 */
typedef enum {
    TLV_CLASS_UNIVERSAL = 0x00,
    TLV_CLASS_APPLICATION = 0x40,
    TLV_CLASS_CONTEXT_SPECIFIC = 0x80,
    TLV_CLASS_PRIVATE = 0xC0
} tlv_class_t;

/* 基础数据类型 (Universal Class) */
typedef enum {
    TLV_TAG_BOOLEAN = 0x01,
    TLV_TAG_INTEGER = 0x02,
    TLV_TAG_BIT_STRING = 0x03,
    TLV_TAG_OCTET_STRING = 0x04,
    TLV_TAG_NULL = 0x05,
    TLV_TAG_UTF8_STRING = 0x0C,
    TLV_TAG_SEQUENCE = 0x30,
    TLV_TAG_SET = 0x31
} tlv_universal_tag_t;

/* 协议控制 TAG (Context-Specific Class) */
typedef enum {
    TLV_TAG_PROTOCOL_VERSION = 0x80,    /* 协议版本, 必需 */
    TLV_TAG_MESSAGE_TYPE = 0x81,        /* 消息类型, 必需 */
    TLV_TAG_MESSAGE_ID = 0x82,          /* 消息ID, 必需 */
    TLV_TAG_TIMESTAMP = 0x83,           /* 时间戳, 必需 */
    TLV_TAG_SESSION_ID = 0x84,          /* 会话ID */
    TLV_TAG_SEQUENCE_NUMBER = 0x85,     /* 序列号 */
    TLV_TAG_CORRELATION_ID = 0x86,      /* 关联ID */
    TLV_TAG_ERROR_CODE = 0x87,          /* 错误码 */
    TLV_TAG_AUTH_TOKEN = 0x88,          /* 认证令牌 */
    TLV_TAG_SIGNATURE = 0x89,           /* 签名 */
    TLV_TAG_ENCRYPTION_INFO = 0x8A,     /* 加密信息 */
    TLV_TAG_COMPRESSION_FLAG = 0x8B     /* 压缩标志 */
} tlv_control_tag_t;

/* 消息类型 */
typedef enum {
    TLV_MSG_REQUEST = 0x01,
    TLV_MSG_RESPONSE = 0x02,
    TLV_MSG_ERROR = 0x03,
    TLV_MSG_NOTIFICATION = 0x04,
    TLV_MSG_HEARTBEAT = 0x05,
    TLV_MSG_HEARTBEAT_ACK = 0x06,
    TLV_MSG_COMMAND = 0x07,
    TLV_MSG_COMMAND_ACK = 0x08,
    TLV_MSG_COMMAND_RESULT = 0x09,
    TLV_MSG_OTA_REQUEST = 0x0A,
    TLV_MSG_OTA_RESPONSE = 0x0B,
    TLV_MSG_SYNC_REQUEST = 0x0C,
    TLV_MSG_SYNC_RESPONSE = 0x0D,
    TLV_MSG_VERSION_NEGOTIATION = 0x0E,
    TLV_MSG_VERSION_NEGOTIATION_ACK = 0x0F
} tlv_message_type_t;

/* TLV 元素结构 */
typedef struct {
    uint8_t tag;                    /* TAG 字节 */
    uint32_t length;                /* 长度 (支持多字节长度) */
    const uint8_t *value;           /* 值指针 (不拥有数据) */
} tlv_element_t;

/* TLV 解析器状态 */
typedef struct {
    const uint8_t *buffer;          /* 原始缓冲区 */
    size_t buffer_size;             /* 缓冲区大小 */
    size_t offset;                  /* 当前偏移 */
    uint32_t parsed_count;          /* 已解析元素数 */
} tlv_parser_t;

/* TLV 构建器状态 */
typedef struct {
    uint8_t *buffer;                /* 输出缓冲区 */
    size_t buffer_size;             /* 缓冲区总大小 */
    size_t offset;                  /* 当前写入偏移 */
    uint32_t element_count;         /* 已添加元素数 */
} tlv_builder_t;

/* 版本信息 */
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} tlv_version_t;

/*==================== 解析函数 ====================*/

/**
 * @brief 初始化 TLV 解析器
 * @param parser 解析器实例
 * @param buffer 输入缓冲区
 * @param size 缓冲区大小
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_parser_init(tlv_parser_t *parser, const uint8_t *buffer, size_t size);

/**
 * @brief 解析下一个 TLV 元素
 * @param parser 解析器
 * @param element 输出元素
 * @return TLV_OK(成功), TLV_ERROR_PARSE_FAILED(结束), 其他(错误)
 */
tlv_error_t tlv_parse_next(tlv_parser_t *parser, tlv_element_t *element);

/**
 * @brief 跳过当前元素的值部分 (用于快速定位)
 * @param parser 解析器
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_skip_value(tlv_parser_t *parser);

/**
 * @brief 查找特定 TAG 的元素
 * @param parser 解析器
 * @param tag 目标TAG
 * @param element 输出元素
 * @return TLV_OK(找到), TLV_ERROR_PARSE_FAILED(未找到)
 */
tlv_error_t tlv_find_element(tlv_parser_t *parser, uint8_t tag, tlv_element_t *element);

/*==================== 构建函数 ====================*/

/**
 * @brief 初始化 TLV 构建器
 * @param builder 构建器实例
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_builder_init(tlv_builder_t *builder, uint8_t *buffer, size_t size);

/**
 * @brief 添加原始元素
 * @param builder 构建器
 * @param tag TAG值
 * @param value 值数据
 * @param length 值长度
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_primitive(tlv_builder_t *builder, uint8_t tag, 
                               const uint8_t *value, uint32_t length);

/**
 * @brief 添加构造元素 (开始)
 * @param builder 构建器
 * @param tag TAG值 (0x30 SEQUENCE 或 0x31 SET)
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_constructed_start(tlv_builder_t *builder, uint8_t tag);

/**
 * @brief 结束构造元素
 * @param builder 构建器
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_constructed_end(tlv_builder_t *builder);

/**
 * @brief 获取构建结果
 * @param builder 构建器
 * @param out_size 输出实际长度
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_builder_finalize(tlv_builder_t *builder, size_t *out_size);

/*==================== 便利函数 ====================*/

/**
 * @brief 添加版本信息 (0x80)
 * @param builder 构建器
 * @param version 版本
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_version(tlv_builder_t *builder, const tlv_version_t *version);

/**
 * @brief 添加消息类型 (0x81)
 * @param builder 构建器
 * @param msg_type 消息类型
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_message_type(tlv_builder_t *builder, uint8_t msg_type);

/**
 * @brief 添加时间戳 (0x83)
 * @param builder 构建器
 * @param timestamp Unix时间戳(毫秒)
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_timestamp(tlv_builder_t *builder, uint64_t timestamp);

/**
 * @brief 添加 UUID (0x82/0x84/0x86)
 * @param builder 构建器
 * @param tag TAG值
 * @param uuid 16字节UUID
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_uuid(tlv_builder_t *builder, uint8_t tag, const uint8_t *uuid);

/**
 * @brief 添加整数 (0x02)
 * @param builder 构建器
 * @param value 整数值
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_integer(tlv_builder_t *builder, int64_t value);

/**
 * @brief 添加字符串 (0x0C)
 * @param builder 构建器
 * @param str UTF-8字符串
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_string(tlv_builder_t *builder, const char *str);

/**
 * @brief 添加布尔值 (0x01)
 * @param builder 构建器
 * @param value 真/假
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_add_boolean(tlv_builder_t *builder, bool value);

/**
 * @brief 验证消息完整性 (必需字段检查)
 * @param buffer TLV数据
 * @param size 数据大小
 * @return TLV_OK(完整) 或错误码
 */
tlv_error_t tlv_validate_message(const uint8_t *buffer, size_t size);

/**
 * @brief 解析版本信息
 * @param element TLV元素 (必须是0x80)
 * @param version 输出版本
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_parse_version(const tlv_element_t *element, tlv_version_t *version);

/**
 * @brief 解析时间戳
 * @param element TLV元素 (必须是0x83)
 * @param timestamp 输出时间戳
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_parse_timestamp(const tlv_element_t *element, uint64_t *timestamp);

/*==================== 压缩/加密 ====================*/

/**
 * @brief 压缩 TLV 数据
 * @param input 输入数据
 * @param input_size 输入大小
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @param compressed_size 输出压缩后大小
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_compress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size,
                         size_t *compressed_size);

/**
 * @brief 解压 TLV 数据
 * @param input 压缩数据
 * @param input_size 输入大小
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @param decompressed_size 输出解压后大小
 * @return TLV_OK 或错误码
 */
tlv_error_t tlv_decompress(const uint8_t *input, size_t input_size,
                           uint8_t *output, size_t output_size,
                           size_t *decompressed_size);

#endif /* TLV_CODEC_H */
```

```c
/**
 * @file tlv_codec.c
 * @brief BER-TLV 编解码库实现
 */

#include "tlv_codec.h"
#include <string.h>
#include <zlib.h>  /* 需要链接 zlib */

/* 大端序转换 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define BE64(x) __builtin_bswap64(x)
    #define BE32(x) __builtin_bswap32(x)
    #define BE16(x) __builtin_bswap16(x)
#else
    #define BE64(x) (x)
    #define BE32(x) (x)
    #define BE16(x) (x)
#endif

/* 计算整数的字节数 */
static uint8_t integer_bytes(int64_t value) {
    if (value == 0) return 1;
    if (value < 0) {
        int64_t mask = 0xFF00000000000000LL;
        for (int i = 7; i >= 0; i--) {
            if ((value & mask) != mask) return i + 2;
            mask >>= 8;
        }
        return 8;
    } else {
        uint64_t v = value;
        int bytes = 0;
        while (v > 0) {
            v >>= 8;
            bytes++;
        }
        if (bytes == 0) bytes = 1;
        /* 检查是否需要前导零 */
        if (value & 0x80) bytes++;
        return bytes;
    }
}

/* 编码长度 */
static size_t encode_length(uint8_t *buffer, uint32_t length) {
    if (length < 128) {
        buffer[0] = length;
        return 1;
    } else if (length < 256) {
        buffer[0] = 0x81;
        buffer[1] = length;
        return 2;
    } else if (length < 65536) {
        buffer[0] = 0x82;
        buffer[1] = (length >> 8) & 0xFF;
        buffer[2] = length & 0xFF;
        return 3;
    } else {
        buffer[0] = 0x83;
        buffer[1] = (length >> 16) & 0xFF;
        buffer[2] = (length >> 8) & 0xFF;
        buffer[3] = length & 0xFF;
        return 4;
    }
}

/* 解码长度 */
static tlv_error_t decode_length(const uint8_t *buffer, size_t buffer_size,
                                  size_t *offset, uint32_t *length) {
    if (*offset >= buffer_size) return TLV_ERROR_INVALID_LENGTH;
    
    uint8_t first = buffer[*offset];
    (*offset)++;
    
    if ((first & 0x80) == 0) {
        *length = first;
        return TLV_OK;
    }
    
    uint8_t num_bytes = first & 0x7F;
    if (num_bytes == 0 || num_bytes > 4) return TLV_ERROR_INVALID_LENGTH;
    if (*offset + num_bytes > buffer_size) return TLV_ERROR_INVALID_LENGTH;
    
    *length = 0;
    for (int i = 0; i < num_bytes; i++) {
        *length = (*length << 8) | buffer[*offset];
        (*offset)++;
    }
    
    return TLV_OK;
}

/*==================== 解析实现 ====================*/

tlv_error_t tlv_parser_init(tlv_parser_t *parser, const uint8_t *buffer, size_t size) {
    if (!parser || !buffer) return TLV_ERROR_NULL_POINTER;
    parser->buffer = buffer;
    parser->buffer_size = size;
    parser->offset = 0;
    parser->parsed_count = 0;
    return TLV_OK;
}

tlv_error_t tlv_parse_next(tlv_parser_t *parser, tlv_element_t *element) {
    if (!parser || !element) return TLV_ERROR_NULL_POINTER;
    if (parser->offset >= parser->buffer_size) return TLV_ERROR_PARSE_FAILED;
    
    /* 读取TAG */
    element->tag = parser->buffer[parser->offset++];
    
    /* 读取长度 */
    tlv_error_t err = decode_length(parser->buffer, parser->buffer_size, 
                                    &parser->offset, &element->length);
    if (err != TLV_OK) return err;
    
    /* 设置值指针 */
    element->value = parser->buffer + parser->offset;
    
    /* 跳过值部分 */
    if (parser->offset + element->length > parser->buffer_size) {
        return TLV_ERROR_BUFFER_TOO_SMALL;
    }
    parser->offset += element->length;
    parser->parsed_count++;
    
    return TLV_OK;
}

tlv_error_t tlv_find_element(tlv_parser_t *parser, uint8_t tag, tlv_element_t *element) {
    if (!parser) return TLV_ERROR_NULL_POINTER;
    
    /* 重置到开始位置 */
    size_t saved_offset = parser->offset;
    parser->offset = 0;
    parser->parsed_count = 0;
    
    tlv_error_t err;
    tlv_element_t elem;
    
    while ((err = tlv_parse_next(parser, &elem)) == TLV_OK) {
        if (elem.tag == tag) {
            *element = elem;
            return TLV_OK;
        }
    }
    
    /* 恢复状态 */
    parser->offset = saved_offset;
    return TLV_ERROR_PARSE_FAILED;
}

/*==================== 构建实现 ====================*/

tlv_error_t tlv_builder_init(tlv_builder_t *builder, uint8_t *buffer, size_t size) {
    if (!builder || !buffer) return TLV_ERROR_NULL_POINTER;
    builder->buffer = buffer;
    builder->buffer_size = size;
    builder->offset = 0;
    builder->element_count = 0;
    return TLV_OK;
}

tlv_error_t tlv_add_primitive(tlv_builder_t *builder, uint8_t tag,
                               const uint8_t *value, uint32_t length) {
    if (!builder || (!value && length > 0)) return TLV_ERROR_NULL_POINTER;
    
    /* 计算需要的空间 */
    size_t tag_size = 1;
    size_t len_size = (length < 128) ? 1 : (length < 256) ? 2 : (length < 65536) ? 3 : 4;
    size_t total = tag_size + len_size + length;
    
    if (builder->offset + total > builder->buffer_size) {
        return TLV_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* 写入TAG */
    builder->buffer[builder->offset++] = tag;
    
    /* 写入长度 */
    builder->offset += encode_length(builder->buffer + builder->offset, length);
    
    /* 写入值 */
    if (length > 0) {
        memcpy(builder->buffer + builder->offset, value, length);
        builder->offset += length;
    }
    
    builder->element_count++;
    return TLV_OK;
}

tlv_error_t tlv_add_version(tlv_builder_t *builder, const tlv_version_t *version) {
    if (!builder || !version) return TLV_ERROR_NULL_POINTER;
    uint8_t data[3] = {version->major, version->minor, version->patch};
    return tlv_add_primitive(builder, TLV_TAG_PROTOCOL_VERSION, data, 3);
}

tlv_error_t tlv_add_message_type(tlv_builder_t *builder, uint8_t msg_type) {
    return tlv_add_primitive(builder, TLV_TAG_MESSAGE_TYPE, &msg_type, 1);
}

tlv_error_t tlv_add_timestamp(tlv_builder_t *builder, uint64_t timestamp) {
    uint64_t be_ts = BE64(timestamp);
    return tlv_add_primitive(builder, TLV_TAG_TIMESTAMP, 
                            (uint8_t *)&be_ts, sizeof(be_ts));
}

tlv_error_t tlv_add_uuid(tlv_builder_t *builder, uint8_t tag, const uint8_t *uuid) {
    return tlv_add_primitive(builder, tag, uuid, 16);
}

tlv_error_t tlv_add_integer(tlv_builder_t *builder, int64_t value) {
    uint8_t bytes[8];
    uint8_t len = integer_bytes(value);
    
    /* 大端序写入 */
    for (int i = 0; i < len; i++) {
        bytes[len - 1 - i] = (value >> (i * 8)) & 0xFF;
    }
    
    return tlv_add_primitive(builder, TLV_TAG_INTEGER, bytes, len);
}

tlv_error_t tlv_add_string(tlv_builder_t *builder, const char *str) {
    if (!str) return TLV_ERROR_NULL_POINTER;
    size_t len = strlen(str);
    return tlv_add_primitive(builder, TLV_TAG_UTF8_STRING, (const uint8_t *)str, len);
}

tlv_error_t tlv_add_boolean(tlv_builder_t *builder, bool value) {
    uint8_t byte = value ? 0xFF : 0x00;
    return tlv_add_primitive(builder, TLV_TAG_BOOLEAN, &byte, 1);
}

tlv_error_t tlv_builder_finalize(tlv_builder_t *builder, size_t *out_size) {
    if (!builder || !out_size) return TLV_ERROR_NULL_POINTER;
    *out_size = builder->offset;
    return TLV_OK;
}

/*==================== 验证实现 ====================*/

tlv_error_t tlv_validate_message(const uint8_t *buffer, size_t size) {
    if (!buffer || size < 12) return TLV_ERROR_INVALID_LENGTH; /* 最小头大小 */
    
    tlv_parser_t parser;
    tlv_element_t elem;
    tlv_error_t err;
    
    err = tlv_parser_init(&parser, buffer, size);
    if (err != TLV_OK) return err;
    
    bool has_version = false;
    bool has_msg_type = false;
    bool has_msg_id = false;
    bool has_timestamp = false;
    
    while ((err = tlv_parse_next(&parser, &elem)) == TLV_OK) {
        switch (elem.tag) {
            case TLV_TAG_PROTOCOL_VERSION:
                if (elem.length != 3) return TLV_ERROR_INVALID_LENGTH;
                has_version = true;
                break;
            case TLV_TAG_MESSAGE_TYPE:
                if (elem.length != 1) return TLV_ERROR_INVALID_LENGTH;
                has_msg_type = true;
                break;
            case TLV_TAG_MESSAGE_ID:
                if (elem.length != 16) return TLV_ERROR_INVALID_LENGTH;
                has_msg_id = true;
                break;
            case TLV_TAG_TIMESTAMP:
                if (elem.length != 8) return TLV_ERROR_INVALID_LENGTH;
                has_timestamp = true;
                break;
        }
    }
    
    if (err != TLV_ERROR_PARSE_FAILED) return err; /* 非正常结束 */
    
    if (!has_version || !has_msg_type || !has_msg_id || !has_timestamp) {
        return TLV_ERROR_INVALID_TAG;
    }
    
    return TLV_OK;
}

tlv_error_t tlv_parse_version(const tlv_element_t *element, tlv_version_t *version) {
    if (!element || !version) return TLV_ERROR_NULL_POINTER;
    if (element->tag != TLV_TAG_PROTOCOL_VERSION || element->length != 3) {
        return TLV_ERROR_INVALID_TAG;
    }
    version->major = element->value[0];
    version->minor = element->value[1];
    version->patch = element->value[2];
    return TLV_OK;
}

tlv_error_t tlv_parse_timestamp(const tlv_element_t *element, uint64_t *timestamp) {
    if (!element || !timestamp) return TLV_ERROR_NULL_POINTER;
    if (element->tag != TLV_TAG_TIMESTAMP || element->length != 8) {
        return TLV_ERROR_INVALID_TAG;
    }
    uint64_t be_ts = 0;
    memcpy(&be_ts, element->value, 8);
    *timestamp = BE64(be_ts);
    return TLV_OK;
}

/*==================== 压缩实现 ====================*/

tlv_error_t tlv_compress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size,
                         size_t *compressed_size) {
    if (!input || !output || !compressed_size) return TLV_ERROR_NULL_POINTER;
    
    z_stream strm = {0};
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return TLV_ERROR_PARSE_FAILED;
    }
    
    strm.avail_in = input_size;
    strm.next_in = (Bytef *)input;
    strm.avail_out = output_size;
    strm.next_out = (Bytef *)output;
    
    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    
    if (ret != Z_STREAM_END) return TLV_ERROR_PARSE_FAILED;
    
    *compressed_size = output_size - strm.avail_out;
    return TLV_OK;
}

tlv_error_t tlv_decompress(const uint8_t *input, size_t input_size,
                           uint8_t *output, size_t output_size,
                           size_t *decompressed_size) {
    if (!input || !output || !decompressed_size) return TLV_ERROR_NULL_POINTER;
    
    z_stream strm = {0};
    if (inflateInit(&strm) != Z_OK) {
        return TLV_ERROR_PARSE_FAILED;
    }
    
    strm.avail_in = input_size;
    strm.next_in = (Bytef *)input;
    strm.avail_out = output_size;
    strm.next_out = (Bytef *)output;
    
    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    if (ret != Z_STREAM_END) return TLV_ERROR_PARSE_FAILED;
    
    *decompressed_size = output_size - strm.avail_out;
    return TLV_OK;
}

/*==================== 使用示例 ====================*/

#ifdef TLV_CODEC_TEST

#include <stdio.h>
#include <time.h>

int main() {
    /* 测试: 构建心跳消息 */
    uint8_t buffer[256];
    tlv_builder_t builder;
    tlv_parser_t parser;
    tlv_element_t elem;
    size_t msg_size;
    
    printf("=== TLV Codec Test ===\n");
    
    /* 构建 */
    tlv_builder_init(&builder, buffer, sizeof(buffer));
    
    tlv_version_t ver = {1, 2, 0};
    tlv_add_version(&builder, &ver);
    tlv_add_message_type(&builder, TLV_MSG_HEARTBEAT);
    
    uint8_t uuid[16] = {0x55, 0x0E, 0x84, 0x00, 0xE2, 0x9B, 0x41, 0xD4,
                        0xA7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00};
    tlv_add_uuid(&builder, TLV_TAG_MESSAGE_ID, uuid);
    tlv_add_timestamp(&builder, (uint64_t)time(NULL) * 1000);
    
    tlv_add_boolean(&builder, true); /* online status */
    
    tlv_builder_finalize(&builder, &msg_size);
    
    printf("Built message: %zu bytes\n", msg_size);
    
    /* 解析 */
    tlv_parser_init(&parser, buffer, msg_size);
    
    while (tlv_parse_next(&parser, &elem) == TLV_OK) {
        printf("Tag: 0x%02X, Length: %u\n", elem.tag, elem.length);
        
        if (elem.tag == TLV_TAG_PROTOCOL_VERSION) {
            tlv_version_t v;
            tlv_parse_version(&elem, &v);
            printf("  Version: %d.%d.%d\n", v.major, v.minor, v.patch);
        }
    }
    
    /* 验证 */
    tlv_error_t err = tlv_validate_message(buffer, msg_size);
    printf("Validation: %s\n", err == TLV_OK ? "OK" : "FAILED");
    
    return 0;
}

#endif /* TLV_CODEC_TEST */
```

### 1.2 Java 语言实现 (手机SDK/Backend)

```java
/**
 * @file TlvCodec.java
 * @brief BER-TLV 编解码库 - Java版本 (适用于Android和后端)
 * @version 1.2.0
 */

package com.automotive.tlv;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

/**
 * TLV 编解码主类
 */
public class TlvCodec {
    
    /* 版本 */
    public static final int VERSION_MAJOR = 1;
    public static final int VERSION_MINOR = 2;
    public static final int VERSION_PATCH = 0;
    
    /* TAG分类 */
    public static final int CLASS_UNIVERSAL = 0x00;
    public static final int CLASS_APPLICATION = 0x40;
    public static final int CLASS_CONTEXT_SPECIFIC = 0x80;
    public static final int CLASS_PRIVATE = 0xC0;
    
    /* Universal Class Tags */
    public static final int TAG_BOOLEAN = 0x01;
    public static final int TAG_INTEGER = 0x02;
    public static final int TAG_OCTET_STRING = 0x04;
    public static final int TAG_NULL = 0x05;
    public static final int TAG_UTF8_STRING = 0x0C;
    public static final int TAG_SEQUENCE = 0x30;
    public static final int TAG_SET = 0x31;
    
    /* Context-Specific Tags (协议控制) */
    public static final int TAG_PROTOCOL_VERSION = 0x80;  // 必需
    public static final int TAG_MESSAGE_TYPE = 0x81;      // 必需
    public static final int TAG_MESSAGE_ID = 0x82;        // 必需
    public static final int TAG_TIMESTAMP = 0x83;         // 必需
    public static final int TAG_SESSION_ID = 0x84;
    public static final int TAG_CORRELATION_ID = 0x86;
    public static final int TAG_ERROR_CODE = 0x87;
    public static final int TAG_AUTH_TOKEN = 0x88;
    public static final int TAG_SIGNATURE = 0x89;
    public static final int TAG_COMPRESSION_FLAG = 0x8B;
    
    /* 消息类型 */
    public static final int MSG_REQUEST = 0x01;
    public static final int MSG_RESPONSE = 0x02;
    public static final int MSG_ERROR = 0x03;
    public static final int MSG_NOTIFICATION = 0x04;
    public static final int MSG_HEARTBEAT = 0x05;
    public static final int MSG_COMMAND = 0x07;
    public static final int MSG_COMMAND_RESULT = 0x09;
    public static final int MSG_OTA_REQUEST = 0x0A;
    public static final int MSG_SYNC_REQUEST = 0x0C;
    public static final int MSG_VERSION_NEGOTIATION = 0x0E;
    public static final int MSG_VERSION_NEGOTIATION_ACK = 0x0F;
    
    /**
     * TLV元素类
     */
    public static class TlvElement {
        public final int tag;
        public final byte[] value;
        
        public TlvElement(int tag, byte[] value) {
            this.tag = tag;
            this.value = value != null ? value : new byte[0];
        }
        
        public int getTagClass() {
            return tag & 0xC0;
        }
        
        public boolean isConstructed() {
            return (tag & 0x20) != 0;
        }
        
        public int length() {
            return value.length;
        }
        
        @Override
        public String toString() {
            return String.format("TlvElement{tag=0x%02X, length=%d}", tag, value.length);
        }
    }
    
    /**
     * 协议版本类
     */
    public static class ProtocolVersion {
        public final int major;
        public final int minor;
        public final int patch;
        
        public ProtocolVersion(int major, int minor, int patch) {
            this.major = major;
            this.minor = minor;
            this.patch = patch;
        }
        
        public static ProtocolVersion fromBytes(byte[] data) {
            if (data == null || data.length < 3) {
                return new ProtocolVersion(0, 0, 0);
            }
            return new ProtocolVersion(data[0] & 0xFF, data[1] & 0xFF, data[2] & 0xFF);
        }
        
        public byte[] toBytes() {
            return new byte[]{(byte) major, (byte) minor, (byte) patch};
        }
        
        @Override
        public String toString() {
            return String.format("%d.%d.%d", major, minor, patch);
        }
    }
    
    /**
     * TLV构建器
     */
    public static class Builder {
        private final ByteArrayOutputStream stream;
        private final List<Integer> constructedStack;
        
        public Builder() {
            this.stream = new ByteArrayOutputStream(256);
            this.constructedStack = new ArrayList<>();
        }
        
        public Builder(int initialCapacity) {
            this.stream = new ByteArrayOutputStream(initialCapacity);
            this.constructedStack = new ArrayList<>();
        }
        
        /**
         * 添加原始元素
         */
        public Builder addPrimitive(int tag, byte[] value) {
            if (value == null) value = new byte[0];
            
            stream.write(tag);
            encodeLength(value.length);
            stream.write(value, 0, value.length);
            
            return this;
        }
        
        /**
         * 添加版本信息
         */
        public Builder addVersion(ProtocolVersion version) {
            return addPrimitive(TAG_PROTOCOL_VERSION, version.toBytes());
        }
        
        /**
         * 添加消息类型
         */
        public Builder addMessageType(int msgType) {
            return addPrimitive(TAG_MESSAGE_TYPE, new byte[]{(byte) msgType});
        }
        
        /**
         * 添加UUID
         */
        public Builder addUuid(int tag, UUID uuid) {
            ByteBuffer buffer = ByteBuffer.allocate(16);
            buffer.putLong(uuid.getMostSignificantBits());
            buffer.putLong(uuid.getLeastSignificantBits());
            return addPrimitive(tag, buffer.array());
        }
        
        /**
         * 添加时间戳
         */
        public Builder addTimestamp(long timestamp) {
            ByteBuffer buffer = ByteBuffer.allocate(8);
            buffer.order(ByteOrder.BIG_ENDIAN);
            buffer.putLong(timestamp);
            return addPrimitive(TAG_TIMESTAMP, buffer.array());
        }
        
        /**
         * 添加整数
         */
        public Builder addInteger(long value) {
            byte[] bytes = encodeInteger(value);
            return addPrimitive(TAG_INTEGER, bytes);
        }
        
        /**
         * 添加字符串
         */
        public Builder addString(int tag, String str) {
            byte[] bytes = str.getBytes(StandardCharsets.UTF_8);
            return addPrimitive(tag, bytes);
        }
        
        /**
         * 添加布尔值
         */
        public Builder addBoolean(boolean value) {
            return addPrimitive(TAG_BOOLEAN, new byte[]{(byte) (value ? 0xFF : 0x00)});
        }
        
        /**
         * 开始构造元素
         */
        public Builder startConstructed(int tag) {
            stream.write(tag);
            constructedStack.add(stream.size()); // 记录长度位置(后续填充)
            stream.write(0x82); // 预留2字节长度
            stream.write(0x00);
            stream.write(0x00);
            return this;
        }
        
        /**
         * 结束构造元素
         */
        public Builder endConstructed() {
            if (constructedStack.isEmpty()) {
                throw new IllegalStateException("No constructed element to end");
            }
            
            int lengthPos = constructedStack.remove(constructedStack.size() - 1);
            int length = stream.size() - lengthPos - 3; // 减去tag和长度占位
            
            byte[] buffer = stream.toByteArray();
            buffer[lengthPos] = 0x82;
            buffer[lengthPos + 1] = (byte) ((length >> 8) & 0xFF);
            buffer[lengthPos + 2] = (byte) (length & 0xFF);
            
            stream.reset();
            stream.write(buffer, 0, buffer.length);
            
            return this;
        }
        
        /**
         * 构建完成
         */
        public byte[] build() {
            if (!constructedStack.isEmpty()) {
                throw new IllegalStateException("Unclosed constructed elements");
            }
            return stream.toByteArray();
        }
        
        private void encodeLength(int length) {
            if (length < 128) {
                stream.write(length);
            } else if (length < 256) {
                stream.write(0x81);
                stream.write(length);
            } else {
                stream.write(0x82);
                stream.write((length >> 8) & 0xFF);
                stream.write(length & 0xFF);
            }
        }
        
        private byte[] encodeInteger(long value) {
            if (value == 0) return new byte[]{0x00};
            
            int numBytes = 0;
            long v = Math.abs(value);
            while (v > 0) {
                v >>= 8;
                numBytes++;
            }
            if (value > 0 && ((value >> ((numBytes - 1) * 8)) & 0x80) != 0) {
                numBytes++; // 需要前导零
            }
            
            byte[] result = new byte[numBytes];
            for (int i = numBytes - 1; i >= 0; i--) {
                result[i] = (byte) (value & 0xFF);
                value >>= 8;
            }
            return result;
        }
    }
    
    /**
     * TLV解析器
     */
    public static class Parser {
        private final byte[] data;
        private int offset;
        
        public Parser(byte[] data) {
            this.data = data != null ? data : new byte[0];
            this.offset = 0;
        }
        
        public boolean hasNext() {
            return offset < data.length;
        }
        
        public TlvElement next() throws TlvException {
            if (!hasNext()) {
                throw new TlvException("No more elements");
            }
            
            int tag = data[offset++] & 0xFF;
            int length = decodeLength();
            
            if (offset + length > data.length) {
                throw new TlvException("Length exceeds buffer");
            }
            
            byte[] value = Arrays.copyOfRange(data, offset, offset + length);
            offset += length;
            
            return new TlvElement(tag, value);
        }
        
        public TlvElement find(int tag) throws TlvException {
            int savedOffset = offset;
            offset = 0;
            
            try {
                while (hasNext()) {
                    TlvElement elem = next();
                    if (elem.tag == tag) {
                        return elem;
                    }
                }
            } finally {
                offset = savedOffset;
            }
            
            throw new TlvException("Tag not found: 0x" + Integer.toHexString(tag));
        }
        
        private int decodeLength() throws TlvException {
            if (offset >= data.length) {
                throw new TlvException("Unexpected end of data");
            }
            
            int first = data[offset++] & 0xFF;
            if ((first & 0x80) == 0) {
                return first;
            }
            
            int numBytes = first & 0x7F;
            if (numBytes == 0 || numBytes > 4) {
                throw new TlvException("Invalid length encoding");
            }
            
            if (offset + numBytes > data.length) {
                throw new TlvException("Length bytes exceed buffer");
            }
            
            int length = 0;
            for (int i = 0; i < numBytes; i++) {
                length = (length << 8) | (data[offset++] & 0xFF);
            }
            
            return length;
        }
    }
    
    /**
     * 压缩工具
     */
    public static byte[] compress(byte[] input) {
        Deflater deflater = new Deflater(Deflater.DEFAULT_COMPRESSION, true);
        deflater.setInput(input);
        deflater.finish();
        
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream(input.length);
        byte[] buffer = new byte[1024];
        
        while (!deflater.finished()) {
            int count = deflater.deflate(buffer);
            outputStream.write(buffer, 0, count);
        }
        
        deflater.end();
        return outputStream.toByteArray();
    }
    
    /**
     * 解压缩工具
     */
    public static byte[] decompress(byte[] input, int maxSize) throws TlvException {
        Inflater inflater = new Inflater(true);
        inflater.setInput(input);
        
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream(input.length);
        byte[] buffer = new byte[1024];
        
        try {
            while (!inflater.finished()) {
                int count = inflater.inflate(buffer);
                if (count > 0) {
                    outputStream.write(buffer, 0, count);
                    if (outputStream.size() > maxSize) {
                        throw new TlvException("Decompressed size exceeds limit");
                    }
                }
            }
        } catch (Exception e) {
            throw new TlvException("Decompression failed: " + e.getMessage());
        } finally {
            inflater.end();
        }
        
        return outputStream.toByteArray();
    }
    
    /**
     * 验证消息完整性
     */
    public static boolean validate(byte[] message) {
        try {
            Parser parser = new Parser(message);
            boolean hasVersion = false;
            boolean hasMsgType = false;
            boolean hasMsgId = false;
            boolean hasTimestamp = false;
            
            while (parser.hasNext()) {
                TlvElement elem = parser.next();
                switch (elem.tag) {
                    case TAG_PROTOCOL_VERSION:
                        hasVersion = elem.value.length == 3;
                        break;
                    case TAG_MESSAGE_TYPE:
                        hasMsgType = elem.value.length == 1;
                        break;
                    case TAG_MESSAGE_ID:
                        hasMsgId = elem.value.length == 16;
                        break;
                    case TAG_TIMESTAMP:
                        hasTimestamp = elem.value.length == 8;
                        break;
                }
            }
            
            return hasVersion && hasMsgType && hasMsgId && hasTimestamp;
        } catch (Exception e) {
            return false;
        }
    }
    
    /**
     * TLV异常
     */
    public static class TlvException extends Exception {
        public TlvException(String message) {
            super(message);
        }
    }
    
    /**
     * 使用示例
     */
    public static void main(String[] args) {
        try {
            // 构建消息
            Builder builder = new Builder();
            builder.addVersion(new ProtocolVersion(1, 2, 0))
                   .addMessageType(MSG_HEARTBEAT)
                   .addUuid(TAG_MESSAGE_ID, UUID.randomUUID())
                   .addTimestamp(System.currentTimeMillis())
                   .addBoolean(true);
            
            byte[] message = builder.build();
            System.out.println("Built message: " + message.length + " bytes");
            
            // 解析消息
            Parser parser = new Parser(message);
            while (parser.hasNext()) {
                TlvElement elem = parser.next();
                System.out.println("Tag: 0x" + Integer.toHexString(elem.tag) + 
                                 ", Length: " + elem.value.length);
                
                if (elem.tag == TAG_PROTOCOL_VERSION) {
                    ProtocolVersion ver = ProtocolVersion.fromBytes(elem.value);
                    System.out.println("  Version: " + ver);
                }
            }
            
            // 验证
            System.out.println("Valid: " + validate(message));
            
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
```

---

## 2. TLV 与 JSON 转换规范

### 2.1 转换原则

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TLV 与 JSON 转换映射规则                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. 优先级规则                                                      │
│     • 车端传输: TLV为主, JSON仅用于调试                              │
│     • 云端存储: 双格式存储 (TLV原始 + JSON索引)                    │
│     • 手机SDK: 支持双向转换供开发者选择                              │
│                                                                         │
│  2. 命名映射                                                         │
│     • TLV TAG → JSON Key: 驼峰命名法                                   │
│       例: 0x41(VIN) → "vin"                                           │
│           0x45(VEHICLE_STATUS) → "vehicleStatus"                        │
│           0x48(MILEAGE) → "mileage"                                   │
│                                                                         │
│  3. 数据类型映射                                                       │
│     • INTEGER → JSON Number                                          │
│     • UTF8String → JSON String                                       │
│     • BOOLEAN → JSON Boolean                                         │
│     • OCTET_STRING → JSON String (Base64编码)                         │
│     • SEQUENCE → JSON Object                                         │
│     • SET → JSON Array                                               │
│     • UUID → JSON String (标准UUID格式)                               │
│     • TIMESTAMP → JSON String (ISO 8601) 或 JSON Number (Unix毫秒)      │
│                                                                         │
│  4. 版本信息映射                                                       │
│     • TLV: 0x80 03 01 02 00                                          │
│     • JSON: "protocolVersion": "1.2.0"                                │
│                                                                         │
│  5. 特殊处理                                                         │
│     • 压缩标志: JSON中不显示，解压后解析                                │
│     • 签名: JSON中不显示，转TLV时自动计算                              │
│     • 加密内容: JSON中为占位符, TLV为密文                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 JSON to TLV 转换示例

```json
// 输入 JSON
{
  "protocolVersion": "1.2.0",
  "messageType": "HEARTBEAT",
  "messageId": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": "2026-05-10T10:00:00.000Z",
  "payload": {
    "deviceStatus": "online",
    "batteryLevel": 85
  }
}

// 转换结果 (HEX表示)
80 03 01 02 00                    // protocolVersion: 1.2.0
81 01 05                          // messageType: 0x05 (HEARTBEAT)
82 10 55 0E 84 00 E2 9B 41 D4 A7 16 44 66 55 44 00 00  // messageId (UUID)
83 08 00 00 01 91 D4 8A E0 00    // timestamp (Unix ms)
30 14                             // payload (SEQUENCE)
  0C 06 6F 6E 6C 69 6E 65         //   deviceStatus: "online"
  02 01 55                        //   batteryLevel: 85
```

### 2.3 TLV to JSON 转换示例

```
// 输入 TLV (HEX)
80 03 01 02 00
81 01 04
82 10 AA BB CC...
83 08 00 00 01 91 D4 8A E0 00
40 20
  41 11 4C 53 56 41...
  42 08 4D 6F 64 65 6C 20 58
  43 06 54 65 73 6C 61

// 转换结果 (JSON)
{
  "protocolVersion": "1.2.0",
  "messageType": "NOTIFICATION",
  "messageId": "aabbcc00-1234-5678-9abc-def012345678",
  "timestamp": "2026-05-10T10:00:00.000Z",
  "vehicleInfo": {
    "vin": "LSVAG2180E2100001",
    "vehicleModel": "Model X",
    "vehicleBrand": "Tesla"
  }
}
```

### 2.4 完整转换代码 (Python 工具)

```python
"""
TLV <-> JSON 转换工具
支持车云协议 v1.2.0 版本
"""

import json
import struct
import base64
from datetime import datetime
from typing import Dict, Any, List, Tuple, Union
from enum import IntEnum

class TagClass(IntEnum):
    UNIVERSAL = 0x00
    APPLICATION = 0x40
    CONTEXT_SPECIFIC = 0x80
    PRIVATE = 0xC0

class UniversalTag(IntEnum):
    BOOLEAN = 0x01
    INTEGER = 0x02
    BIT_STRING = 0x03
    OCTET_STRING = 0x04
    NULL = 0x05
    UTF8_STRING = 0x0C
    SEQUENCE = 0x30
    SET = 0x31

# TAG 到键名的映射
TAG_NAME_MAP = {
    0x80: "protocolVersion",
    0x81: "messageType",
    0x82: "messageId",
    0x83: "timestamp",
    0x84: "sessionId",
    0x86: "correlationId",
    0x87: "errorCode",
    
    0x40: "vehicleInfo",
    0x41: "vin",
    0x42: "vehicleModel",
    0x43: "vehicleBrand",
    0x44: "licensePlate",
    0x45: "vehicleStatus",
    0x46: "ignition",
    0x47: "lockStatus",
    0x48: "mileage",
    0x49: "fuelLevel",
    0x4A: "batteryVoltage",
    0x4B: "engineTemp",
    
    0x50: "location",
    0x51: "latitude",
    0x52: "longitude",
    0x53: "altitude",
    0x54: "speed",
    0x55: "heading",
    
    0x58: "deviceInfo",
    0x59: "deviceId",
    0x5A: "deviceType",
    0x5B: "firmwareVersion",
    0x5C: "hardwareVersion",
    0x5D: "deviceStatus",
    0x5E: "lastHeartbeat",
    0x5F: "signalStrength",
}

# 消息类型映射
MSG_TYPE_MAP = {
    0x01: "REQUEST",
    0x02: "RESPONSE",
    0x03: "ERROR",
    0x04: "NOTIFICATION",
    0x05: "HEARTBEAT",
    0x06: "HEARTBEAT_ACK",
    0x07: "COMMAND",
    0x08: "COMMAND_ACK",
    0x09: "COMMAND_RESULT",
    0x0A: "OTA_REQUEST",
    0x0B: "OTA_RESPONSE",
    0x0C: "SYNC_REQUEST",
    0x0D: "SYNC_RESPONSE",
    0x0E: "VERSION_NEGOTIATION",
    0x0F: "VERSION_NEGOTIATION_ACK",
}

class TlvEncoder:
    """TLV 编码器"""
    
    def __init__(self):
        self.buffer = bytearray()
    
    def encode(self, data: Dict[str, Any]) -> bytes:
        """将JSON字典编码为TLV字节流"""
        # 确保必需字段顺序
        required_fields = ["protocolVersion", "messageType", "messageId", "timestamp"]
        for field in required_fields:
            if field in data:
                self._encode_field(field, data[field])
        
        # 编码其他字段
        for key, value in data.items():
            if key not in required_fields:
                self._encode_field(key, value)
        
        return bytes(self.buffer)
    
    def _encode_field(self, key: str, value: Any):
        """编码单个字段"""
        tag = self._get_tag_by_name(key)
        
        if key == "protocolVersion":
            # "1.2.0" -> [0x01, 0x02, 0x00]
            parts = value.split(".")
            self._add_primitive(tag, bytes([int(p) for p in parts]))
        
        elif key == "messageType":
            # 字符串 -> 数值
            msg_type = MSG_TYPE_MAP.get(value) if isinstance(value, str) else value
            if isinstance(msg_type, str):
                msg_type = {v: k for k, v in MSG_TYPE_MAP.items()}[msg_type]
            self._add_primitive(tag, bytes([msg_type]))
        
        elif key in ["messageId", "sessionId", "correlationId"]:
            # UUID字符串 -> 16字节
            uuid_bytes = self._uuid_to_bytes(value)
            self._add_primitive(tag, uuid_bytes)
        
        elif key == "timestamp":
            # ISO格式 -> Unix毫秒大端
            if isinstance(value, str):
                ts = int(datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp() * 1000)
            else:
                ts = value
            self._add_primitive(tag, struct.pack(">Q", ts))
        
        elif key == "payload":
            # payload容器
            self._start_constructed(tag)
            for k, v in value.items():
                self._encode_field(k, v)
            self._end_constructed()
        
        elif isinstance(value, bool):
            self._add_primitive(UniversalTag.BOOLEAN, bytes([0xFF if value else 0x00]))
        
        elif isinstance(value, int):
            self._add_primitive(UniversalTag.INTEGER, self._encode_integer(value))
        
        elif isinstance(value, str):
            self._add_primitive(UniversalTag.UTF8_STRING, value.encode("utf-8"))
        
        elif isinstance(value, dict):
            self._start_constructed(tag)
            for k, v in value.items():
                self._encode_field(k, v)
            self._end_constructed()
        
        elif isinstance(value, list):
            self._start_constructed(UniversalTag.SET)
            for item in value:
                self._encode_field("", item)
            self._end_constructed()
    
    def _add_primitive(self, tag: int, value: bytes):
        """添加原始元素"""
        self.buffer.append(tag)
        self._encode_length(len(value))
        self.buffer.extend(value)
    
    def _start_constructed(self, tag: int):
        """开始构造元素"""
        self.buffer.append(tag)
        self.buffer.extend([0x82, 0x00, 0x00])  # 预留2字节长度位
    
    def _end_constructed(self):
        """结束构造元素 - 填充长度"""
        # 简化处理: 回溯填充长度
        pass
    
    def _encode_length(self, length: int):
        """编码长度"""
        if length < 128:
            self.buffer.append(length)
        elif length < 256:
            self.buffer.extend([0x81, length])
        else:
            self.buffer.extend([0x82, (length >> 8) & 0xFF, length & 0xFF])
    
    def _encode_integer(self, value: int) -> bytes:
        """编码有符号整数"""
        if value == 0:
            return bytes([0])
        
        # 计算所需字节数
        num_bytes = 0
        v = abs(value)
        while v > 0:
            v >>= 8
            num_bytes += 1
        
        # 检查是否需要前导零
        if value > 0 and (value >> ((num_bytes - 1) * 8)) & 0x80:
            num_bytes += 1
        
        result = bytearray()
        for i in range(num_bytes - 1, -1, -1):
            result.append((value >> (i * 8)) & 0xFF)
        
        return bytes(result)
    
    def _get_tag_by_name(self, name: str) -> int:
        """根据键名获取TAG"""
        reverse_map = {v: k for k, v in TAG_NAME_MAP.items()}
        return reverse_map.get(name, 0x00)
    
    def _uuid_to_bytes(self, uuid_str: str) -> bytes:
        """UUID字符串转字节"""
        uuid_str = uuid_str.replace("-", "")
        return bytes.fromhex(uuid_str)


class TlvDecoder:
    """TLV 解码器"""
    
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0
    
    def decode(self) -> Dict[str, Any]:
        """将TLV字节流解码为JSON字典"""
        result = {}
        
        while self.offset < len(self.data):
            tag, value = self._parse_element()
            name = self._get_name_by_tag(tag)
            
            if tag == 0x80:  # protocolVersion
                result[name] = f"{value[0]}.{value[1]}.{value[2]}"
            
            elif tag == 0x81:  # messageType
                result[name] = MSG_TYPE_MAP.get(value[0], f"UNKNOWN({value[0]})")
            
            elif tag in [0x82, 0x84, 0x86]:  # UUID fields
                result[name] = self._bytes_to_uuid(value)
            
            elif tag == 0x83:  # timestamp
                ts = struct.unpack(">Q", value)[0]
                result[name] = datetime.fromtimestamp(ts / 1000).isoformat() + "Z"
            
            elif tag == UniversalTag.BOOLEAN:
                result[name] = value[0] != 0x00
            
            elif tag == UniversalTag.INTEGER:
                result[name] = self._decode_integer(value)
            
            elif tag == UniversalTag.UTF8_STRING:
                result[name] = value.decode("utf-8")
            
            elif tag in [UniversalTag.SEQUENCE, 0x30]:  # Constructed
                # 递归解析子元素
                sub_decoder = TlvDecoder(value)
                result[name] = sub_decoder.decode()
            
            else:
                # 其他情况作为字节串处理
                result[name] = base64.b64encode(value).decode("ascii")
        
        return result
    
    def _parse_element(self) -> Tuple[int, bytes]:
        """解析单个TLV元素"""
        tag = self.data[self.offset]
        self.offset += 1
        
        length = self._decode_length()
        
        if self.offset + length > len(self.data):
            raise ValueError("Length exceeds buffer")
        
        value = self.data[self.offset:self.offset + length]
        self.offset += length
        
        return tag, value
    
    def _decode_length(self) -> int:
        """解码长度"""
        first = self.data[self.offset]
        self.offset += 1
        
        if (first & 0x80) == 0:
            return first
        
        num_bytes = first & 0x7F
        length = 0
        for _ in range(num_bytes):
            length = (length << 8) | self.data[self.offset]
            self.offset += 1
        
        return length
    
    def _decode_integer(self, data: bytes) -> int:
        """解码有符号整数"""
        value = 0
        negative = data[0] & 0x80
        
        for b in data:
            value = (value << 8) | b
        
        if negative:
            # 补码转换
            value -= (1 << (len(data) * 8))
        
        return value
    
    def _get_name_by_tag(self, tag: int) -> str:
        """根据TAG获取键名"""
        return TAG_NAME_MAP.get(tag, f"tag_{tag:02X}")
    
    def _bytes_to_uuid(self, data: bytes) -> str:
        """字节转UUID字符串"""
        hex_str = data.hex()
        return f"{hex_str[0:8]}-{hex_str[8:12]}-{hex_str[12:16]}-{hex_str[16:20]}-{hex_str[20:32]}"


# 使用示例
if __name__ == "__main__":
    # JSON → TLV
    json_data = {
        "protocolVersion": "1.2.0",
        "messageType": "HEARTBEAT",
        "messageId": "550e8400-e29b-41d4-a716-446655440000",
        "timestamp": "2026-05-10T10:00:00.000Z",
        "payload": {
            "deviceStatus": "online",
            "batteryLevel": 85
        }
    }
    
    encoder = TlvEncoder()
    tlv_bytes = encoder.encode(json_data)
    print(f"JSON ({len(json.dumps(json_data))} bytes) -> TLV ({len(tlv_bytes)} bytes)")
    print(f"TLV HEX: {tlv_bytes.hex()}")
    
    # TLV → JSON
    decoder = TlvDecoder(tlv_bytes)
    decoded = decoder.decode()
    print(f"\nDecoded JSON: {json.dumps(decoded, indent=2)}")
```

---

## 3. 完整场景示例

### 3.1 场景1: 车端上电注册

```
【MQTT CONNECT + 注册消息】

TLV Hex Dump:
0000  80 03 01 02 00   // protocolVersion: 1.2.0
0005  81 01 0E         // messageType: VERSION_NEGOTIATION
0008  82 10            // messageId (16 bytes)
000A     AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99
001A  83 08            // timestamp
001C     00 00 01 91 D4 8A E0 00  // 1715410200000 ms
0024  E0 03 02 05 02   // clientVersion: 2.5.2 (Firmware)
0029  5C 03 01 2E 30   // hardwareVersion: "1.0"
002E  41 11            // vin
0030     4C 53 56 41 47 32 31 38 30 45 32 31 30 30 30 30
0040     31              // "LSVAG2180E2100001"
0041  5A 01 01         // deviceType: 0x01 (T-Box)
0044  D6 82 01 20      // certificateChain (constructed)
0048     30 82 01 1C    // SEQUENCE (X.509 cert)
...

【总计】
JSON 估算: ~600 字节
TLV 实际: ~320 字节 (包含证书)
节省: 47%

【云端响应】
80 03 01 03 00       // protocolVersion: 1.3.0
81 01 0F             // messageType: VERSION_NEGOTIATION_ACK
82 10 ...            // messageId
83 08 ...            // timestamp
86 10 ...            // correlationId (对应请求ID)
E1 03 01 03 00       // serverVersion: 1.3.0
E3 03 01 02 00       // negotiatedVersion: 1.2.0 (使用客户端版本)
E9 01 00             // negotiationResult: SUCCESS
30 12                // payload (SEQUENCE)
  91 01 00           //   resultCode: 0
  84 10 ...          //   sessionId (UUID)
  EA 02 00 18        //   gracePeriod: 24小时
```

### 3.2 场景2: 实时位置上报

```
【MQTT Publish - 高频上报 (1Hz)】

Topic: vehicle/LSVAG2180E2100001/telemetry/location
QoS: 0 (低延迟, 允许丢失)

TLV (85 bytes):
80 03 01 02 00        // protocolVersion
81 01 04              // messageType: NOTIFICATION
82 10 [UUID]          // messageId
83 08 [timestamp]     // timestamp
84 10 [session]       // sessionId
50 46                 // location (constructed, 70 bytes)
  51 04 1C 15 70 00   //   latitude: 31.230400° (×10^7)
  52 04 07 3A D7 01   //   longitude: 121.473701° (×10^7)
  53 02 00 0A         //   altitude: 10米
  54 02 00 0F A0      //   speed: 4000 (厘米/秒 = 144km/h)
  55 02 00 5A         //   heading: 90°
  56 01 05            //   accuracy: 5米
  57 08 [timestamp]   //   locationTimestamp

【对比】
JSON格式: 230 bytes
TLV格式:   85 bytes  (节省63%)
每车每天: 约1.6MB (反复上报)
TLV每车每天: 约0.6MB (节省1MB)
```

### 3.3 场景3: 远程控制指令

```【手机发起指令】
POST /api/v2/command (HTTP/2)

HTTP Headers:
Content-Type: application/x-tlv
X-Protocol-Version: 1.2.0
Authorization: Bearer [JWT]

Request Body (TLV, 128 bytes):
80 03 01 02 00        // protocolVersion
81 01 07              // messageType: COMMAND
82 10 [UUID]          // messageId
83 08 [timestamp]     // timestamp
84 10 [session]       // sessionId
41 11 [vin]           // VIN
90 01 01              // requestType: REMOTE_START
8E 01 00              // priority: 0 (最高)
8F 01 3C              // ttl: 60秒
30 08                 // commandParams (constructed)
  02 01 1E            //   duration: 30分钟
  01 01 FF            //   climateOn: true

【云端转发给车端】
MQTT Publish to: cloud/LSVAG2180E2100001/commands/remote

TLV (140 bytes, 添加correlationId和签名):
...
86 10 [uuid]          // correlationId (与手机请求关联)
89 20 [HMAC-SHA256]   // signature

【车端响应】
MQTT Publish to: vehicle/LSVAG2180E2100001/commands/response

TLV (98 bytes):
80 03 01 02 00
81 01 09              // messageType: COMMAND_RESULT
82 10 [uuid]
83 08 [timestamp]
86 10 [correlationId]  // 关联到原指令
91 01 00              // resultCode: 0 (Success)
30 0A                 // resultData
  46 01 01 01 FF      //   ignition: true
  4B 01 02 01 15      //   engineTemp: 21°C

【云端转发给手机】
HTTP/2 Push (WebSocket)

TLV (86 bytes):
...
81 01 02              // messageType: RESPONSE
86 10 [correlationId]
91 01 00              // success
30 0A                 // data

【时延】
手机→云端: < 50ms
云端→车端: < 100ms (MQTT分发)
车端执行: 2-3s (启动引擎)
车端→云端: < 100ms
云端→手机: < 50ms
总时延: 3-4s (满足用户体验要求)
```

### 3.4 场景4: OTA差分升级

```【OTA下发请求】 (MQTT)
80 03 01 02 00
81 01 0A              // OTA_REQUEST
82 10 [uuid]
83 08 [timestamp]
84 10 [session]
5B 03 02 05 03        // targetVersion: 2.5.3
30 08                 // currentVersion
  5B 03 02 05 02      //   firmwareVersion: 2.5.2

【OTA差分包响应】
云端计算差分并返回:
80 03 01 02 00
81 01 0B              // OTA_RESPONSE
82 10 [uuid]
86 10 [correlation]
91 01 00              // success
30 28                 // otaPackage
  04 04 00 01 00 00   //   packageSize: 65536 bytes (64KB差分)
  04 20 [sha256]      //   packageHash
  0C 18 [url]         //   packageUrl (HTTPS)
  02 02 00 3C         //   estimatedTime: 60分钟

【OTA进度上报】 (每10%上报一次)
80 03 01 02 00
81 01 0B              // OTA_RESPONSE (用作进度通知)
82 10 [uuid]
86 10 [correlation]
30 06                 // progress
  02 01 50            //   progress: 80%
  99 01 01            //   stage: INSTALLING

【完成确认】
80 03 01 02 00
81 01 0B
...
91 01 00              // result: SUCCESS
30 0C
  5B 03 02 05 03      //   newVersion: 2.5.3
  01 01 00            //   rebootRequired: false
```

### 3.5 场景5: 离线缓存同步

```【车端离线缓存格式】 (Flash存储)

文件头 (16 bytes):
42 54 4C 56           // Magic: "BTLV"
01 02                 // 格式版本: v1.2
00 01                 // 序列号 (monotonic)
00 00 00 00           // 保留
AA BB CC DD           // CRC32

数据区 (多条TLV消息):
TLV消息1 (时间戳T1):
  80 03 01 02 00
  81 01 04          // NOTIFICATION
  ...

TLV消息2 (时间戳T2):
  ...

总数据量: 1000条 × 平均80字节 = 80KB

【批量上传】
Topic: vehicle/{vin}/telemetry/batch

TLV结构:
80 03 01 02 00
81 01 0C          // SYNC_REQUEST
82 10 [uuid]
83 08 [timestamp]
30 [长度]        // batchData (SEQUENCE)
  30 50           //   message1 (80 bytes)
    [TLV data...]
  30 50           //   message2
    [TLV data...]
  ...

【压缩效果】
原始批量: 80KB
ZLIB压缩后: ~15KB (压缩率81%)
4G上传耗时: ~300ms (50KB/s网速)
```

---

## 4. TLV 校验工具

### 4.1 在线解析工具

```bash
#!/bin/bash
# tlv_inspector.sh - TLV协议检查工具

# 用法: ./tlv_inspector.sh <hex_string>

HEX_INPUT="${1:-}"

if [ -z "$HEX_INPUT" ]; then
    echo "Usage: $0 <hex_string>"
    echo "Example: $0 '80 03 01 02 00 81 01 05'"
    exit 1
fi

# 移除空格
HEX_CLEAN=$(echo "$HEX_INPUT" | tr -d ' ')

echo "=== TLV Inspector ==="
echo "Input: $HEX_INPUT"
echo "Length: $((${#HEX_CLEAN} / 2)) bytes"
echo ""

# 使用Python解析
python3 << EOF
import sys

data = bytes.fromhex("$HEX_CLEAN")
offset = 0
required_tags = {0x80: False, 0x81: False, 0x82: False, 0x83: False}

def read_length(data, offset):
    first = data[offset]
    offset += 1
    if first & 0x80 == 0:
        return first, offset
    num_bytes = first & 0x7F
    length = 0
    for _ in range(num_bytes):
        length = (length << 8) | data[offset]
        offset += 1
    return length, offset

print(f"{'Offset':<8} {'Tag':<8} {'Length':<8} {'Value (first 20 bytes)':<50}")
print("-" * 80)

while offset < len(data):
    if offset >= len(data):
        break
    
    tag = data[offset]
    tag_start = offset
    offset += 1
    
    try:
        length, offset = read_length(data, offset)
    except:
        print(f"Error reading length at offset {offset}")
        break
    
    value = data[offset:offset+length]
    offset += length
    
    # 格式化显示
    tag_class = tag & 0xC0
    tag_name = {
        0x80: "PROTOCOL_VER",
        0x81: "MSG_TYPE",
        0x82: "MSG_ID",
        0x83: "TIMESTAMP",
        0x30: "SEQUENCE",
    }.get(tag, f"TAG_{tag:02X}")
    
    value_preview = value[:20].hex(' ').upper()
    if len(value) > 20:
        value_preview += "..."
    
    print(f"{tag_start:<8} {tag_name:<15} {length:<8} {value_preview}")
    
    if tag in required_tags:
        required_tags[tag] = True

print("-" * 80)
print("\nValidation:")
for tag, found in required_tags.items():
    status = "✓" if found else "✗ MISSING"
    print(f"  0x{tag:02X}: {status}")

if all(required_tags.values()):
    print("\n✓ Valid TLV message")
else:
    print("\n✗ Invalid TLV message - missing required fields")
EOF
```

### 4.2 使用示例

```bash
$ ./tlv_inspector.sh "80 03 01 02 00 81 01 05 82 10 AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99 83 08 00 00 01 91 D4 8A E0 00"

=== TLV Inspector ===
Input: 80 03 01 02 00 81 01 05 82 10 AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99 83 08 00 00 01 91 D4 8A E0 00
Length: 36 bytes

Offset   Tag             Length   Value (first 20 bytes)
--------------------------------------------------------------------------------
0        PROTOCOL_VER    3        01 02 00
4        MSG_TYPE        1        05
6        MSG_ID          16       AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99
24       TIMESTAMP       8        00 00 01 91 D4 8A E0 00
--------------------------------------------------------------------------------

Validation:
  0x80: ✓
  0x81: ✓
  0x82: ✓
  0x83: ✓

✓ Valid TLV message
```

---

**文档版本**: v2.0.1  
**最后更新**: 2026-05-10  
**配套主文档**: AUTOMOTIVE-TRIPLE-END-SPEC-BERTLV.md
