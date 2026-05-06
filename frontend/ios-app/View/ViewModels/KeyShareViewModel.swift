import Foundation
import Combine

class KeyShareViewModel: ObservableObject {
    @Published var vehicles: [Vehicle] = []
    @Published var selectedVehicle: Vehicle?
    @Published var recipientPhone: String = ""
    @Published var recipientName: String = ""
    @Published var selectedKeyType: KeyType = .friend
    @Published var selectedPermissions: Set<String> = []
    @Published var validDays: Int = 30
    @Published var message: String = ""
    @Published var isLoading: Bool = false
    @Published var errorMessage: String?
    @Published var successMessage: String?
    
    private let keyService = KeyService.shared
    private var cancellables = Set<AnyCancellable>()
    
    init() {
        bindToService()
    }
    
    // MARK: - Public Methods
    
    func selectVehicle(_ vehicle: Vehicle) {
        selectedVehicle = vehicle
    }
    
    func togglePermission(_ permission: String) {
        if selectedPermissions.contains(permission) {
            selectedPermissions.remove(permission)
        } else {
            selectedPermissions.insert(permission)
        }
    }
    
    func isPermissionSelected(_ permission: String) -> Bool {
        return selectedPermissions.contains(permission)
    }
    
    func createShareRequest(completion: @escaping (Bool) -> Void) {
        guard let vehicleId = selectedVehicle?.id else {
            errorMessage = "请选择车辆"
            completion(false)
            return
        }
        
        guard !recipientPhone.isEmpty else {
            errorMessage = "请输入接收者手机号"
            completion(false)
            return
        }
        
        guard !selectedPermissions.isEmpty else {
            errorMessage = "请至少选择一个权限"
            completion(false)
            return
        }
        
        isLoading = true
        errorMessage = nil
        
        let request = KeyShareRequestCreate(
            vehicleId: vehicleId,
            recipientPhone: recipientPhone,
            recipientName: recipientName.isEmpty ? nil : recipientName,
            keyType: selectedKeyType,
            permissions: Array(selectedPermissions),
            validDays: validDays,
            message: message.isEmpty ? nil : message
        )
        
        keyService.createShareRequest(request) { [weak self] result in
            DispatchQueue.main.async {
                self?.isLoading = false
                
                switch result {
                case .success:
                    self?.successMessage = "钥匙分享请求已发送"
                    self?.resetForm()
                    completion(true)
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                    completion(false)
                }
            }
        }
    }
    
    func resetForm() {
        recipientPhone = ""
        recipientName = ""
        selectedKeyType = .friend
        selectedPermissions = []
        validDays = 30
        message = ""
        errorMessage = nil
        successMessage = nil
    }
    
    // MARK: - Private Methods
    
    private func bindToService() {
        keyService.$vehicles
            .receive(on: DispatchQueue.main)
            .assign(to: &$vehicles)
    }
}

// MARK: - Permission Options

extension KeyShareViewModel {
    static let availablePermissions: [(String, String)] = [
        ("unlock", "解锁"),
        ("lock", "锁定"),
        ("start", "启动"),
        ("trunk", "后备箱"),
        ("climate", "空调")
    ]
}
