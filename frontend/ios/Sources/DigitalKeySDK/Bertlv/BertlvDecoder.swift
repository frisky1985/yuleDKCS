// BertlvDecoder.swift
// BER-TLV 解码器
// 遵循 ISO/IEC 8825-1 标准解码规则

import Foundation

// MARK: - BERTLV解码错误

/// BERTLV解码错误
public enum BertlvDecodeError: Error, LocalizedError {
    /// 数据不足
    case insufficientData(expected: Int, actual: Int)
    /// 无效标签
    case invalidTag(message: String)
    /// 无效长度
    case invalidLength(message: String)
    /// 标签不匹配
    case tagMismatch(expected: BertlvTag, actual: BertlvTag)
    /// 解码失败
    case decodeFailed(message: String)

    public var errorDescription: String? {
        switch self {
        case .insufficientData(let expected, let actual):
            return "数据不足：期望 \(expected) 字节，实际 \(actual) 字节"
        case .invalidTag(let msg):
            return "无效标签: \(msg)"
        case .invalidLength(let msg):
            return "无效长度: \(msg)"
        case .tagMismatch(let expected, let actual):
            return "标签不匹配：期望 \(expected.rawBytes), 实际 \(actual.rawBytes)"
        case .decodeFailed(let msg):
            return "解码失败: \(msg)"
        }
    }
}

// MARK: - BERTLV解码器

/// BER-TLV 解码器
/// 遵循 ISO/IEC 8825-1 标准解码规则
public class BertlvDecoder {

    /// 解析上下文
    public struct DecodeContext {
        /// 当前偏移
        public var offset: Int = 0
        /// 总数据长度
        public let totalLength: Int
        /// 原始数据
        public let data: Data

        public init(data: Data) {
            self.data = data
            self.totalLength = data.count
        }

        /// 剩余可读字节数
        public var remaining: Int {
            return totalLength - offset
        }

        /// 是否还有数据
        public var hasMore: Bool {
            return offset < totalLength
        }

        /// 获取指定范围数据（不移动偏移）
        public func peek(_ count: Int) -> Data? {
            guard offset + count <= totalLength else { return nil }
            return data.subdata(in: offset..<(offset + count))
        }

        /// 读取指定字节数（移动偏移）
        public mutating func read(_ count: Int) throws -> Data {
            guard offset + count <= totalLength else {
                throw BertlvDecodeError.insufficientData(
                    expected: offset + count,
                    actual: totalLength
                )
            }
            let result = data.subdata(in: offset..<(offset + count))
            offset += count
            return result
        }

        /// 读取单个字节
        public mutating func readByte() throws -> UInt8 {
            let d = try read(1)
            return d[0]
        }
    }

    // MARK: - 主要解码方法

    /// 解码整个数据，提取所有顶级节点
    /// - Parameter data: 原始BERTLV数据
    /// - Returns: 解码后的节点数组
    public func decode(data: Data) throws -> [BertlvNode] {
        var ctx = DecodeContext(data: data)
        return try decodeAllNodes(context: &ctx)
    }

    /// 解码所有可用的顶级节点
    private func decodeAllNodes(context: inout DecodeContext) throws -> [BertlvNode] {
        var nodes: [BertlvNode] = []
        while context.hasMore {
            let node = try decodeNode(context: &context)
            nodes.append(node)
        }
        return nodes
    }

    /// 解码单个节点（标签-长度-值）
    public func decodeNode(context: inout DecodeContext) throws -> BertlvNode {
        // 1. 解码标签
        let tag = try decodeTag(context: &context)

        // 2. 解码长度
        let length = try decodeLength(context: &context)

        // 3. 读取值数据
        let valueData = try context.read(length)

        // 4. 构建节点
        return buildNode(tag: tag, valueData: valueData)
    }

    /// 解码标签
    private func decodeTag(context: inout DecodeContext) throws -> BertlvTag {
        let firstByte = try context.readByte()

        // 判断标签格式
        if firstByte & 0x1F != 0x1F {
            // 短格式: 单字节标签
            return BertlvTag(bytes: [firstByte])
        }

        // 长格式: 第一个字节为0x1F，后续1-3个字节为标签号
        var bytes: [UInt8] = [firstByte]

        // 读取后续字节（最多3个额外字节，标签号最多4字节）
        var maxExtraBytes = 3
        while maxExtraBytes > 0 {
            let b = try context.readByte()
            bytes.append(b)
            maxExtraBytes -= 1
            // 最高位为0表示标签号结束
            if b & 0x80 == 0 {
                break
            }
        }

        // 验证标签号格式是否合法
        if bytes.count < 2 {
            throw BertlvDecodeError.invalidTag(message: "标签号格式错误")
        }

        // 检查是否有后续字节
        if bytes.count > 4 {
            throw BertlvDecodeError.invalidTag(message: "标签号超过4字节限制")
        }

        return BertlvTag(bytes: bytes)
    }

    /// 解码长度字段
    private func decodeLength(context: inout DecodeContext) throws -> Int {
        let firstByte = try context.readByte()

        if firstByte & 0x80 == 0 {
            // 短格式: 0x00-0x7F，直接是长度值
            return Int(firstByte)
        }

        // 长格式: 0x80表示不确定长度，0x81-0x84表示后续字节数
        let numBytes = Int(firstByte & 0x7F)

        if numBytes == 0 {
            // 不确定长度格式（由0x00 0x00结束）
            throw BertlvDecodeError.invalidLength(message: "不确定长度格式暂不支持")
        }

        if numBytes > 4 {
            throw BertlvDecodeError.invalidLength(message: "长度字段超过4字节")
        }

        guard let lengthBytes = try? context.read(numBytes) else {
            throw BertlvDecodeError.insufficientData(expected: numBytes, actual: context.remaining)
        }

        var length = 0
        for i in 0..<numBytes {
            length = (length << 8) | Int(lengthBytes[i])
        }

        return length
    }

    /// 根据标签类型构建节点（判断是否构造类型并递归）
    private func buildNode(tag: BertlvTag, valueData: Data) -> BertlvNode {
        if tag.isConstructed {
            // 构造类型: 递归解码子节点
            var subCtx = DecodeContext(data: valueData)
            if let children = try? decodeAllNodes(context: &subCtx) {
                return BertlvNode(tag: tag, children: children)
            }
        }
        // 叶子节点
        return BertlvNode(tag: tag, value: Array(valueData))
    }

    // MARK: - 便捷解码方法

    /// 解码单个节点（从数据起始位置）
    /// - Parameter data: 原始数据
    /// - Returns: 解码后的节点
    public func decodeFirstNode(data: Data) throws -> BertlvNode {
        var ctx = DecodeContext(data: data)
        return try decodeNode(context: &ctx)
    }

    /// 解码指定标签的节点（查找第一个匹配）
    /// - Parameters:
    ///   - data: 原始数据
    ///   - tag: 要查找的标签
    /// - Returns: 匹配的节点，若不存在则抛出错误
    public func decodeNode(in data: Data, matchingTag tag: BertlvTag) throws -> BertlvNode {
        let nodes = try decode(data: data)
        guard let found = nodes.first(where: { $0.tag == tag }) else {
            throw BertlvDecodeError.tagMismatch(expected: tag, actual: BertlvTag(0))
        }
        return found
    }

    /// 解码指定标签的字节值
    /// - Parameters:
    ///   - data: 原始数据
    ///   - tag: 要查找的标签
    /// - Returns: 标签对应的值字节
    public func decodeValue(in data: Data, matchingTag tag: BertlvTag) throws -> [UInt8] {
        let node = try decodeNode(in: data, matchingTag: tag)
        guard let value = node.value else {
            throw BertlvDecodeError.decodeFailed(message: "标签 \(tag.rawBytes) 为构造类型，无值")
        }
        return value
    }

    /// 解码整数类型值（标签）
    /// - Parameters:
    ///   - data: 原始数据
    ///   - tag: 标签
    /// - Returns: 整数值
    public func decodeInteger(in data: Data, matchingTag tag: BertlvTag) throws -> Int {
        let bytes = try decodeValue(in: data, matchingTag: tag)
        return bytesToInteger(bytes)
    }

    /// 解码字符串类型值
    /// - Parameters:
    ///   - data: 原始数据
    ///   - tag: 标签
    /// - Returns: 字符串值
    public func decodeString(in data: Data, matchingTag tag: BertlvTag) throws -> String {
        let bytes = try decodeValue(in: data, matchingTag: tag)
        return String(bytes: bytes, encoding: .utf8) ?? ""
    }

    /// 字节数组转整数（大端序）
    public func bytesToInteger(_ bytes: [UInt8]) -> Int {
        guard !bytes.isEmpty else { return 0 }
        var result = 0
        for b in bytes {
            result = (result << 8) | Int(b)
        }
        // 处理负数（最高位为1）
        if bytes[0] & 0x80 != 0 {
            let bitCount = bytes.count * 8
            let mask = 1 << bitCount
            result = result - mask
        }
        return result
    }

    /// 验证数据是否符合BERTLV格式
    /// - Parameter data: 待验证数据
    /// - Returns: 是否有效
    public func validate(_ data: Data) -> Bool {
        do {
            _ = try decode(data: data)
            return true
        } catch {
            return false
        }
    }

    // MARK: - 常用标签定义

    /// 常用标签常量
    public struct Tags {
        // 通用标签 (Universal Class)
        /// 序列号
        public static let serialNumber = BertlvTag(0x02)       // INTEGER
        /// 端到端标识
        public static let endToEndId = BertlvTag(0x10)         // INTEGER
        /// OID标识符
        public static let oid = BertlvTag(0x06)                // OBJECT IDENTIFIER
        /// UTF8字符串
        public static let utf8String = BertlvTag(0x0C)        // UTF8String
        /// OCTET STRING
        public static let octetString = BertlvTag(0x04)
        /// 构造序列
        public static let sequence = BertlvTag(0x30)
        /// 构造集合
        public static let set = BertlvTag(0x31)
        /// 打印字符串
        public static let printableString = BertlvTag(0x13)
        /// UTC时间
        public static let utcTime = BertlvTag(0x17)

        // 应用标签 (Application Class)
        /// 应用标识
        public static let applicationId = BertlvTag([0x40, 0x01])  // 应用标签 + 1字节
        /// 会话标识
        public static let sessionId = BertlvTag([0x40, 0x02])
        /// 密钥数据
        public static let keyData = BertlvTag([0x40, 0x03])
        /// 车辆标识
        public static let vehicleId = BertlvTag([0x40, 0x04])
        /// 时间戳
        public static let timestamp = BertlvTag([0x40, 0x05])
        /// 签名
        public static let signature = BertlvTag([0x40, 0x06])

        // 上下文标签 (Context-specific)
        /// 响应数据
        public static let response = BertlvTag([0x80, 0x01])
        /// 请求数据
        public static let request = BertlvTag([0x80, 0x02])
        /// 状态码
        public static let statusCode = BertlvTag([0x80, 0x03])
        /// 挑战码
        public static let challenge = BertlvTag([0x80, 0x04])
        /// 响应数据
        public static let responseData = BertlvTag([0x80, 0x05])
    }
}
