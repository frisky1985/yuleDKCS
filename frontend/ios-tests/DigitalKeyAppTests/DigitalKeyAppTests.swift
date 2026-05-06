import XCTest
@testable import DigitalKeySDK

class DigitalKeyAppTests: XCTestCase {
    
    override func setUpWithError() throws {
        // Put setup code here. This method is called before the invocation of each test method in the class.
    }
    
    override func tearDownWithError() throws {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
    }
    
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
        XCTAssertEqual(vehicle.keyInfo?.keyType, .owner)
    }
    
    func testKeyServiceInitialization() throws {
        let service = KeyService.shared
        service.initialize()
        
        // Test that service initializes without errors
        XCTAssertNotNil(service)
    }
    
    func testVehicleServiceInitialization() throws {
        let service = VehicleService.shared
        service.initialize()
        
        // Test that service initializes without errors
        XCTAssertNotNil(service)
    }
    
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
        XCTAssertEqual(request.keyType, .friend)
        XCTAssertEqual(request.permissions.count, 2)
    }
}
