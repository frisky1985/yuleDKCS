import SwiftUI

struct DKCard<Content: View>: View {
    let title: String?
    let subtitle: String?
    let icon: String?
    let iconColor: Color?
    let content: Content
    
    init(
        title: String? = nil,
        subtitle: String? = nil,
        icon: String? = nil,
        iconColor: Color? = nil,
        @ViewBuilder content: () -> Content
    ) {
        self.title = title
        self.subtitle = subtitle
        self.icon = icon
        self.iconColor = iconColor
        self.content = content()
    }
    
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            if title != nil || icon != nil {
                HStack(spacing: 12) {
                    if let icon = icon {
                        Image(systemName: icon)
                            .font(.system(size: 20, weight: .medium))
                            .foregroundColor(iconColor ?? .accentColor)
                            .frame(width: 32, height: 32)
                            .background((iconColor ?? .accentColor).opacity(0.1))
                            .cornerRadius(8)
                    }
                    
                    VStack(alignment: .leading, spacing: 2) {
                        if let title = title {
                            Text(title)
                                .font(.system(size: 17, weight: .semibold))
                                .foregroundColor(.primary)
                        }
                        
                        if let subtitle = subtitle {
                            Text(subtitle)
                                .font(.system(size: 13))
                                .foregroundColor(.secondary)
                        }
                    }
                    
                    Spacer()
                }
            }
            
            content
        }
        .padding(20)
        .background(Color(UIColor.secondarySystemGroupedBackground))
        .cornerRadius(16)
        .shadow(color: Color.black.opacity(0.05), radius: 8, x: 0, y: 2)
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 20) {
        DKCard {
            Text("Simple Card")
                .font(.system(size: 16))
        }
        
        DKCard(
            title: "Card with Title",
            subtitle: "Subtitle here",
            icon: "car.fill"
        ) {
            VStack(alignment: .leading, spacing: 8) {
                Text("Vehicle Information")
                    .font(.system(size: 15))
                Text("BMW iX3 2024")
                    .font(.system(size: 14))
                    .foregroundColor(.secondary)
            }
        }
        
        DKCard(
            title: "Battery Status",
            icon: "battery.100",
            iconColor: .green
        ) {
            HStack {
                VStack(alignment: .leading) {
                    Text("85%")
                        .font(.system(size: 28, weight: .bold))
                    Text("约 350 km")
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                }
                Spacer()
            }
        }
    }
    .padding()
    .background(Color(UIColor.systemGroupedBackground))
}
