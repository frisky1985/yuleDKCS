import SwiftUI

struct VehicleStatusView: View {
    let status: VehicleStatus
    let isRefreshing: Bool
    
    var body: some View {
        VStack(spacing: 20) {
            // Online Status
            HStack {
                Circle()
                    .fill(status.isOnline ? Color.green : Color.gray)
                    .frame(width: 10, height: 10)
                
                Text(status.isOnline ? "在线" : "离线")
                    .font(.system(size: 15, weight: .medium))
                    .foregroundColor(status.isOnline ? .green : .secondary)
                
                Spacer()
                
                if isRefreshing {
                    ProgressView()
                        .scaleEffect(0.8)
                }
            }
            
            // Battery Level
            statusRow(
                icon: "battery.100",
                iconColor: batteryColor,
                label: "电量",
                value: "\(status.batteryLevel)%",
                detail: status.fuelLevel != nil ? "续航约 \(status.batteryLevel * 4) km" : nil
            )
            
            // Fuel Level (if available)
            if let fuelLevel = status.fuelLevel {
                statusRow(
                    icon: "fuelpump.fill",
                    iconColor: fuelColor(for: fuelLevel),
                    label: "油量",
                    value: "\(fuelLevel)%",
                    detail: nil
                )
            }
            
            // Mileage
            statusRow(
                icon: "speedometer",
                iconColor: .blue,
                label: "里程",
                value: formatMileage(status.mileage),
                detail: nil
            )
            
            // Location
            if let location = status.location {
                Divider()
                
                HStack {
                    Image(systemName: "location.fill")
                        .font(.system(size: 16))
                        .foregroundColor(.accentColor)
                        .frame(width: 24)
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("位置")
                            .font(.system(size: 13))
                            .foregroundColor(.secondary)
                        
                        Text(location.address ?? "未知位置")
                            .font(.system(size: 15))
                            .foregroundColor(.primary)
                    }
                    
                    Spacer()
                }
            }
            
            // Last Updated
            HStack {
                Spacer()
                Text("更新于 \(formatDate(status.lastUpdated))")
                    .font(.system(size: 12))
                    .foregroundColor(.secondary)
            }
        }
        .padding(20)
        .background(Color(UIColor.secondarySystemGroupedBackground))
        .cornerRadius(16)
        .shadow(color: Color.black.opacity(0.05), radius: 8, x: 0, y: 2)
    }
    
    private func statusRow(
        icon: String,
        iconColor: Color,
        label: String,
        value: String,
        detail: String?
    ) -> some View {
        HStack {
            Image(systemName: icon)
                .font(.system(size: 16))
                .foregroundColor(iconColor)
                .frame(width: 24)
            
            Text(label)
                .font(.system(size: 15))
                .foregroundColor(.secondary)
            
            Spacer()
            
            VStack(alignment: .trailing, spacing: 2) {
                Text(value)
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundColor(.primary)
                
                if let detail = detail {
                    Text(detail)
                        .font(.system(size: 12))
                        .foregroundColor(.secondary)
                }
            }
        }
    }
    
    private var batteryColor: Color {
        if status.batteryLevel > 50 {
            return .green
        } else if status.batteryLevel > 20 {
            return .orange
        } else {
            return .red
        }
    }
    
    private func fuelColor(for level: Int) -> Color {
        if level > 30 {
            return .blue
        } else if level > 15 {
            return .orange
        } else {
            return .red
        }
    }
    
    private func formatMileage(_ km: Int) -> String {
        if km >= 10000 {
            return String(format: "%.1f 万km", Double(km) / 10000.0)
        } else {
            return "\(km) km"
        }
    }
    
    private func formatDate(_ date: Date) -> String {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .abbreviated
        return formatter.localizedString(for: date, relativeTo: Date())
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 20) {
        VehicleStatusView(
            status: Vehicle.mockVehicles[0].status,
            isRefreshing: false
        )
        
        VehicleStatusView(
            status: Vehicle.mockVehicles[1].status,
            isRefreshing: true
        )
    }
    .padding()
    .background(Color(UIColor.systemGroupedBackground))
}
