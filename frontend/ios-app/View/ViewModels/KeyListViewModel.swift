import Foundation
import Combine

class KeyListViewModel: ObservableObject {
    @Published var vehicles: [Vehicle] = []
    @Published var shareRequests: [KeyShareRequest] = []
    @Published var isLoading: Bool = false
    @Published var errorMessage: String?
    
    private let keyService = KeyService.shared
    private var cancellables = Set<AnyCancellable>()
    
    init() {
        bindToService()
    }
    
    // MARK: - Public Methods
    
    func loadData() {
        keyService.loadVehicles()
        keyService.loadShareRequests()
    }
    
    func addKey() {
        // Will be handled by AddKeyViewController
    }
    
    func removeKey(_ vehicle: Vehicle) {
        keyService.removeKey(vehicleId: vehicle.id) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func approveRequest(_ request: KeyShareRequest) {
        keyService.approveShareRequest(requestId: request.id) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    func rejectRequest(_ request: KeyShareRequest) {
        keyService.rejectShareRequest(requestId: request.id) { [weak self] result in
            DispatchQueue.main.async {
                switch result {
                case .success:
                    self?.errorMessage = nil
                case .failure(let error):
                    self?.errorMessage = error.localizedDescription
                }
            }
        }
    }
    
    // MARK: - Private Methods
    
    private func bindToService() {
        keyService.$vehicles
            .receive(on: DispatchQueue.main)
            .assign(to: &$vehicles)
        
        keyService.$shareRequests
            .receive(on: DispatchQueue.main)
            .assign(to: &$shareRequests)
        
        keyService.$isLoading
            .receive(on: DispatchQueue.main)
            .assign(to: &$isLoading)
    }
}
