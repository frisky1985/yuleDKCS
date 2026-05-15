import SwiftUI

struct KeyCardView: View {
    let vehicle: Vehicle
    let onTap: (() -> Void)?
    let onLongPress: (() -> Void)?
    
    init(vehicle: Vehicle, onTap: (() -> Void)? = nil, onLongPress: (() -> Void)? = nil) {
        self.vehicle = vehicle
        self.onTap = onTap
        self.onLongPress = onLongPress
    }
    
    var body: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Image(systemName: "car.fill")
                    .font(.system(size: 24, weight: .medium))
                    .foregroundColor(.white)
                    .frame(width: 48, height: 48)
                    .background(Color.accentColor)
                    .cornerRadius(12)
                
                VStack(alignment: .leading, spacing: 4) {
                    Text(vehicle.nickname ?? vehicle.model)
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundColor(.primary)
                    
                    Text("\(vehicle.brand) \(vehicle.model)")
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                // Online status
                Circle()
                    .fill(vehicle.status.isOnline ? Color.green : Color.gray)
                    .frame(width: 12, height: 12)
            }
            .padding(.horizontal, 20)
            .padding(.top, 20)
            .padding(.bottom, 16)
            
            Divider()
                .padding(.horizontal, 20)
            
            // Key Info
            HStack(spacing: 24) {
                keyInfoItem(
                    icon: "key.fill",
                    label: "钥匙类型",
                    value: vehicle.keyInfo?.keyType.displayName ?? "未知"
                )
                
                keyInfoItem(
                    icon: "battery.100",
                    label: "电量",
                    value: "\(vehicle.status.batteryLevel)%"
                )
                
                keyInfoItem(
                    icon: "speedometer",
                    label: "里程",
                    value: "\(vehicle.status.mileage) km"
                )
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 16)
            
            // Action buttons
            if let permissions = vehicle.keyInfo?.permissions {
                HStack(spacing: 12) {
                    if permissions.contains("unlock") {
                        actionButton(icon: "lock.open", label: "解锁")
                    }
                    if permissions.contains("lock") {
                        actionButton(icon: "lock", label: "锁定")
                    }
                    if permissions.contains("start") {
                        actionButton(icon: "play.circle", label: "启动")
                    }
                }
                .padding(.horizontal, 20)
                .padding(.bottom, 20)
            }
        }
        .background(Color(UIColor.secondarySystemGroupedBackground))
        .cornerRadius(16)
        .shadow(color: Color.black.opacity(0.08), radius: 12, x: 0, y: 4)
        .onTapGesture {
            onTap?()
        }
        .onLongPressGesture {
            onLongPress?()
        }
    }
    
    private func keyInfoItem(icon: String, label: String, value: String) -> some View {
        VStack(spacing: 4) {
            Image(systemName: icon)
                .font(.system(size: 14))
                .foregroundColor(.accentColor)
            
            Text(value)
                .font(.system(size: 15, weight: .semibold))
                .foregroundColor(.primary)
            
            Text(label)
                .font(.system(size: 11))
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
    }
    
    private func actionButton(icon: String, label: String) -> some View {
        Button(action: {
            // Handle action
        }) {
            VStack(spacing: 6) {
                Image(systemName: icon)
                    .font(.system(size: 18, weight: .medium))
                    .foregroundColor(.accentColor)
                
                Text(label)
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.primary)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 12)
            .background(Color.accentColor.opacity(0.1))
            .cornerRadius(10)
        }
    }
}

// MARK: - KeyType Extension

extension KeyType {
    var displayName: String {
        switch self {
        case .owner: return "车主"
        case .friend: return "亲友"
        case .service: return "服务"
        case .temporary: return "临时"
        }
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 20) {
        KeyCardView(vehicle: Vehicle.mockVehicles[0])
        KeyCardView(vehicle: Vehicle.mockVehicles[1])
    }
    .padding()
    .background(Color(UIColor.systemGroupedBackground))
}
