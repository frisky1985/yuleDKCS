import Foundation
import CoreBluetooth
import CoreNFC
import NearbyInteraction

#if canImport(UIKit)
import UIKit
#endif

/// DeviceCapabilities detects and reports what hardware/software capabilities
/// the current device supports for digital key provisioning.
public struct DeviceCapabilities {

    public let platform: String
    public let model: String
    public let osVersion: String
    public let appVersion: String
    public let ble: Bool
    public let uwb: Bool
    public let nfc: Bool
    public let secureElement: Bool

    /// Detects all capabilities of the current device.
    /// Call this on app startup to register the device with the cloud.
    public static func detect(appVersion: String = "") -> DeviceCapabilities {
        return DeviceCapabilities(
            platform: "ios",
            model: deviceModel(),
            osVersion: UIDevice.current.systemVersion,
            appVersion: appVersion,
            ble: supportsBLE(),
            uwb: supportsUWB(),
            nfc: supportsNFC(),
            secureElement: supportsSE()
        )
    }

    /// Convert to JSON dictionary for API request
    public func toJSON() -> [String: Any] {
        return [
            "platform": platform,
            "model": model,
            "os_version": osVersion,
            "app_version": appVersion,
            "ble": ble,
            "uwb": uwb,
            "nfc": nfc,
            "secure_element": secureElement,
        ]
    }

    // MARK: - Private Detectors

    /// BLE: All iPhones from iPhone 4s onwards support BLE 4.0+
    private static func supportsBLE() -> Bool {
        return CBCentralManager.state != .unsupported
    }

    /// UWB: iPhone 11+ (U1 chip) required for NearbyInteraction
    private static func supportsUWB() -> Bool {
        if #available(iOS 14.0, *) {
            return NISession.isSupported
        }
        return false
    }

    /// NFC: iPhone 7+ supports CoreNFC
    private static func supportsNFC() -> Bool {
        return NFCNDEFReaderSession.readingAvailable
    }

    /// SE: All iPhones with Secure Enclave (iPhone 5s+)
    private static func supportsSE() -> Bool {
        // All iPhones with Touch ID / Face ID have Secure Enclave
        // This is always true for modern iPhones
        return true
    }

    /// Device model string (e.g., "iPhone 15 Pro")
    private static func deviceModel() -> String {
        var systemInfo = utsname()
        uname(&systemInfo)
        let mirror = Mirror(reflecting: systemInfo.machine)
        let identifier = mirror.children.compactMap { child -> String? in
            guard let value = child.value as? Int8, value != 0 else { return nil }
            return String(UnicodeScalar(UInt8(value)))
        }.joined()

        // Map identifier to model name
        return mapToModelName(identifier) ?? identifier
    }

    /// Maps internal identifier to human-readable model name
    private static func mapToModelName(_ identifier: String) -> String? {
        let models: [String: String] = [
            "iPhone15,2": "iPhone 14 Pro",
            "iPhone15,3": "iPhone 14 Pro Max",
            "iPhone15,4": "iPhone 15",
            "iPhone15,5": "iPhone 15 Plus",
            "iPhone16,1": "iPhone 15 Pro",
            "iPhone16,2": "iPhone 15 Pro Max",
            "iPhone17,1": "iPhone 16 Pro",
            "iPhone17,2": "iPhone 16 Pro Max",
        ]
        return models[identifier]
    }
}
