import SwiftUI

struct DKButton: View {
    let title: String
    let icon: String?
    let style: DKButtonStyle
    let size: DKButtonSize
    let action: () -> Void
    
    @State private var isPressed = false
    
    init(
        _ title: String,
        icon: String? = nil,
        style: DKButtonStyle = .primary,
        size: DKButtonSize = .medium,
        action: @escaping () -> Void
    ) {
        self.title = title
        self.icon = icon
        self.style = style
        self.size = size
        self.action = action
    }
    
    var body: some View {
        Button(action: action) {
            HStack(spacing: 8) {
                if let icon = icon {
                    Image(systemName: icon)
                        .font(.system(size: size.iconSize, weight: .medium))
                }
                Text(title)
                    .font(.system(size: size.fontSize, weight: .semibold))
            }
            .frame(minWidth: size.minWidth, minHeight: size.height)
            .padding(.horizontal, size.horizontalPadding)
            .background(backgroundColor)
            .foregroundColor(foregroundColor)
            .cornerRadius(size.cornerRadius)
            .overlay(
                RoundedRectangle(cornerRadius: size.cornerRadius)
                    .stroke(borderColor, lineWidth: style == .outline ? 1.5 : 0)
            )
        }
        .disabled(style == .disabled)
        .scaleEffect(isPressed ? 0.95 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
    
    private var backgroundColor: Color {
        switch style {
        case .primary:
            return Color.accentColor
        case .secondary:
            return Color.secondary.opacity(0.2)
        case .outline:
            return Color.clear
        case .danger:
            return Color.red
        case .disabled:
            return Color.gray.opacity(0.3)
        }
    }
    
    private var foregroundColor: Color {
        switch style {
        case .primary, .danger:
            return .white
        case .secondary:
            return .primary
        case .outline:
            return .accentColor
        case .disabled:
            return .gray
        }
    }
    
    private var borderColor: Color {
        switch style {
        case .outline:
            return .accentColor
        default:
            return .clear
        }
    }
}

enum DKButtonStyle {
    case primary
    case secondary
    case outline
    case danger
    case disabled
}

enum DKButtonSize {
    case small
    case medium
    case large
    
    var fontSize: CGFloat {
        switch self {
        case .small: return 13
        case .medium: return 15
        case .large: return 17
        }
    }
    
    var iconSize: CGFloat {
        switch self {
        case .small: return 13
        case .medium: return 15
        case .large: return 17
        }
    }
    
    var height: CGFloat {
        switch self {
        case .small: return 32
        case .medium: return 44
        case .large: return 52
        }
    }
    
    var minWidth: CGFloat {
        switch self {
        case .small: return 60
        case .medium: return 80
        case .large: return 120
        }
    }
    
    var horizontalPadding: CGFloat {
        switch self {
        case .small: return 12
        case .medium: return 16
        case .large: return 20
        }
    }
    
    var cornerRadius: CGFloat {
        switch self {
        case .small: return 8
        case .medium: return 10
        case .large: return 12
        }
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 20) {
        DKButton("Primary", style: .primary) {}
        DKButton("Secondary", style: .secondary) {}
        DKButton("Outline", style: .outline) {}
        DKButton("Danger", style: .danger) {}
        DKButton("With Icon", icon: "key", style: .primary) {}
        DKButton("Small", style: .primary, size: .small) {}
        DKButton("Large", style: .primary, size: .large) {}
    }
    .padding()
}
