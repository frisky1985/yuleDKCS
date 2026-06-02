import XCTest
@testable import DigitalKeySDK

/// 基础单元测试
///
/// 测试 Data Model 的初始化、编码、解码行为
final class DigitalKeyAppTests: XCTestCase {

    override func setUpWithError() throws {
        // Put setup code here. This method is called before the invocation of each test method in the class.
    }

    override func tearDownWithError() throws {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
    }

    // MARK: - Vehicle Tests

    func testVehicleInitialization() throws {
        let vehicle = Vehicle(
            id: "test_001",
            vin: "TEST123456789",
            brand: "TestBrand",
            model: "TestModel",
            year: 2024,
            color: "White",
            nickname: "Test Car",
            status: VehicleStatus(
                isOnline: true,
                batteryLevel: 80,
                fuelLevel: nil,
                mileage: 10000,
                location: nil,
                lastUpdated: Date()
            ),
            keyInfo: VehicleKeyInfo(
                keyId: "key_001",
                keyType: .owner,
                permissions: ["unlock", "lock", "start"],
                validFrom: Date(),
                validUntil: nil,
                isPrimary: true
            )
        )

        XCTAssertEqual(vehicle.id, "test_001")
        XCTAssertEqual(vehicle.brand, "TestBrand")
        XCTAssertEqual(vehicle.vin, "TEST123456789")
        XCTAssertEqual(vehicle.model, "TestModel")
        XCTAssertEqual(vehicle.year, 2024)
        XCTAssertEqual(vehicle.color, "White")
        XCTAssertEqual(vehicle.nickname, "Test Car")
        XCTAssertEqual(vehicle.keyInfo?.keyType, .owner)
        XCTAssertEqual(vehicle.keyInfo?.permissions, ["unlock", "lock", "start"])
        XCTAssertTrue(vehicle.keyInfo?.isPrimary ?? false)
        XCTAssertNil(vehicle.keyInfo?.validUntil)
    }

    func testVehicleWithMinimalFields() throws {
        let vehicle = Vehicle(
            id: "test_002",
            vin: "VIN002",
            brand: "Brand",
            model: "Model",
            year: 2024,
            color: "Black",
            nickname: nil,
            status: VehicleStatus(
                isOnline: false,
                batteryLevel: 50,
                fuelLevel: 30,
                mileage: 5000,
                location: Location(latitude: 31.23, longitude: 121.47, address: nil),
                lastUpdated: Date()
            ),
            keyInfo: nil
        )

        XCTAssertEqual(vehicle.id, "test_002")
        XCTAssertNil(vehicle.nickname)
        XCTAssertNil(vehicle.keyInfo)
        XCTAssertNotNil(vehicle.status.location)
    }

    func testVehicleEncodingDecoding() throws {
        // Given
        let original = Vehicle(
            id: "test_enc_001",
            vin: "ENC_VIN_001",
            brand: "ENC Brand",
            model: "ENC Model",
            year: 2024,
            color: "Red",
            nickname: "Encoded Car",
            status: VehicleStatus(
                isOnline: true,
                batteryLevel: 75,
                fuelLevel: 50,
                mileage: 20000,
                location: Location(latitude: 31.2304, longitude: 121.4737, address: "上海"),
                lastUpdated: Date(timeIntervalSince1970: 1717200000)
            ),
            keyInfo: VehicleKeyInfo(
                keyId: "key_enc_001",
                keyType: .friend,
                permissions: ["unlock", "lock"],
                validFrom: Date(timeIntervalSince1970: 1717200000),
                validUntil: Date(timeIntervalSince1970: 1719792000),
                isPrimary: false
            )
        )

        // When: 编码再解码
        let encoder = JSONEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        let data = try encoder.encode(original)

        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        let decoded = try decoder.decode(Vehicle.self, from: data)

        // Then
        XCTAssertEqual(decoded.id, original.id)
        XCTAssertEqual(decoded.vin, original.vin)
        XCTAssertEqual(decoded.brand, original.brand)
        XCTAssertEqual(decoded.model, original.model)
        XCTAssertEqual(decoded.year, original.year)
        XCTAssertEqual(decoded.color, original.color)
        XCTAssertEqual(decoded.nickname, original.nickname)
        XCTAssertEqual(decoded.status.isOnline, original.status.isOnline)
        XCTAssertEqual(decoded.status.batteryLevel, original.status.batteryLevel)
        XCTAssertEqual(decoded.status.fuelLevel, original.status.fuelLevel)
        XCTAssertEqual(decoded.status.mileage, original.status.mileage)
        XCTAssertEqual(decoded.status.location?.latitude, original.status.location?.latitude)
        XCTAssertEqual(decoded.keyInfo?.keyType, original.keyInfo?.keyType)
        XCTAssertEqual(decoded.keyInfo?.isPrimary, original.keyInfo?.isPrimary)
    }

    // MARK: - VehicleStatus Tests

    func testVehicleStatusWithAllFields() throws {
        let status = VehicleStatus(
            isOnline: true,
            batteryLevel: 90,
            fuelLevel: 45,
            mileage: 15000,
            location: Location(latitude: 31.2304, longitude: 121.4737, address: "上海浦东"),
            lastUpdated: Date()
        )

        XCTAssertTrue(status.isOnline)
        XCTAssertEqual(status.batteryLevel, 90)
        XCTAssertEqual(status.fuelLevel, 45)
        XCTAssertEqual(status.mileage, 15000)
        XCTAssertNotNil(status.location)
    }

    func testVehicleStatusWithNilFuelAndLocation() throws {
        let status = VehicleStatus(
            isOnline: false,
            batteryLevel: 0,
            fuelLevel: nil,
            mileage: 0,
            location: nil,
            lastUpdated: Date()
        )

        XCTAssertFalse(status.isOnline)
        XCTAssertNil(status.fuelLevel)
        XCTAssertNil(status.location)
    }

    // MARK: - KeyShareRequest Tests

    func testKeyShareRequestCreation() throws {
        let request = KeyShareRequestCreate(
            vehicleId: "vehicle_001",
            recipientPhone: "13800138000",
            recipientName: "测试用户",
            keyType: .friend,
            permissions: ["unlock", "lock"],
            validDays: 30,
            message: "测试分享"
        )

        XCTAssertEqual(request.vehicleId, "vehicle_001")
        XCTAssertEqual(request.recipientPhone, "13800138000")
        XCTAssertEqual(request.recipientName, "测试用户")
        XCTAssertEqual(request.keyType, .friend)
        XCTAssertEqual(request.permissions.count, 2)
        XCTAssertEqual(request.validDays, 30)
        XCTAssertEqual(request.message, "测试分享")
    }

    func testKeyShareRequestWithMinimalFields() throws {
        let request = KeyShareRequestCreate(
            vehicleId: "vehicle_002",
            recipientPhone: "13900139000",
            recipientName: nil,
            keyType: .temporary,
            permissions: ["unlock"],
            validDays: nil,
            message: nil
        )

        XCTAssertEqual(request.vehicleId, "vehicle_002")
        XCTAssertNil(request.recipientName)
        XCTAssertNil(request.validDays)
        XCTAssertNil(request.message)
        XCTAssertEqual(request.permissions, ["unlock"])
    }

    func testKeyShareRequestCodingKeys() throws {
        // 验证 JSON 序列化使用 snake_case 格式
        let request = KeyShareRequestCreate(
            vehicleId: "v001",
            recipientPhone: "13800138000",
            recipientName: "Name",
            keyType: .owner,
            permissions: ["unlock"],
            validDays: 7,
            message: "Hello"
        )

        let encoder = JSONEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        let data = try encoder.encode(request)

        let json = try JSONSerialization.jsonObject(with: data) as! [String: Any]
        XCTAssertNotNil(json["vehicle_id"])
        XCTAssertNotNil(json["recipient_phone"])
        XCTAssertNotNil(json["recipient_name"])
        XCTAssertNotNil(json["key_type"])
        XCTAssertNotNil(json["valid_days"])
    }

    // MARK: - ShareRequestStatus Tests

    func testShareRequestStatusRawValues() throws {
        XCTAssertEqual(ShareRequestStatus.pending.rawValue, "PENDING")
        XCTAssertEqual(ShareRequestStatus.approved.rawValue, "APPROVED")
        XCTAssertEqual(ShareRequestStatus.rejected.rawValue, "REJECTED")
        XCTAssertEqual(ShareRequestStatus.expired.rawValue, "EXPIRED")
        XCTAssertEqual(ShareRequestStatus.cancelled.rawValue, "CANCELLED")
    }

    func testShareRequestStatusDecoding() throws {
        let json = """
        ["PENDING", "APPROVED", "REJECTED", "EXPIRED", "CANCELLED"]
        """
        let data = json.data(using: .utf8)!
        let statuses = try JSONDecoder().decode([ShareRequestStatus].self, from: data)

        XCTAssertEqual(statuses, [.pending, .approved, .rejected, .expired, .cancelled])
    }

    func testKeyShareRequestMockData() throws {
        let mocks = KeyShareRequest.mockRequests

        XCTAssertEqual(mocks.count, 2)
        XCTAssertEqual(mocks[0].requesterName, "张三")
        XCTAssertEqual(mocks[0].status, .pending)
        XCTAssertEqual(mocks[0].keyType, .friend)
        XCTAssertEqual(mocks[1].requesterName, "李四")
        XCTAssertEqual(mocks[1].status, .pending)
        XCTAssertEqual(mocks[1].keyType, .temporary)
    }

    // MARK: - KeyType Tests

    func testKeyTypeRawValues() throws {
        XCTAssertEqual(KeyType.owner.rawValue, "OWNER")
        XCTAssertEqual(KeyType.friend.rawValue, "FRIEND")
        XCTAssertEqual(KeyType.service.rawValue, "SERVICE")
        XCTAssertEqual(KeyType.temporary.rawValue, "TEMPORARY")
    }

    func testKeyTypeDecoding() throws {
        let json = """
        ["OWNER", "FRIEND", "SERVICE", "TEMPORARY"]
        """
        let data = json.data(using: .utf8)!
        let types = try JSONDecoder().decode([KeyType].self, from: data)

        XCTAssertEqual(types, [.owner, .friend, .service, .temporary])
    }

    // MARK: - Vehicle Mock Data

    func testVehicleMockData() throws {
        let mocks = Vehicle.mockVehicles

        XCTAssertEqual(mocks.count, 2)
        XCTAssertEqual(mocks[0].brand, "BMW")
        XCTAssertEqual(mocks[0].model, "iX3")
        XCTAssertEqual(mocks[0].keyInfo?.keyType, .owner)
        XCTAssertEqual(mocks[1].brand, "Mercedes-Benz")
        XCTAssertEqual(mocks[1].model, "EQC")
        XCTAssertEqual(mocks[1].keyInfo?.keyType, .friend)
    }

    // MARK: - Hashable Conformance

    func testVehicleHashable() throws {
        let v1 = Vehicle.mockVehicles[0]
        let v2 = Vehicle.mockVehicles[0]
        let v3 = Vehicle.mockVehicles[1]

        XCTAssertEqual(v1, v2)
        XCTAssertNotEqual(v1, v3)

        let set: Set<Vehicle> = [v1, v2, v3]
        XCTAssertEqual(set.count, 2) // v1 和 v2 重复
    }
}
