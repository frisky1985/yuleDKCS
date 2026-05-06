// BertlvEncoder.swift
// BER-TLV 编码器
// 遵循 ISO/IEC 8825-1 标准编码规则

import Foundation

// MARK: - BER-TLV 标签

/// BER-TLV 标签表示
public struct BertlvTag: Equatable, Hashable {
    /// 标签原始字节
    public let rawBytes: [UInt8]

    /// 标签类别
    public var tagClass: UInt8 {
        rawBytes[0] & 0xC0  // 最高2位: 00=Universal, 01=Application, 10=Context, 11=Private
    }

    /// 是否为构造类型（包含子TLV）
    public var isConstructed: Bool {
        (rawBytes[0] & 0x20) != 0
    }

    /// 标签编号
    public var tagNumber: UInt32 {
        if rawBytes[0] & 0x1F != 0x1F {
            // 短格式: 标签号在第一个字节的低5位
            return UInt32(rawBytes[0] & 0x1F)
        }
        // 长格式: 后续字节编码标签号
        var number: UInt32 = 0
        for i in 1..<rawBytes.count {
            number = (number << 7) | UInt32(rawBytes[i] & 0x7F)
            if rawBytes[i] & 0x80 == 0 { break }
        }
        return number
    }

    /// 从原始字节创建标签
    public init(bytes: [UInt8]) {
        self.rawBytes = bytes
    }

    /// 从UInt16创建标签（常用场景）
    public init(_ value: UInt16) {
        if value <= 0x1E {
            // 短格式
            self.rawBytes = [UInt8(value)]
        } else if value <= 0x7E {
            // 两字节: 0x1F + 标签号
            self.rawBytes = [0x1F, UInt8(value)]
        } else if value <= 0x3FFF {
            // 三字节
            self.rawBytes = [0x1F, UInt8((value >> 7) | 0x80), UInt8(value & 0x7F)]
        } else {
            // 四字节
            self.rawBytes = [
                0x1F,
                UInt8((value >> 14) | 0x80),
                UInt8(((value >> 7) & 0x7F) | 0x80),
                UInt8(value & 0x7F)
            ]
        }
    }
}

// MARK: - BER-TLV 节点

/// BER-TLV 数据节点
public class BertlvNode: CustomDebugStringConvertible {
    /// 标签
    public let tag: BertlvTag
    /// 值（叶子节点时非nil）
    public private(set) var value: [UInt8]?
    /// 子节点（构造节点时非空）
    public private(set) var children: [BertlvNode]

    /// 是否为构造节点
    public var isConstructed: Bool {
        return tag.isConstructed || children.count > 0
    }

    /// 创建叶子节点
    public init(tag: BertlvTag, value: [UInt8]) {
        self.tag = tag
        self.value = value
        self.children = []
    }

    /// 创建构造节点
    public init(tag: BertlvTag, children: [BertlvNode]) {
        self.tag = tag
        self.value = nil
        self.children = children
    }

    /// 编码为字节序列
    public func encode() -> [UInt8] {
        var result: [UInt8] = []

        // 1. 编码标签
        result.append(contentsOf: tag.rawBytes)

        // 2. 获取值字节
        let valueBytes: [UInt8]
        if let v = value {
            valueBytes = v
        } else {
            // 构造节点: 递归编码子节点
            valueBytes = children.flatMap { $0.encode() }
        }

        // 3. 编码长度
        result.append(contentsOf: BertlvEncoder.encodeLength(valueBytes.count))

        // 4. 编码值
        result.append(contentsOf: valueBytes)

        return result
    }

    /// 查找指定标签的第一个子节点
    public func find(tag: BertlvTag) -> BertlvNode? {
        return children.first { $0.tag == tag }
    }

    /// 查找指定标签的所有子节点
    public func findAll(tag: BertlvTag) -> [BertlvNode] {
        return children.filter { $0.tag == tag }
    }

    /// 递归查找指定路径的节点
    public func find(path: [BertlvTag]) -> BertlvNode? {
        guard !path.isEmpty else { return self }
        guard let first = children.first(where: { $0.tag == path[0] }) else { return nil }
        if path.count == 1 { return first }
        return first.find(path: Array(path[1...]))
    }

    public var debugDescription: String {
        if isConstructed {
            let childDesc = children.map { $0.debugDescription }.joined(separator: ", ")
            return "T\(tag.rawBytes.map { String(format: "%02X", $0) }.joined()){\(childDesc)}"
        } else {
            let val = value?.map { String(format: "%02X", $0) }.joined() ?? ""
            return "T\(tag.rawBytes.map { String(format: "%02X", $0) }.joined())=\(val)"
        }
    }
}

// MARK: - BER-TLV 编码器

/// BER-TLV 编码器
/// 遵循 ISO/IEC 8825-1 标准编码规则
public class BertlvEncoder {

    /// 根节点
    private var rootNodes: [BertlvNode] = []

    public init() {}

    // MARK: - 构建式API

    /// 添加叶子节点
    @discardableResult
    public func add(tag: BertlvTag, value: [UInt8]) -> Self {
        rootNodes.append(BertlvNode(tag: tag, value: value))
        return self
    }

    /// 添加叶子节点（字符串值）
    @discardableResult
    public func add(tag: BertlvTag, stringValue: String) -> Self {
        let bytes = Array(stringValue.utf8)
        rootNodes.append(BertlvNode(tag: tag, value: bytes))
        return self
    }

    /// 添加叶子节点（整数值，大端序）
    @discardableResult
    public func add(tag: BertlvTag, intValue: Int) -> Self {
        var bytes: [UInt8] = []
        var value = intValue
        if value == 0 {
            bytes = [0x00]
        } else {
            // 大端序编码，去掉前导零
            var temp: [UInt8] = []
            var v = value > 0 ? value : -value
            while v > 0 {
                temp.append(UInt8(v & 0xFF))
                v >>= 8
            }
            bytes = temp.reversed()
            if value < 0 {
                // 补码处理
                bytes = BertlvEncoder.twosComplement(bytes)
            }
        }
        rootNodes.append(BertlvNode(tag: tag, value: bytes))
        return self
    }

    /// 添加构造节点
    @discardableResult
    public func add(tag: BertlvTag, @_DocResultBuilder builder: () -> [BertlvNode]) -> Self {
        rootNodes.append(BertlvNode(tag: tag, children: builder()))
        return self
    }

    /// 编码所有节点
    public func encode() -> Data {
        var result: [UInt8] = []
        for node in rootNodes {
            result.append(contentsOf: node.encode())
        }
        return Data(result)
    }

    // MARK: - 静态编码方法

    /// 编码长度字段
    /// - Parameter length: 数据长度
    /// - Returns: BER编码的长度字节
    public static func encodeLength(_ length: Int) -> [UInt8] {
        if length < 0x80 {
            // 短格式: 1字节
            return [UInt8(length)]
        } else if length <= 0xFF {
            // 长格式: 0x81 + 1字节
            return [0x81, UInt8(length)]
        } else if length <= 0xFFFF {
            // 长格式: 0x82 + 2字节
            return [0x82, UInt8(length >> 8), UInt8(length & 0xFF)]
        } else if length <= 0xFFFFFF {
            // 长格式: 0x83 + 3字节
            return [0x83, UInt8(length >> 16), UInt8((length >> 8) & 0xFF), UInt8(length & 0xFF)]
        } else {
            // 长格式: 0x84 + 4字节
            return [
                0x84,
                UInt8(length >> 24),
                UInt8((length >> 16) & 0xFF),
                UInt8((length >> 8) & 0xFF),
                UInt8(length & 0xFF)
            ]
        }
    }

    /// 构建单个TLV
    /// - Parameters:
    ///   - tag: 标签
    ///   - value: 值
    /// - Returns: TLV编码数据
    public static func encode(tag: BertlvTag, value: [UInt8]) -> Data {
        let node = BertlvNode(tag: tag, value: value)
        return Data(node.encode())
    }

    /// 构建构造TLV
    /// - Parameters:
    ///   - tag: 标签
    ///   - children: 子节点
    /// - Returns: TLV编码数据
    public static func encodeConstructed(tag: BertlvTag, children: [BertlvNode]) -> Data {
        let node = BertlvNode(tag: tag, children: children)
        return Data(node.encode())
    }

    // MARK: - 辅助方法

    /// 计算补码（用于负数编码）
    private static func twosComplement(_ bytes: [UInt8]) -> [UInt8] {
        var result = bytes
        var carry = true
        for i in stride(from: result.count - 1, through: 0, by: -1) {
            result[i] = ~result[i]
            if carry {
                if result[i] == 0xFF {
                    result[i] = 0x00
                    carry = true
                } else {
                    result[i] = result[i] + 1
                    carry = false
                }
            }
        }
        if carry {
            result.insert(0xFF, at: 0)
        }
        return result
    }
}

// MARK: - 结果构建器（用于嵌套构造节点）

@resultBuilder
public struct _DocResultBuilder {
    public static func buildBlock(_ components: BertlvNode...) -> [BertlvNode] {
        components
    }
    public static func buildOptional(_ component: [BertlvNode]?) -> [BertlvNode] {
        component ?? []
    }
    public static func buildEither(first component: [BertlvNode]) -> [BertlvNode] {
        component
    }
    public static func buildEither(second component: [BertlvNode]) -> [BertlvNode] {
        component
    }
    public static func buildArray(_ components: [[BertlvNode]]) -> [BertlvNode] {
        components.flatMap { $0 }
    }
}
