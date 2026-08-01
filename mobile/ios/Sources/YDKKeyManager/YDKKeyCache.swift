import Foundation
import YDKHubClient

/// 本地钥匙缓存 — JSON 文件持久化
///
/// 缓存文件位置: `<ApplicationSupportDirectory>/com.yuledkcs.sdk/keys_cache.json`
/// 使用 JSON 格式存储，方便调试和迁移。
final class YDKKeyCache {

    struct CacheData: Codable {
        let version: Int
        let lastSyncAt: Int64
        let keys: [YDKKey]
    }

    private let fileURL: URL
    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()
    private let logger: YDKLogger

    init(fileURL: URL? = nil, logger: YDKLogger) {
        self.logger = logger
        if let url = fileURL {
            self.fileURL = url
        } else {
            let supportDir = FileManager.default.urls(
                for: .applicationSupportDirectory, in: .userDomainMask
            ).first!
            let sdkDir = supportDir.appendingPathComponent("com.yuledkcs.sdk", isDirectory: true)
            try? FileManager.default.createDirectory(at: sdkDir, withIntermediateDirectories: true)
            self.fileURL = sdkDir.appendingPathComponent("keys_cache.json")
        }
    }

    /// 读取缓存
    func read() -> CacheData? {
        guard let data = try? Data(contentsOf: fileURL) else {
            logger.log("Cache: no existing cache at \(fileURL.lastPathComponent)")
            return nil
        }
        guard let cache = try? decoder.decode(CacheData.self, from: data) else {
            logger.log("Cache: corrupted cache, ignoring")
            return nil
        }
        logger.log("Cache: loaded \(cache.keys.count) keys, synced at \(cache.lastSyncAt)")
        return cache
    }

    /// 获取本地钥匙列表
    func getLocalKeys() -> [YDKKey] {
        read()?.keys ?? []
    }

    /// 最近一次成功同步时间戳（毫秒）; 无缓存时返回 0
    func lastSyncTimestampMillis() -> Int64 {
        read()?.lastSyncAt ?? 0
    }

    /// 写入缓存（覆盖）
    func write(keys: [YDKKey]) {
        let data = CacheData(
            version: 1,
            lastSyncAt: Int64(Date().timeIntervalSince1970 * 1000),
            keys: keys
        )
        do {
            let encoded = try encoder.encode(data)
            try encoded.write(to: fileURL, options: .atomic)
            logger.log("Cache: wrote \(keys.count) keys")
        } catch {
            logger.log("Cache: write failed — \(error.localizedDescription)")
        }
    }

    /// 清空缓存
    func clear() {
        try? FileManager.default.removeItem(at: fileURL)
        logger.log("Cache: cleared")
    }
}
