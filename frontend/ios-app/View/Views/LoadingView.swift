import SwiftUI

struct LoadingView: View {
    let message: String?
    let style: LoadingStyle
    
    init(message: String? = nil, style: LoadingStyle = .default) {
        self.message = message
        self.style = style
    }
    
    var body: some View {
        VStack(spacing: 20) {
            switch style {
            case .default:
                ProgressView()
                    .progressViewStyle(CircularProgressViewStyle())
                    .scaleEffect(1.5)
                
            case .overlay:
                ZStack {
                    Color.black.opacity(0.3)
                        .edgesIgnoringSafeArea(.all)
                    
                    VStack(spacing: 16) {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle(tint: .white))
                            .scaleEffect(1.5)
                        
                        if let message = message {
                            Text(message)
                                .font(.system(size: 15, weight: .medium))
                                .foregroundColor(.white)
                        }
                    }
                    .padding(32)
                    .background(Color.black.opacity(0.7))
                    .cornerRadius(16)
                }
                
            case .inline:
                HStack(spacing: 12) {
                    ProgressView()
                        .progressViewStyle(CircularProgressViewStyle())
                    
                    if let message = message {
                        Text(message)
                            .font(.system(size: 15))
                            .foregroundColor(.secondary)
                    }
                }
            }
            
            if style != .overlay, let message = message {
                Text(message)
                    .font(.system(size: 15))
                    .foregroundColor(.secondary)
                    .multilineTextAlignment(.center)
            }
        }
    }
}

enum LoadingStyle {
    case `default`
    case overlay
    case inline
}

// MARK: - View Extension

extension View {
    func loadingOverlay(isLoading: Bool, message: String? = nil) -> some View {
        ZStack {
            self
            
            if isLoading {
                LoadingView(message: message, style: .overlay)
            }
        }
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 40) {
        LoadingView()
        
        LoadingView(message: "加载中...")
        
        LoadingView(message: "请稍候", style: .inline)
    }
    .padding()
}

#Preview("Overlay") {
    VStack {
        Text("Content")
            .font(.system(size: 20))
            .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
    .loadingOverlay(isLoading: true, message: "处理中...")
}
