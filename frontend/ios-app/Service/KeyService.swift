import Foundation
import Combine

class KeyService: ObservableObject {
    static let shared = KeyService()
    
    @Published var vehicles: [Vehicle] = []
    @Published var shareRequests: [KeyShareRequest] = []
    @Published var isLoading: Bool = false
    @Published var error: Error?
    
    private var cancellables = Set<AnyCancellable>()
    
    private init() {}
    
    // MARK: - Public Methods
    
    func initialize() {
        loadVehicles()
        loadShareRequests()
    }
    
    func loadVehicles() {
        isLoading = true
        
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            self?.vehicles = Vehicle.mockVehicles
            self?.isLoading = false
        }
    }
    
    func loadShareRequests() {
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            self?.shareRequests = KeyShareRequest.mockRequests
        }
    }
    
    func addKey(vehicleId: String, completion: @escaping (Result<Vehicle, Error>) -> Void) {
        isLoading = true
        
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
            self?.isLoading = false
            
            // Mock: Create a new vehicle entry
            let newVehicle = Vehicle(
                id: "vehicle_new",
                vin: "NEW_VIN_\(Int.random(in: 1000...9999))",
                brand: "Brand",
                model: "Model",
                year: 2024,
                color: "Color",
                nickname: nil,
                status: VehicleStatus(
                    isOnline: false,
                    batteryLevel: 0,
                    fuelLevel: nil,
                    mileage: 0,
                    location: nil,
                    lastUpdated: Date()
                ),
                keyInfo: VehicleKeyInfo(
                    keyId: "key_new",
                    keyType: .owner,
                    permissions: ["unlock", "lock", "start"],
                    validFrom: Date(),
                    validUntil: nil,
                    isPrimary: true
                )
            )
            
            completion(.success(newVehicle))
        }
    }
    
    func removeKey(vehicleId: String, completion: @escaping (Result<Void, Error>) -> Void) {
        isLoading = true
        
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            self?.vehicles.removeAll { $0.id == vehicleId }
            self?.isLoading = false
            completion(.success(()))
        }
    }
    
    func approveShareRequest(requestId: String, completion: @escaping (Result<Void, Error>) -> Void) {
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            if let index = self?.shareRequests.firstIndex(where: { $0.id == requestId }) {
                let request = self?.shareRequests[index]
                let updatedRequest = KeyShareRequest(
                    id: request!.id,
                    requesterId: request!.requesterId,
                    requesterName: request!.requesterName,
                    vehicleId: request!.vehicleId,
                    vehicleInfo: request!.vehicleInfo,
                    keyType: request!.keyType,
                    permissions: request!.permissions,
                    message: request!.message,
                    status: .approved,
                    createdAt: request!.createdAt,
                    expiresAt: request!.expiresAt
                )
                self?.shareRequests[index] = updatedRequest
            }
            completion(.success(()))
        }
    }
    
    func rejectShareRequest(requestId: String, completion: @escaping (Result<Void, Error>) -> Void) {
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            if let index = self?.shareRequests.firstIndex(where: { $0.id == requestId }) {
                let request = self?.shareRequests[index]
                let updatedRequest = KeyShareRequest(
                    id: request!.id,
                    requesterId: request!.requesterId,
                    requesterName: request!.requesterName,
                    vehicleId: request!.vehicleId,
                    vehicleInfo: request!.vehicleInfo,
                    keyType: request!.keyType,
                    permissions: request!.permissions,
                    message: request!.message,
                    status: .rejected,
                    createdAt: request!.createdAt,
                    expiresAt: request!.expiresAt
                )
                self?.shareRequests[index] = updatedRequest
            }
            completion(.success(()))
        }
    }
    
    func createShareRequest(_ request: KeyShareRequestCreate, completion: @escaping (Result<KeyShareRequest, Error>) -> Void) {
        // TODO: Replace with actual API call
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            let newRequest = KeyShareRequest(
                id: "request_\(Int.random(in: 1000...9999))",
                requesterId: "current_user",
                requesterName: "我",
                vehicleId: request.vehicleId,
                vehicleInfo: "Vehicle Info",
                keyType: request.keyType,
                permissions: request.permissions,
                message: request.message,
                status: .pending,
                createdAt: Date(),
                expiresAt: Date().addingTimeInterval(86400 * 7)
            )
            completion(.success(newRequest))
        }
    }
}
