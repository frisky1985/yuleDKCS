import SwiftUI
import UIKit

struct KeyDetailView: View {
    let vehicle: Vehicle
    
    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Vehicle Info Card
                DKCard(
                    title: vehicle.nickname ?? vehicle.model,
                    subtitle: "\(vehicle.brand) \(vehicle.year)",
                    icon: "car.fill"
                ) {
                    VStack(alignment: .leading, spacing: 12) {
                        infoRow(label: "VIN", value: vehicle.vin)
                        infoRow(label: "颜色", value: vehicle.color)
                        infoRow(label: "型号", value: vehicle.model)
                    }
                }
                
                // Key Info Card
                if let keyInfo = vehicle.keyInfo {
                    DKCard(
                        title: "钥匙信息",
                        icon: "key.fill",
                        iconColor: .blue
                    ) {
                        VStack(alignment: .leading, spacing: 12) {
                            infoRow(label: "类型", value: keyInfo.keyType.displayName)
                            infoRow(label: "权限", value: keyInfo.permissions.joined(separator: ", "))
                            
                            if let validUntil = keyInfo.validUntil {
                                infoRow(label: "有效期至", value: formatDate(validUntil))
                            } else {
                                infoRow(label: "有效期", value: "永久有效")
                            }
                        }
                    }
                }
                
                // Vehicle Status Card
                VehicleStatusView(
                    status: vehicle.status,
                    isRefreshing: false
                )
                
                // Quick Actions
                DKCard(title: "快捷操作", icon: "hand.tap") {
                    HStack(spacing: 16) {
                        quickActionButton(
                            icon: "location.fill",
                            label: "定位",
                            color: .blue
                        )
                        
                        quickActionButton(
                            icon: "bell.fill",
                            label: "鸣笛",
                            color: .orange
                        )
                        
                        quickActionButton(
                            icon: "square.and.arrow.up",
                            label: "分享",
                            color: .green
                        )
                    }
                }
            }
            .padding()
        }
        .background(Color(UIColor.systemGroupedBackground))
        .navigationTitle(vehicle.nickname ?? vehicle.model)
        .navigationBarTitleDisplayMode(.large)
    }
    
    private func infoRow(label: String, value: String) -> some View {
        HStack {
            Text(label)
                .font(.system(size: 14))
                .foregroundColor(.secondary)
                .frame(width: 80, alignment: .leading)
            
            Text(value)
                .font(.system(size: 15))
                .foregroundColor(.primary)
            
            Spacer()
        }
    }
    
    private func quickActionButton(icon: String, label: String, color: Color) -> some View {
        Button(action: {}) {
            VStack(spacing: 8) {
                Image(systemName: icon)
                    .font(.system(size: 20, weight: .medium))
                    .foregroundColor(color)
                
                Text(label)
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.primary)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 16)
            .background(color.opacity(0.1))
            .cornerRadius(12)
        }
    }
    
    private func formatDate(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .none
        return formatter.string(from: date)
    }
}

class KeyDetailViewController: UIHostingController<KeyDetailView> {
    let vehicle: Vehicle
    
    init(vehicle: Vehicle) {
        self.vehicle = vehicle
        super.init(rootView: KeyDetailView(vehicle: vehicle))
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = vehicle.nickname ?? vehicle.model
    }
}

// MARK: - Preview

#Preview {
    NavigationView {
        KeyDetailView(vehicle: Vehicle.mockVehicles[0])
    }
}
