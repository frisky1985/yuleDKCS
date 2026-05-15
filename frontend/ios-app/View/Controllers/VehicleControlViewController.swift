import SwiftUI
import UIKit

struct VehicleControlView: View {
    @StateObject private var viewModel = VehicleControlViewModel()
    
    var body: some View {
        ZStack {
            Color(UIColor.systemGroupedBackground)
                .edgesIgnoringSafeArea(.all)
            
            if viewModel.selectedVehicle == nil {
                emptyStateView
            } else {
                mainContent
            }
        }
        .navigationTitle("车辆控制")
        .navigationBarTitleDisplayMode(.large)
        .alert("操作结果", isPresented: .constant(viewModel.operationMessage != nil)) {
            Button("确定") {
                viewModel.operationMessage = nil
            }
        } message: {
            Text(viewModel.operationMessage ?? "")
        }
    }
    
    private var emptyStateView: some View {
        VStack(spacing: 20) {
            Image(systemName: "car.slash")
                .font(.system(size: 60))
                .foregroundColor(.secondary)
            
            Text("暂无可用车辆")
                .font(.system(size: 18, weight: .medium))
                .foregroundColor(.primary)
            
            Text("请先在钥匙列表中添加车辆")
                .font(.system(size: 14))
                .foregroundColor(.secondary)
        }
    }
    
    private var mainContent: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Vehicle Selector
                vehicleSelector
                
                // Status Card
                if let vehicle = viewModel.selectedVehicle {
                    VehicleStatusView(
                        status: vehicle.status,
                        isRefreshing: viewModel.isOperating
                    )
                    
                    // Control Buttons
                    controlButtonsSection(vehicle: vehicle)
                }
            }
            .padding()
        }
        .refreshable {
            viewModel.refreshStatus()
        }
    }
    
    private var vehicleSelector: some View {
        Menu {
            ForEach(KeyService.shared.vehicles) { vehicle in
                Button(action: {
                    viewModel.selectVehicle(vehicle)
                }) {
                    HStack {
                        Text(vehicle.nickname ?? vehicle.model)
                        
                        if viewModel.selectedVehicle?.id == vehicle.id {
                            Image(systemName: "checkmark")
                        }
                    }
                }
            }
        } label: {
            DKCard(
                title: viewModel.selectedVehicle?.nickname ?? "选择车辆",
                subtitle: viewModel.selectedVehicle?.model,
                icon: "car.fill"
            ) {
                HStack {
                    Text("点击切换车辆")
                        .font(.system(size: 14))
                        .foregroundColor(.secondary)
                    
                    Spacer()
                    
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.system(size: 14))
                        .foregroundColor(.secondary)
                }
            }
        }
    }
    
    private func controlButtonsSection(vehicle: Vehicle) -> some View {
        DKCard(title: "远程控制", icon: "slider.horizontal.3") {
            VStack(spacing: 24) {
                // Row 1: Lock & Unlock
                HStack(spacing: 24) {
                    ControlButtonView(
                        icon: "lock.open",
                        label: "解锁",
                        color: .green,
                        isLoading: viewModel.isOperating,
                        action: viewModel.unlock
                    )
                    
                    ControlButtonView(
                        icon: "lock",
                        label: "锁定",
                        color: .blue,
                        isLoading: viewModel.isOperating,
                        action: viewModel.lock
                    )
                    
                    ControlButtonView(
                        icon: "play.circle",
                        label: "启动",
                        color: .orange,
                        isLoading: viewModel.isOperating,
                        action: viewModel.startEngine
                    )
                }
                
                // Row 2: Additional Controls
                HStack(spacing: 24) {
                    ControlButtonView(
                        icon: "trunk.rear",
                        label: "后备箱",
                        color: .purple,
                        isLoading: viewModel.isOperating,
                        action: viewModel.openTrunk
                    )
                    
                    ControlButtonView(
                        icon: "fanblades",
                        label: "空调",
                        color: .cyan,
                        isLoading: viewModel.isOperating,
                        action: viewModel.startClimate
                    )
                    
                    ControlButtonView(
                        icon: "car.rear",
                        label: "鸣笛",
                        color: .red,
                        isLoading: viewModel.isOperating,
                        action: viewModel.honkAndFlash
                    )
                }
                
                // Row 3: Locate
                HStack(spacing: 24) {
                    ControlButtonView(
                        icon: "location.fill",
                        label: "定位",
                        color: .blue,
                        isLoading: viewModel.isOperating,
                        action: viewModel.locateVehicle
                    )
                    
                    ControlButtonView(
                        icon: "xmark.circle",
                        label: "停止",
                        color: .gray,
                        isLoading: viewModel.isOperating,
                        action: viewModel.stopEngine
                    )
                    
                    ControlButtonView(
                        icon: "fanblades.slash",
                        label: "关空调",
                        color: .gray,
                        isLoading: viewModel.isOperating,
                        action: viewModel.stopClimate
                    )
                }
            }
            .padding(.top, 8)
        }
    }
}

class VehicleControlViewController: UIHostingController<VehicleControlView> {
    init() {
        super.init(rootView: VehicleControlView())
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "车辆控制"
    }
}

// MARK: - Preview

#Preview {
    NavigationView {
        VehicleControlView()
    }
}
