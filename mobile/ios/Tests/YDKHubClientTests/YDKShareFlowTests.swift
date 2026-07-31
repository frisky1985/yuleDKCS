import XCTest
@testable import YDKHubClient

/// Phase 4.4 / W1 — CCC 分享高层编排层测试（mock MailboxClient）
///
/// 验证对象: `YDKShareFlow`（YDKShareFlow.swift）
/// 覆盖: PHASE4-4-SHARE-FLOW-CONTRACT.md B1-B4 / AC1-AC5
///   - B1.1/B1.2/B1.3: Sender 流（createMailbox 调用 + URL 形状 + 失败中止）
///   - B2.1/B2.2: Receiver 流顺序（read → keySigning → import）
///   - B2.3: 非法 URL 抛错（无 mailbox_id / 无 secret）
///   - B2.4: readSecureContent 失败 → 流程中止
///   - B3.1/B3.2: 取消流（senderCancel / receiverCancel）
///   - B4.1: MailboxDataType 枚举值与 relay.proto 一致
final class YDKShareFlowTests: XCTestCase {

    /// 记录调用顺序的 Mock MailboxClient
    private final class MockMailboxClient: YDKMailboxClientProtocol {
        enum Call: Equatable {
            case createMailbox(String)                       // senderVendor
            case readDisplayInfo(String)                     // mailboxId
            case readSecureContent(String)                   // mailboxId
            case updateMailbox(String, MailboxDataType, Data, String?)  // mailboxId, type, payload, updaterDeviceId
        }

        var calls: [Call] = []

        // 可配置返回值 / 错误
        var createResult = MailboxCreateResult(
            mailboxId: "mb-123",
            sharingUrl: "https://hub.example.com/api/v1/mailbox/mb-123#s3cret",
            expiresAt: 1_800_000_000
        )
        var createError: Error?
        var displayInfo = MailboxDisplayInfo(displayInfo: nil, version: 1)
        var displayInfoError: Error?
        var content = MailboxContent(
            payload: Data("encrypted-key-material".utf8),
            version: 1,
            errorCode: nil,
            errorMsg: nil
        )
        var contentError: Error?
        var updateResult = MailboxUpdateResult(status: "ok", version: 2, errorCode: nil, errorMsg: nil)
        var updateError: Error?
        var updateErrorForType: MailboxDataType?

        func createMailbox(
            payload: Data,
            displayInfo: Data?,
            senderVendor: String,
            senderDeviceId: String,
            expirationSeconds: Int64,
            maxUpdates: Int32,
            notificationToken: String?,
            deviceAttestation: Data?
        ) async throws -> MailboxCreateResult {
            calls.append(.createMailbox(senderVendor))
            if let createError { throw createError }
            return createResult
        }

        func readDisplayInfo(mailboxId: String) async throws -> MailboxDisplayInfo {
            calls.append(.readDisplayInfo(mailboxId))
            if let displayInfoError { throw displayInfoError }
            return displayInfo
        }

        func readSecureContent(mailboxId: String) async throws -> MailboxContent {
            calls.append(.readSecureContent(mailboxId))
            if let contentError { throw contentError }
            return content
        }

        func updateMailbox(
            mailboxId: String,
            dataType: MailboxDataType,
            payload: Data,
            notificationToken: String?,
            updaterDeviceId: String?
        ) async throws -> MailboxUpdateResult {
            calls.append(.updateMailbox(mailboxId, dataType, payload, updaterDeviceId))
            if let updateError { throw updateError }
            if let updateErrorForType, updateErrorForType == dataType {
                throw YDKError.httpError(500)
            }
            return updateResult
        }

        func deleteMailbox(
            mailboxId: String,
            reason: String,
            deleterDeviceId: String?
        ) async throws -> MailboxDeleteResult {
            MailboxDeleteResult(success: true, errorCode: nil)
        }

        func relinquishMailbox(
            mailboxId: String,
            fromDeviceId: String,
            toDeviceId: String
        ) async throws -> MailboxRelinquishResult {
            MailboxRelinquishResult(success: true, errorCode: nil, errorMsg: nil)
        }
    }

    // MARK: - B4.1 数据模型对齐

    /// MailboxDataType 枚举值必须与 relay.proto / 规范 §11.3.4 一致
    func testMailboxDataTypeRawValuesMatchSpec() {
        XCTAssertEqual(MailboxDataType.keyCreation.rawValue, 1)
        XCTAssertEqual(MailboxDataType.keySigning.rawValue, 2)
        XCTAssertEqual(MailboxDataType.import.rawValue, 3)
        XCTAssertEqual(MailboxDataType.senderCancel.rawValue, 4)
        XCTAssertEqual(MailboxDataType.receiverCancel.rawValue, 5)
    }

    // MARK: - AC2 / B1.2 URL 生成与解析

    /// buildSharingURL 格式 = `https://{host}/api/v1/mailbox/{mailbox_id}#{secret}`
    func testBuildSharingURLFormat() {
        let url = YDKShareFlow.buildSharingURL(
            host: "hub.yuletech.com:8080",
            mailboxId: "mb-abc-123",
            secret: "s3cret-token"
        )
        XCTAssertEqual(url, "https://hub.yuletech.com:8080/api/v1/mailbox/mb-abc-123#s3cret-token")
    }

    /// buildSharingURL 对误传 scheme 的 host 自动剥离
    func testBuildSharingURLStripsSchemeFromHost() {
        let url = YDKShareFlow.buildSharingURL(
            host: "https://hub.yuletech.com/",
            mailboxId: "mb-1",
            secret: "s"
        )
        XCTAssertEqual(url, "https://hub.yuletech.com/api/v1/mailbox/mb-1#s")
    }

    /// build → parse 往返一致（含 `secret=` 前缀变体）
    func testParseSharingURLRoundtrip() {
        let built = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-r1", secret: "sec-r1")
        let info = YDKMailboxClient.parseSharingURL(built)
        XCTAssertEqual(info, MailboxInfo(mailboxId: "mb-r1", secret: "sec-r1"))
        XCTAssertTrue(info?.hasSecret == true)

        // secret= 前缀变体（parse 兼容两种格式）
        let prefixed = YDKMailboxClient.parseSharingURL(
            "https://hub.example.com/api/v1/mailbox/mb-r2#secret=sec-r2"
        )
        XCTAssertEqual(prefixed, MailboxInfo(mailboxId: "mb-r2", secret: "sec-r2"))
    }

    // MARK: - B1 Sender 流

    /// B1.1: shareKeyViaMailbox 调 createMailbox 并返回分享 URL（含 fragment secret）
    func testSenderFlowCallsCreateMailboxAndReturnsServerURL() async throws {
        let mock = MockMailboxClient()
        mock.createResult = MailboxCreateResult(
            mailboxId: "mb-123",
            sharingUrl: "https://hub.example.com/api/v1/mailbox/mb-123#s3cret",
            expiresAt: 1_800_000_000
        )
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = try await flow.shareKeyViaMailbox(
            payload: Data("key".utf8),
            displayInfo: Data("info".utf8),
            senderVendor: "APPLE",
            senderDeviceId: "iphone-001",
            host: "fallback.example.com"
        )

        XCTAssertEqual(mock.calls, [.createMailbox("APPLE")], "Sender 流只应调用 createMailbox")
        XCTAssertEqual(url, "https://hub.example.com/api/v1/mailbox/mb-123#s3cret", "应原样返回服务端 sharingUrl")
    }

    /// 服务端未返回 sharingUrl 时，降级用 host + mailboxId 组装（fragment 置空，代码注释已说明）
    func testSenderFlowFallsBackToBuiltURLWhenServerMissingSharingUrl() async throws {
        let mock = MockMailboxClient()
        mock.createResult = MailboxCreateResult(mailboxId: "mb-777", sharingUrl: nil, expiresAt: nil)
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = try await flow.shareKeyViaMailbox(
            payload: Data(),
            senderVendor: "APPLE",
            senderDeviceId: "iphone-001",
            host: "hub.example.com"
        )

        XCTAssertEqual(url, "https://hub.example.com/api/v1/mailbox/mb-777#")
    }

    /// B1.3: createMailbox 失败 → 抛错，不生成 URL
    func testSenderFlowThrowsWhenCreateMailboxFails() async {
        let mock = MockMailboxClient()
        mock.createError = YDKError.hubError("MAILBOX_QUOTA", "quota exceeded")
        let flow = YDKShareFlow(mailboxClient: mock)

        do {
            _ = try await flow.shareKeyViaMailbox(
                payload: Data(),
                senderVendor: "APPLE",
                senderDeviceId: "iphone-001",
                host: "hub.example.com"
            )
            XCTFail("createMailbox 失败必须抛错")
        } catch let error as YDKError {
            guard case .hubError(let code, _) = error else {
                return XCTFail("错误类型不符: \(error)")
            }
            XCTAssertEqual(code, "MAILBOX_QUOTA")
        } catch {
            XCTFail("错误类型不符: \(error)")
        }
    }

    /// AC4: 服务端返回畸形 sharingUrl → 抛错而非静默返回垃圾
    func testSenderFlowThrowsWhenServerSharingUrlMalformed() async {
        let mock = MockMailboxClient()
        mock.createResult = MailboxCreateResult(
            mailboxId: "mb-123",
            sharingUrl: "not-a-valid-sharing-url",
            expiresAt: nil
        )
        let flow = YDKShareFlow(mailboxClient: mock)

        do {
            _ = try await flow.shareKeyViaMailbox(
                payload: Data(),
                senderVendor: "APPLE",
                senderDeviceId: "iphone-001",
                host: "hub.example.com"
            )
            XCTFail("畸形 sharingUrl 必须抛错")
        } catch let error as YDKShareFlowError {
            guard case .invalidSharingURL = error else {
                return XCTFail("错误类型不符: \(error)")
            }
        } catch {
            XCTFail("错误类型不符: \(error)")
        }
    }

    // MARK: - B2 Receiver 接受流

    /// B2.1 + B2.2: 调用顺序 = parse → readDisplayInfo → readSecureContent
    /// → update(KEY_SIGNING) → update(IMPORT)，返回 content
    func testReceiverFlowCallOrder() async throws {
        let mock = MockMailboxClient()
        mock.content = MailboxContent(payload: Data("enc".utf8), version: 3, errorCode: nil, errorMsg: nil)
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-acc", secret: "sec-acc")
        let content = try await flow.acceptSharedKeyViaMailbox(
            urlString: url,
            updaterDeviceId: "xiaomi-001",
            keySigningPayload: Data("signed".utf8),
            importPayload: Data("ack".utf8)
        )

        // 顺序断言（B2.1: read 顺序；B2.2: keySigning → import 顺序）
        XCTAssertEqual(mock.calls, [
            .readDisplayInfo("mb-acc"),
            .readSecureContent("mb-acc"),
            .updateMailbox("mb-acc", .keySigning, Data("signed".utf8), "xiaomi-001"),
            .updateMailbox("mb-acc", .import, Data("ack".utf8), "xiaomi-001"),
        ])
        XCTAssertEqual(content.payload, Data("enc".utf8), "应返回 readSecureContent 读到的内容")
        XCTAssertEqual(content.version, 3)
    }

    /// B2.3: 无 secret fragment 的 URL → 抛错
    func testReceiverFlowRejectsURLWithoutSecret() async {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        do {
            _ = try await flow.acceptSharedKeyViaMailbox(
                urlString: "https://hub.example.com/api/v1/mailbox/mb-nosecret",
                updaterDeviceId: "dev-1"
            )
            XCTFail("无 secret 的 URL 必须抛错")
        } catch let error as YDKShareFlowError {
            guard case .invalidSharingURL = error else {
                return XCTFail("错误类型不符: \(error)")
            }
        } catch {
            XCTFail("错误类型不符: \(error)")
        }
        XCTAssertTrue(mock.calls.isEmpty, "非法 URL 不应触发任何 Mailbox 调用")
    }

    /// B2.3: 无 mailbox_id / 完全非法 URL → 抛错
    func testReceiverFlowRejectsMalformedURL() async {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        let badURLs = [
            "https://hub.example.com/api/v1/mailbox/#secret",          // 空 mailbox_id
            "https://hub.example.com/other/path#secret",               // 路径不含 /mailbox/
            "not-a-url",                                               // 完全非法
        ]
        for bad in badURLs {
            do {
                _ = try await flow.acceptSharedKeyViaMailbox(urlString: bad, updaterDeviceId: "dev-1")
                XCTFail("非法 URL 必须抛错: \(bad)")
            } catch let error as YDKShareFlowError {
                guard case .invalidSharingURL = error else {
                    return XCTFail("错误类型不符: \(error)")
                }
            } catch {
                XCTFail("错误类型不符: \(error)")
            }
        }
        XCTAssertTrue(mock.calls.isEmpty)
    }

    /// B2.4: readSecureContent 失败 → 流程中止（不再调用 update）
    func testReceiverFlowAbortsWhenReadSecureContentFails() async {
        let mock = MockMailboxClient()
        mock.contentError = YDKError.httpError(503)
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-abort", secret: "s")
        do {
            _ = try await flow.acceptSharedKeyViaMailbox(urlString: url, updaterDeviceId: "dev-1")
            XCTFail("readSecureContent 失败必须中止")
        } catch let error as YDKError {
            guard case .httpError(503) = error else {
                return XCTFail("错误类型不符: \(error)")
            }
        } catch {
            XCTFail("错误类型不符: \(error)")
        }

        XCTAssertEqual(mock.calls, [
            .readDisplayInfo("mb-abort"),
            .readSecureContent("mb-abort"),
        ], "readSecureContent 失败后不得继续 updateMailbox")
    }

    /// AC4: readDisplayInfo 失败 → 同样中止
    func testReceiverFlowAbortsWhenReadDisplayInfoFails() async {
        let mock = MockMailboxClient()
        mock.displayInfoError = YDKError.networkError(URLError(.notConnectedToInternet))
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-abort2", secret: "s")
        do {
            _ = try await flow.acceptSharedKeyViaMailbox(urlString: url, updaterDeviceId: "dev-1")
            XCTFail("readDisplayInfo 失败必须中止")
        } catch {
            // 期望抛错
        }
        XCTAssertEqual(mock.calls, [.readDisplayInfo("mb-abort2")], "readDisplayInfo 失败后不得继续")
    }

    /// AC4: updateMailbox(KEY_SIGNING) 失败 → 中止，不再执行 IMPORT
    func testReceiverFlowAbortsWhenKeySigningUpdateFails() async {
        let mock = MockMailboxClient()
        mock.updateErrorForType = .keySigning
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-abort3", secret: "s")
        do {
            _ = try await flow.acceptSharedKeyViaMailbox(urlString: url, updaterDeviceId: "dev-1")
            XCTFail("KEY_SIGNING 更新失败必须中止")
        } catch {
            // 期望抛错
        }
        XCTAssertEqual(mock.calls, [
            .readDisplayInfo("mb-abort3"),
            .readSecureContent("mb-abort3"),
            .updateMailbox("mb-abort3", .keySigning, Data(), "dev-1"),
        ], "KEY_SIGNING 失败后不得执行 IMPORT")
    }

    /// AC4: content 带 errorCode（200 但业务失败）→ 抛错并中止
    func testReceiverFlowThrowsWhenContentHasErrorCode() async {
        let mock = MockMailboxClient()
        mock.content = MailboxContent(
            payload: nil,
            version: nil,
            errorCode: "CONTENT_NOT_READY",
            errorMsg: "sender has not uploaded yet"
        )
        let flow = YDKShareFlow(mailboxClient: mock)

        let url = YDKShareFlow.buildSharingURL(host: "hub.example.com", mailboxId: "mb-err", secret: "s")
        do {
            _ = try await flow.acceptSharedKeyViaMailbox(urlString: url, updaterDeviceId: "dev-1")
            XCTFail("content 带 errorCode 必须抛错")
        } catch let error as YDKShareFlowError {
            guard case .mailboxContentError(let code, _) = error else {
                return XCTFail("错误类型不符: \(error)")
            }
            XCTAssertEqual(code, "CONTENT_NOT_READY")
        } catch {
            XCTFail("错误类型不符: \(error)")
        }
        XCTAssertEqual(mock.calls, [
            .readDisplayInfo("mb-err"),
            .readSecureContent("mb-err"),
        ], "content 业务错误后不得继续 updateMailbox")
    }

    // MARK: - B3 取消流

    /// B3.1: asSender=true → updateMailbox(SENDER_CANCEL)
    func testCancelFlowSender() async throws {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        try await flow.cancelMailboxShare(mailboxId: "mb-cancel", asSender: true, updaterDeviceId: "apple-001")

        XCTAssertEqual(mock.calls, [
            .updateMailbox("mb-cancel", .senderCancel, Data(), "apple-001"),
        ])
    }

    /// B3.2: asSender=false → updateMailbox(RECEIVER_CANCEL)
    func testCancelFlowReceiver() async throws {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        try await flow.cancelMailboxShare(mailboxId: "mb-cancel", asSender: false, updaterDeviceId: "xiaomi-001")

        XCTAssertEqual(mock.calls, [
            .updateMailbox("mb-cancel", .receiverCancel, Data(), "xiaomi-001"),
        ])
    }

    /// AC1: senderCancelMailboxShare / receiverCancelMailboxShare 便捷 API 映射正确
    func testCancelConvenienceAPIs() async throws {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        try await flow.senderCancelMailboxShare(mailboxId: "mb-a", updaterDeviceId: "d1")
        try await flow.receiverCancelMailboxShare(mailboxId: "mb-b", updaterDeviceId: "d2")

        XCTAssertEqual(mock.calls, [
            .updateMailbox("mb-a", .senderCancel, Data(), "d1"),
            .updateMailbox("mb-b", .receiverCancel, Data(), "d2"),
        ])
    }

    /// 空 mailboxId → 抛错
    func testCancelFlowRejectsEmptyMailboxId() async {
        let mock = MockMailboxClient()
        let flow = YDKShareFlow(mailboxClient: mock)

        do {
            try await flow.cancelMailboxShare(mailboxId: "", asSender: true, updaterDeviceId: "d1")
            XCTFail("空 mailboxId 必须抛错")
        } catch let error as YDKShareFlowError {
            guard case .invalidMailboxId = error else {
                return XCTFail("错误类型不符: \(error)")
            }
        } catch {
            XCTFail("错误类型不符: \(error)")
        }
        XCTAssertTrue(mock.calls.isEmpty)
    }
}
