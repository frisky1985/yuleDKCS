import Foundation

struct Vehicle: Identifiable, Codable, Hashable {
    let id: String
    let vin: String
    let brand: String
    let model: String
    let year: Int
    let color: String
    let nickname: String?
    let status: VehicleStatus
    let keyInfo: VehicleKeyInfo?
    
    enum CodingKeys: String, CodingKey {
        case id
        case vin
        case brand
        case model
        case year
        case color
        case nickname
        case status
        case keyInfo = "key_info"
    }
}

struct VehicleStatus: Codable, Hashable {
    let isOnline: Bool
    let batteryLevel: Int
    let fuelLevel: Int?
    let mileage: Int
    let location: Location?
    let lastUpdated: Date
    
    enum CodingKeys: String, CodingKey {
        case isOnline = "is_online"
        case batteryLevel = "battery_level"
        case fuelLevel = "fuel_level"
        case mileage
        case location
        case lastUpdated = "last_updated"
    }
}

struct Location: Codable, Hashable {
    let latitude: Double
    let longitude: Double
    let address: String?
}

struct VehicleKeyInfo: Codable, Hashable {
    let keyId: String
    let keyType: KeyType
    let permissions: [String]
    let validFrom: Date
    let validUntil: Date?
    let isPrimary: Bool
    
    enum CodingKeys: String, CodingKey {
        case keyId = "key_id"
        case keyType = "key_type"
        case permissions
        case validFrom = "valid_from"
        case validUntil = "valid_until"
        case isPrimary = "is_primary"
    }
}

enum KeyType: String, Codable {
    case owner = "OWNER"
    case friend = "FRIEND"
    case service = "SERVICE"
    case temporary = "TEMPORARY"
}

// MARK: - Mock Data

extension Vehicle {
    static let mockVehicles: [Vehicle] = [
        Vehicle(
            id: "vehicle_001",
            vin: "LSVAU2180N2123456",
            brand: "BMW",
            model: "iX3",
            year: 2024,
            color: "Mountain White",
            nickname: "我的宝马",
            status: VehicleStatus(
                isOnline: true,
                batteryLevel: 85,
                fuelLevel: nil,
                mileage: 12580,
                location: Location(
                    latitude: 31.2304,
                    longitude: 121.4737,
                    address: "上海市浦东新区"
                ),
                lastUpdated: Date()
            ),
            keyInfo: VehicleKeyInfo(
                keyId: "key_001",
                keyType: .owner,
                permissions: ["unlock", "lock", "start", "trunk", "climate"],
                validFrom: Date(),
                validUntil: nil,
                isPrimary: true
            )
        ),
        Vehicle(
            id: "vehicle_002",
            vin: "LSVAU2180N2123457",
            brand: "Mercedes-Benz",
            model: "EQC",
            year: 2023,
            color: "Obsidian Black",
            nickname: nil,
            status: VehicleStatus(
                isOnline: false,
                batteryLevel: 45,
                fuelLevel: nil,
                mileage: 25680,
                location: nil,
                lastUpdated: Date().addingTimeInterval(-3600)
            ),
            keyInfo: VehicleKeyInfo(
                keyId: "key_002",
                keyType: .friend,
                permissions: ["unlock", "lock"],
                validFrom: Date(),
                validUntil: Date().addingTimeInterval(86400 * 30),
                isPrimary: false
            )
        )
    ]
}
