package com.yuledkcs.sdk.mailbox

import com.yuledkcs.sdk.hub.YDKError

/**
 * CCC 分享高层编排层 — CCC-TS-101 §11.4 Inter-Account Key Sharing Flow
 *
 * 把 MailboxClient 的 6 个底层 API 串成三个高层流程（与 iOS YDKShareFlow 完全镜像）:
 *  1. Sender 分享流   `shareKeyViaMailbox`        — CreateMailbox → 分享 URL（§11.4.1）
 *  2. Receiver 接受流 `acceptSharedKeyViaMailbox` — parseSharingURL → readDisplayInfo →
 *                                                   readSecureContent → updateMailbox(KEY_SIGNING)
 *                                                   → updateMailbox(IMPORT)（§11.4.2-11.4.4）
 *  3. 取消流         `cancelMailboxShare`        — SENDER_CANCEL(4) / RECEIVER_CANCEL(5)（§11.3.3）
 *
 * 职责边界:
 *  - 密钥材料（KeyCreationRequest / KeySigningRequest / ImportRequest 的 payload）由
 *    钱包层/KeyManager 生成，本编排层只做顺序编排与字节透传，不做加解密。
 *  - secret 位于分享 URL 的 fragment（`#`）中，永不发送到服务器。
 */

/**
 * 构建 Sharing URL: `https://{host}/api/v1/mailbox/{mailboxId}#{secret}`
 *
 * @param host host[:port]，如 "hub.yuletech.com" 或 "hub.yuletech.com:8080"；
 *             若误传完整 scheme（"https://..."）会自动剥离，尾部 "/" 也会去除
 * @param mailboxId 服务端返回的 mailbox_id
 * @param secret 端到端加密密钥（置于 fragment，永不发送到服务器）。不做百分号编码，
 *               若 secret 含 `#`/`/` 等保留字符需调用方自行编码，与双端 parse 约定一致
 */
fun buildSharingURL(host: String, mailboxId: String, secret: String): String {
    var bareHost = host
    val schemeIdx = bareHost.indexOf("://")
    if (schemeIdx >= 0) bareHost = bareHost.substring(schemeIdx + 3)
    while (bareHost.endsWith("/")) bareHost = bareHost.dropLast(1)
    return "https://$bareHost/api/v1/mailbox/$mailboxId#$secret"
}

/**
 * Sender 分享流（§11.4.1 Steps 1-5）: 创建 Mailbox 并返回分享 URL。
 *
 * @param payload 密钥创建请求（KeyCreationRequest）字节，钱包层加密后传入
 * @param displayInfo 展示信息（接收方确认界面用），可空
 * @param senderVendor 发送方厂商标识（如 "APPLE" / "SAMSUNG"）
 * @param senderDeviceId 发送方设备 ID
 * @param host 兜底路径用裸主机名 host[:port]（服务端返回 sharingUrl 时不使用）
 * @param expirationSeconds 过期秒数（默认 24h）
 * @param maxUpdates 最大更新次数
 * @param notificationToken / deviceAttestation 透传给 Hub（可选）
 * @return 分享 URL: `https://{host}/api/v1/mailbox/{mailboxId}#{secret}`
 *
 * 失败: createMailbox 抛错（网络/服务端错误）→ 直接上抛，不生成 URL。
 *
 * 注: 服务端 CreateMailbox 响应含完整 sharingUrl（含 secret fragment），优先直接使用并校验形状；
 *     兜底路径（仅 mailboxId）拿不到 secret，fragment 为空 —— 接收方会因缺 secret 拒绝该 URL，
 *     仅用于可追踪/调试场景，生产环境应以服务端返回的 sharingUrl 为准。
 */
suspend fun MailboxClient.shareKeyViaMailbox(
    payload: ByteArray,
    displayInfo: ByteArray? = null,
    senderVendor: String,
    senderDeviceId: String,
    host: String,
    expirationSeconds: Long = 86400,
    maxUpdates: Int = 10,
    notificationToken: String? = null,
    deviceAttestation: ByteArray? = null
): String {
    val result = createMailbox(
        payload = payload,
        displayInfo = displayInfo,
        senderVendor = senderVendor,
        senderDeviceId = senderDeviceId,
        expirationSeconds = expirationSeconds,
        maxUpdates = maxUpdates,
        notificationToken = notificationToken,
        deviceAttestation = deviceAttestation
    )

    // 服务端返回的 sharingUrl 已含完整 URL（含 secret fragment）→ 校验形状后直接使用
    // （AC4: 不把垃圾字符串当分享 URL 静默返回）
    if (!result.sharingUrl.isNullOrBlank()) {
        val info = MailboxClient.parseSharingURL(result.sharingUrl)
            ?: throw YDKError.Internal("非法分享 URL（无法解析 mailbox_id）: ${result.sharingUrl}")
        if (info.mailboxId.isBlank()) {
            throw YDKError.Internal("非法分享 URL（缺少 mailbox_id）: ${result.sharingUrl}")
        }
        return result.sharingUrl
    }

    // 兜底: 服务端未返回 sharingUrl（旧版/降级实现）时用 mailboxId 组装。
    // ⚠️ secret 由服务端生成且只存在于 sharingUrl 中，此处无法重建，fragment 只能置空
    // —— 接收方会因缺 secret 拒绝该 URL（见 acceptSharedKeyViaMailbox 校验）。
    // 若服务端单独下发 secret，可在此拼接 "#$secret" 另行约定。
    val mailboxId = result.mailboxId
        ?: throw YDKError.Internal("createMailbox 响应缺少 mailboxId 与 sharingUrl")
    return buildSharingURL(host, mailboxId, "")
}

/**
 * Receiver 接受流（§11.4.2-11.4.4）: 解析分享 URL → 读展示信息 → 读加密内容 →
 * updateMailbox(KEY_SIGNING) → updateMailbox(IMPORT) → 返回读取到的 content。
 *
 * @param urlString 分享 URL（必须含 mailbox_id + secret fragment）
 * @param updaterDeviceId 接收方设备 ID（KEY_SIGNING / IMPORT 更新的 updater 标识）
 * @param keySigningPayload KeySigningRequest 载荷（钱包层/KeyManager 生成:
 *                          Endpoint Creation 签名结果，§11.4.2 Step 6）。
 *                          默认空字节 —— SDK 编排层不制造密钥材料，调用方务必传入真实签名数据
 * @param importPayload ImportRequest 载荷（钱包层生成，§11.4.3 Step 3）。默认空字节，同上
 * @return MailboxContent — readSecureContent 读到的加密 payload 与版本
 *         （钱包层基于该内容执行 trackKey / 导入）
 *
 * 注:
 *  - 规范 §11.4.3 中 Import 由**发送方**提交；本 Sprint 契约（PHASE4-4 B2.2）把
 *    KEY_SIGNING → IMPORT 两步合并进接受流编排，payload 由钱包层提供，SDK 只保证顺序。
 *    真实全链路按规范拆分为两个设备时，发送方侧可自行调用 updateMailbox(IMPORT)。
 *  - 失败语义: URL 非法（无 mailbox_id / 无 secret）→ 立即抛 [YDKError.Internal]，
 *    不发起任何请求；readDisplayInfo / readSecureContent 失败或 content 带业务错误码
 *    → 流程中止，不执行任何 update。
 */
suspend fun MailboxClient.acceptSharedKeyViaMailbox(
    urlString: String,
    updaterDeviceId: String,
    keySigningPayload: ByteArray = ByteArray(0),
    importPayload: ByteArray = ByteArray(0)
): MailboxContent {
    // 解析分享 URL（缺 mailbox_id 或缺 secret 都视为非法 → B2.3）
    val info = MailboxClient.parseSharingURL(urlString)
        ?: throw YDKError.Internal("非法分享 URL（无法解析 mailbox_id）: $urlString")
    if (info.mailboxId.isBlank() || info.secret.isEmpty()) {
        throw YDKError.Internal("非法分享 URL（需含 mailbox_id 与 secret fragment）: $urlString")
    }

    // §11.4.2: 读取展示信息（确认界面）——失败即中止
    readDisplayInfo(info.mailboxId)

    // 读取加密内容——失败即中止（B2.4）；HTTP 200 但带业务错误码也中止
    val content = readSecureContent(info.mailboxId)
    if (!content.errorCode.isNullOrEmpty()) {
        throw YDKError.Internal(
            "mailbox content 业务错误: [${content.errorCode}] ${content.errorMsg ?: ""}"
        )
    }

    // §11.4.2 Step 6: 接收方提交 KeySigningRequest
    updateMailbox(
        mailboxId = info.mailboxId,
        dataType = MailboxDataType.KEY_SIGNING,
        payload = keySigningPayload,
        updaterDeviceId = updaterDeviceId
    )

    // §11.4.3 Step 3: ImportRequest（契约 B2.2 合并进接受流）
    updateMailbox(
        mailboxId = info.mailboxId,
        dataType = MailboxDataType.IMPORT,
        payload = importPayload,
        updaterDeviceId = updaterDeviceId
    )

    return content
}

/**
 * 取消流（§11.3.3 sharingDataType 4/5）:
 *   asSender = true  → updateMailbox(SENDER_CANCEL)   — 发送方取消
 *   asSender = false → updateMailbox(RECEIVER_CANCEL) — 接收方取消
 *
 * 取消操作无需业务 payload（规范仅要求类型标记），传空字节数组。
 */
suspend fun MailboxClient.cancelMailboxShare(
    mailboxId: String,
    asSender: Boolean,
    updaterDeviceId: String
) {
    if (mailboxId.isBlank()) {
        throw YDKError.Internal("mailboxId 非法: $mailboxId")
    }
    updateMailbox(
        mailboxId = mailboxId,
        dataType = if (asSender) MailboxDataType.SENDER_CANCEL else MailboxDataType.RECEIVER_CANCEL,
        payload = ByteArray(0),
        updaterDeviceId = updaterDeviceId
    )
}

/** 发送方取消（SENDER_CANCEL=4）— `cancelMailboxShare(mailboxId, asSender = true)` 的便捷别名 */
suspend fun MailboxClient.senderCancelMailboxShare(
    mailboxId: String,
    updaterDeviceId: String
) = cancelMailboxShare(mailboxId, asSender = true, updaterDeviceId = updaterDeviceId)

/** 接收方取消（RECEIVER_CANCEL=5）— `cancelMailboxShare(mailboxId, asSender = false)` 的便捷别名 */
suspend fun MailboxClient.receiverCancelMailboxShare(
    mailboxId: String,
    updaterDeviceId: String
) = cancelMailboxShare(mailboxId, asSender = false, updaterDeviceId = updaterDeviceId)
