import Foundation

struct KeyShareRequest: Identifiable, Codable {
    let id: String
    let requesterId: String
    let requesterName: String
    let vehicleId: String
    let vehicleInfo: String
    let keyType: KeyType
    let permissions: [String]
    let message: String?
    let status: ShareRequestStatus
    let createdAt: Date
    let expiresAt: Date
    
    enum CodingKeys: String, CodingKey {
        case id
        case requesterId = "requester_id"
        case requesterName = "requester_name"
        case vehicleId = "vehicle_id"
        case vehicleInfo = "vehicle_info"
        case keyType = "key_type"
        case permissions
        case message
        case status
        case createdAt = "created_at"
        case expiresAt = "expires_at"
    }
}

enum ShareRequestStatus: String, Codable {
    case pending = "PENDING"
    case approved = "APPROVED"
    case rejected = "REJECTED"
    case expired = "EXPIRED"
    case cancelled = "CANCELLED"
}

struct KeyShareRequestCreate: Codable {
    let vehicleId: String
    let recipientPhone: String
    let recipientName: String?
    let keyType: KeyType
    let permissions: [String]
    let validDays: Int?
    let message: String?
    
    enum CodingKeys: String, CodingKey {
        case vehicleId = "vehicle_id"
        case recipientPhone = "recipient_phone"
        case recipientName = "recipient_name"
        case keyType = "key_type"
        case permissions
        case validDays = "valid_days"
        case message
    }
}

// MARK: - Mock Data

extension KeyShareRequest {
    static let mockRequests: [KeyShareRequest] = [
        KeyShareRequest(
            id: "request_001",
            requesterId: "user_002",
            requesterName: "张三",
            vehicleId: "vehicle_001",
            vehicleInfo: "BMW iX3 2024",
            keyType: .friend,
            permissions: ["unlock", "lock", "start"],
            message: "周末需要用车，请批准",
            status: .pending,
            createdAt: Date().addingTimeInterval(-3600),
            expiresAt: Date().addingTimeInterval(86400)
        ),
        KeyShareRequest(
            id: "request_002",
            requesterId: "user_003",
            requesterName: "李四",
            vehicleId: "vehicle_001",
            vehicleInfo: "BMW iX3 2024",
            keyType: .temporary,
            permissions: ["unlock", "lock"],
            message: nil,
            status: .pending,
            createdAt: Date().addingTimeInterval(-7200),
            expiresAt: Date().addingTimeInterval(86400 * 2)
        )
    ]
}
