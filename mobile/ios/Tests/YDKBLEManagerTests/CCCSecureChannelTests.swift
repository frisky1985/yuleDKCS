import XCTest
@testable import YDKBLEManager

// MARK: - CMAC-AES-128 (RFC4493 标准向量)

final class CCCAesCmacTests: XCTestCase {

    private let key = Data([
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    ])

    func testRFC4493Subkeys() throws {
        // RFC4493 §2.4: L=7df76b0c1ab899b33e42f047b91b546f
        //   K1 = L<<1 (MSB(L)=0) = fbeed618357133667c85e08f7236a8de
        //   K2 = (K1<<1) XOR Rb (MSB(K1)=1) = f7ddac306ae266ccf90bc11ee46d513b
        // (K2 是 K1<<1 后 XOR 0x87 的结果, 不是 K1<<1 本身)
        let (k1, k2) = try CCCAesCmac.subkeys(key: key)
        XCTAssertEqual(k1.hex, "fbeed618357133667c85e08f7236a8de")
        XCTAssertEqual(k2.hex, "f7ddac306ae266ccf90bc11ee46d513b")
    }

    func testRFC4493Example1EmptyMessage() throws {
        // RFC4493 §4 Example 1: 空消息 → T = bb1d6929e95937287fa37d129b756746
        let mac = try CCCAesCmac.cmac(key: key, message: Data())
        XCTAssertEqual(mac.hex, "bb1d6929e95937287fa37d129b756746")
    }

    func testRFC4493Example2OneBlock() throws {
        // RFC4493 §4 Example 2: 16 字节 → T = 070a16b46b4d4144f79bdd9dd04a287c
        let msg = Data([0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a])
        let mac = try CCCAesCmac.cmac(key: key, message: msg)
        XCTAssertEqual(mac.hex, "070a16b46b4d4144f79bdd9dd04a287c")
    }

    func testRFC4493Example3FortyBytes() throws {
        // RFC4493 §4 Example 3: 40 字节 → T = dfa66747de9ae63030ca32611497c827
        let msg = Data([
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
            0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11
        ])
        let mac = try CCCAesCmac.cmac(key: key, message: msg)
        XCTAssertEqual(mac.hex, "dfa66747de9ae63030ca32611497c827")
    }

    func testRFC4493Example4SixtyFourBytes() throws {
        // RFC4493 §4 Example 4: 64 字节 → T = 51f0bebf7e3b9d92fc49741779363cfe
        let msg = Data([
            0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
            0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
            0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
            0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
            0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
            0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
        ])
        let mac = try CCCAesCmac.cmac(key: key, message: msg)
        XCTAssertEqual(mac.hex, "51f0bebf7e3b9d92fc49741779363cfe")
    }

    func testMac8Truncation() throws {
        // C-MAC 8 字节: CMAC 前 8 字节
        let mac8 = try CCCAesCmac.mac8(key: key, message: Data([0x01, 0x02, 0x03]))
        XCTAssertEqual(mac8.count, 8)
        let full = try CCCAesCmac.cmac(key: key, message: Data([0x01, 0x02, 0x03]))
        XCTAssertEqual(mac8, full.prefix(8))
    }
}

// MARK: - HKDF-SHA256 (RFC5869 标准向量)

final class CCCSystemKeyDerivationTests: XCTestCase {

    func testRFC5869TestCase1() {
        // RFC5869 §A.1: IKM=0b*22, salt=00 01 02 03 04 05 06 07 08 09 0a 0b 0c,
        // info=f0 f1 f2 f3 f4 f5 f6 f7 f8 f9, L=42
        let ikm = Data([UInt8](repeating: 0x0b, count: 22))
        let salt = Data([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c])
        let info = Data([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9])
        let prk = CCCSystemKeyDerivation.extract(salt: salt, ikm: ikm)
        XCTAssertEqual(prk.hex, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5")
        let okm = CCCSystemKeyDerivation.expand(prk: prk, info: info, outputLength: 42)
        XCTAssertEqual(okm.hex, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")
    }

    func testRFC5869TestCase2LongerInputs() {
        // RFC5869 §A.2 简化: 校验 extract/expand 链不崩溃且输出长度正确
        let ikm = Data((0..<80).map { UInt8($0) })
        let salt = Data((60..<80).map { UInt8($0) })
        let info = Data((0..<80).map { UInt8(200 + $0) })
        let prk = CCCSystemKeyDerivation.extract(salt: salt, ikm: ikm)
        XCTAssertEqual(prk.count, 32)
        let okm = CCCSystemKeyDerivation.expand(prk: prk, info: info, outputLength: 82)
        XCTAssertEqual(okm.count, 82)
        // 标准向量 (RFC5869 A.2)
        XCTAssertEqual(okm.hex, "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87")
    }

    func testSystemKeysDerivation() {
        // Info="SystemKeys", L=64: Kenc/Kmac/Krmac/LTSS 各 16 字节且互不相同
        let sk = Data([UInt8](repeating: 0x42, count: 32))
        let keys = CCCSystemKeyDerivation.deriveSystemKeys(sk: sk)
        XCTAssertEqual(keys.kenc.count, 16)
        XCTAssertEqual(keys.kmac.count, 16)
        XCTAssertEqual(keys.krmac.count, 16)
        XCTAssertEqual(keys.longTermSharedSecret.count, 16)
        XCTAssertNotEqual(keys.kenc, keys.kmac)
        XCTAssertNotEqual(keys.kmac, keys.krmac)
        XCTAssertNotEqual(keys.krmac, keys.longTermSharedSecret)
        // 确定性: 相同 SK 派生相同密钥
        let keys2 = CCCSystemKeyDerivation.deriveSystemKeys(sk: sk)
        XCTAssertEqual(keys.kenc, keys2.kenc)
        XCTAssertEqual(keys.longTermSharedSecret, keys2.longTermSharedSecret)
    }
}

// MARK: - 帧格式 (CCC-TS-101 Table 19-19)

final class CCCCommandFrameTests: XCTestCase {

    func testHeaderLengthIsFour() {
        XCTAssertEqual(CCCCommandFrame.headerLength, 4)
    }

    func testSelectApduWireExample() throws {
        // 规范 §19.3 示例: SELECT APDU → 0x010B0013 00A404000DA000000809434343444B41763100
        let apdu = Data([
            0x00, 0xA4, 0x04, 0x00, 0x0D,
            0xA0, 0x00, 0x00, 0x08, 0x09, 0x43, 0x43, 0x43, 0x44, 0x4B, 0x41, 0x76, 0x31,
            0x00
        ])
        XCTAssertEqual(apdu.count, 19)
        let frame = CCCCommandFrame(
            messageType: CCCMessageType.se.rawValue,
            messageID: CCCApduMessageID.dkApduRq.rawValue,
            payload: apdu
        )
        XCTAssertEqual(frame.data.hex, "010b001300a404000da000000809434343444b41763100")
    }

    func testFrameRoundTrip() throws {
        let frame = CCCCommandFrame(messageType: 0x01, messageID: 0x0B, payload: Data([0xDE, 0xAD, 0xBE, 0xEF]))
        let parsed = try CCCCommandFrame.parse(frame.data)
        XCTAssertEqual(parsed, frame)
    }

    func testPayloadLengthMismatch() {
        // 声明长度与实际不一致 → 解析失败 (防截断/粘包)
        let bad = Data([0x01, 0x0B, 0x00, 0x10, 0x01, 0x02, 0x03])
        XCTAssertNil(CCCCommandFrame(data: bad))
        XCTAssertThrowsError(try CCCCommandFrame.parse(bad))
    }

    func testRfuBitsMasked() {
        // Message Header Bit[7:6] RFU 置 0
        let frame = CCCCommandFrame(messageType: 0x81, messageID: 0x0B, payload: Data())
        XCTAssertEqual(frame.data[0], 0x01)
    }
}

// MARK: - 广告解析 (CCC-TS-101 Table 19-2)

final class CCCAdvertisementTests: XCTestCase {

    func testServiceDataParsing() {
        // AD2: Length=0x14, Type=0x21, Data=16B UUID + 1B Intent + 2B Brand
        var ad = Data()
        ad.append(0x14)
        ad.append(0x21)
        ad.append(contentsOf: YDKAdvertisementParser.cccServiceDataIntentUUIDBytes)
        ad.append(0x01) // IntentConfiguration default
        ad.append(0x00)
        ad.append(0x2A) // Brand Identifier (BMW)
        let structures = YDKAdvertisementParser.parseADStructures(from: ad)
        let result = YDKAdvertisementParser.cccServiceData(from: structures)
        XCTAssertNotNil(result)
        XCTAssertEqual(result?.intentConfig, 0x01)
        XCTAssertEqual(result?.brandIdentifier.hex, "002a")
    }

    func testServiceDataWrongUUIDRejected() {
        var ad = Data()
        ad.append(0x14)
        ad.append(0x21)
        ad.append(contentsOf: Data(repeating: 0xAA, count: 16)) // 错误 UUID
        ad.append(0x01)
        ad.append(0x00)
        ad.append(0x2A)
        let structures = YDKAdvertisementParser.parseADStructures(from: ad)
        // 注: cccServiceData 返回带标签元组可选。直接内联进 XCTAssertNil 的
        // @autoclosure () throws -> Any? 参数会触发 Swift 6.3.3 编译器解析 bug
        // (模块内符号误报 "cannot find in scope"), 故先绑定局部变量再断言 —
        // 语义等价, 且与上方 testServiceDataParsing 写法保持一致。
        let result = YDKAdvertisementParser.cccServiceData(from: structures)
        XCTAssertNil(result)
    }
}

// MARK: - 安全通道 (SCP03 风格)

final class CCCSecureChannelTests: XCTestCase {

    func testCommandEncryptThenResponseDecrypt() throws {
        // 完整往返: 命令加密 → 响应解密/验 MAC
        let keys = CCCSystemKeyDerivation.deriveSystemKeys(sk: Data([UInt8](repeating: 0x11, count: 32)))
        let channel = CCCSecureChannel(keys: keys)
        let plaintext = Data("unlock command".utf8)
        let (ciphertext, mac) = try channel.encryptCommand(plaintext)

        XCTAssertEqual(ciphertext.count % 16, 0) // 块对齐
        XCTAssertEqual(mac.count, 8)             // C-MAC 8 字节

        // 响应 (payload 模拟 APDU 响应: SW1=0x90)
        let responsePlain = Data([0x90, 0x00])
        let responsePadded = CccPadding.pad(responsePlain)
        var iv = Data(repeating: 0, count: 16); iv[0] = 0x80; iv[15] = 0x01
        let responseCipher = try CCCAESCrypto.cbcEncrypt(key: keys.kenc, iv: iv, plaintext: responsePadded)
        var macInput = Data(repeating: 0, count: 16)
        macInput.append(responseCipher)
        let rmac = try CCCAesCmac.mac8(key: keys.krmac, message: macInput)

        let decrypted = try channel.decryptResponse(ciphertext: responseCipher, rmac: rmac)
        XCTAssertEqual(decrypted, responsePlain)
    }

    func testTamperedRMACFails() throws {
        let keys = CCCSystemKeyDerivation.deriveSystemKeys(sk: Data([UInt8](repeating: 0x11, count: 32)))
        let channel = CCCSecureChannel(keys: keys)
        _ = try channel.encryptCommand(Data("unlock".utf8))

        var iv = Data(repeating: 0, count: 16); iv[0] = 0x80; iv[15] = 0x01
        let responseCipher = try CCCAESCrypto.cbcEncrypt(
            key: keys.kenc, iv: iv,
            plaintext: CccPadding.pad(Data([0x90, 0x00]))
        )
        let badRmac = Data(repeating: 0xAA, count: 8)
        XCTAssertThrowsError(try channel.decryptResponse(ciphertext: responseCipher, rmac: badRmac))
    }

    func testMultiCommandCounterIncrements() throws {
        let keys = CCCSystemKeyDerivation.deriveSystemKeys(sk: Data([UInt8](repeating: 0x11, count: 32)))
        let channel = CCCSecureChannel(keys: keys)
        let (_, mac1) = try channel.encryptCommand(Data("first".utf8))
        let (_, mac2) = try channel.encryptCommand(Data("second".utf8))
        XCTAssertNotEqual(mac1, mac2) // MAC chaining 使第二条 MAC 不同
    }
}

// MARK: - Data hex 工具 (测试用)

extension Data {
    var hex: String { map { String(format: "%02x", $0) }.joined() }
}
