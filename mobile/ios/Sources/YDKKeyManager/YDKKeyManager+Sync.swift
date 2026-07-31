import Foundation

public extension YDKKeyManager {

    /// 自动定时同步
    ///
    /// 启动一个后台定时器，定期调 syncFromHub()。
    /// 适合 App 启动后调用一次，后续自动同步。
    ///
    /// 用法:
    /// ```swift
    /// keyManager.startAutoSync(interval: 300)  // 每 5 分钟同步一次
    /// ```
    func startAutoSync(interval: TimeInterval = 300) {
        stopAutoSync()
        let timer = DispatchSource.makeTimerSource(queue: syncQueue)
        timer.schedule(deadline: .now() + interval, repeating: interval, leeway: .seconds(30))
        timer.setEventHandler { [weak self] in
            guard let self = self else { return }
            Task {
                try? await self.syncFromHub()
            }
        }
        timer.resume()
        autoSyncTimer = timer
        logger.log("AutoSync: started every \(Int(interval))s")
    }

    /// 停止自动同步
    func stopAutoSync() {
        autoSyncTimer?.cancel()
        autoSyncTimer = nil
        logger.log("AutoSync: stopped")
    }
}

private var autoSyncTimerKey: UInt8 = 0

extension YDKKeyManager {
    fileprivate var autoSyncTimer: DispatchSourceTimer? {
        get { objc_getAssociatedObject(self, &autoSyncTimerKey) as? DispatchSourceTimer }
        set { objc_setAssociatedObject(self, &autoSyncTimerKey, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC) }
    }
}
