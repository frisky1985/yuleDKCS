import SwiftUI

struct ControlButtonView: View {
    let icon: String
    let label: String
    let color: Color
    let isEnabled: Bool
    let isLoading: Bool
    let action: () -> Void
    
    @State private var isPressed = false
    
    init(
        icon: String,
        label: String,
        color: Color = .accentColor,
        isEnabled: Bool = true,
        isLoading: Bool = false,
        action: @escaping () -> Void
    ) {
        self.icon = icon
        self.label = label
        self.color = color
        self.isEnabled = isEnabled
        self.isLoading = isLoading
        self.action = action
    }
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 12) {
                ZStack {
                    Circle()
                        .fill(isEnabled ? color : Color.gray.opacity(0.2))
                        .frame(width: 64, height: 64)
                    
                    if isLoading {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle(tint: .white))
                            .scaleEffect(1.2)
                    } else {
                        Image(systemName: icon)
                            .font(.system(size: 24, weight: .medium))
                            .foregroundColor(.white)
                    }
                }
                .overlay(
                    Circle()
                        .stroke(
                            isEnabled ? color.opacity(0.3) : Color.clear,
                            lineWidth: 3
                        )
                        .padding(-4)
                )
                
                Text(label)
                    .font(.system(size: 14, weight: .medium))
                    .foregroundColor(isEnabled ? .primary : .secondary)
            }
            .scaleEffect(isPressed ? 0.9 : 1.0)
            .opacity(isEnabled ? 1.0 : 0.5)
        }
        .disabled(!isEnabled || isLoading)
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 40) {
        HStack(spacing: 24) {
            ControlButtonView(
                icon: "lock.open",
                label: "解锁",
                color: .green
            ) {}
            
            ControlButtonView(
                icon: "lock",
                label: "锁定",
                color: .blue
            ) {}
            
            ControlButtonView(
                icon: "play.circle",
                label: "启动",
                color: .orange
            ) {}
        }
        
        HStack(spacing: 24) {
            ControlButtonView(
                icon: "trunk",
                label: "后备箱",
                color: .purple
            ) {}
            
            ControlButtonView(
                icon: "fanblades",
                label: "空调",
                color: .cyan
            ) {}
            
            ControlButtonView(
                icon: "car.rear",
                label: "鸣笛",
                color: .red
            ) {}
        }
        
        HStack(spacing: 24) {
            ControlButtonView(
                icon: "lock.open",
                label: "禁用",
                color: .gray,
                isEnabled: false
            ) {}
            
            ControlButtonView(
                icon: "lock",
                label: "加载中",
                color: .blue,
                isLoading: true
            ) {}
        }
    }
    .padding()
    .background(Color(UIColor.systemGroupedBackground))
}
