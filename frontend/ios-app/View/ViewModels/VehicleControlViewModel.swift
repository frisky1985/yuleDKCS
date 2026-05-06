import Foundation
import Combine

class VehicleControlViewModel: ObservableObject {
    @Published var selectedVehicle: Vehicle?
    @Published var isOperating: Bool = false
    @Published var operationMessage: String?
    @Published var errorMessage: String?
    
    private let vehicleService = VehicleService.shared
    private let keyService = KeyService.shared
    private var cancellables = Set<AnyCancellable>()
    
    init() {
        bindToService()
    }
    
    // MARK: - Public Methods
    
    func selectVehicle(_ vehicle: Vehicle) {
        selectedVehicle = vehicle
    }
    
    func unlock() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.unlockVehicle(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "车辆已解锁"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func lock() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.lockVehicle(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "车辆已锁定"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func startEngine() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.startEngine(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "引擎已启动"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func stopEngine() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.stopEngine(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "引擎已关闭"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func openTrunk() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.openTrunk(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "后备箱已打开"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func startClimate() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.startClimate(vehicleId: vehicleId, temperature: 22.0) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "空调已开启"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func stopClimate() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.stopClimate(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "空调已关闭"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func honkAndFlash() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.honkAndFlash(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.operationMessage = "已鸣笛闪灯"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func locateVehicle() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.locateVehicle(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success(let location):
                    self?.operationMessage = "车辆位置: \(location.address ?? "未知")"
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func refreshStatus() {
        guard let vehicleId = selectedVehicle?.id else { return }
        
        vehicleService.refreshVehicleStatus(vehicleId: vehicleId) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success(let status):
                    // Update selected vehicle with new status
                    if let vehicle = self?.selectedVehicle {
                        self?.selectedVehicle = Vehicle(
                            id: vehicle.id,
                            vin: vehicle.vin,
                            brand: vehicle.brand,
                            model: vehicle.model,
                            year: vehicle.year,
                            color: vehicle.color,
                            nickname: vehicle.nickname,
                            status: status,
                            keyInfo: vehicle.keyInfo
                        )
                    }
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    // MARK: - Private Methods
    
    private func bindToService() {
        vehicleService.$isOperating
            .receive(on: DispatchQueue.main)
            .assign(to: &$isOperating)
        
        // Auto-select first vehicle if available
        keyService.$vehicles
            .receive(on: DispatchQueue.main)
            .sink { [weak self] vehicles in
                if self?.selectedVehicle == nil && !vehicles.isEmpty {
                    self?.selectedVehicle = vehicles.first
                }
            }
            .store(in: &cancellables)
    }
}
