import SwiftUI

struct DKTextField: View {
    let title: String
    let placeholder: String
    let text: Binding<String>
    let keyboardType: UIKeyboardType
    let autocapitalization: TextAutocapitalization
    let autocorrection: Bool
    let secure: Bool
    let errorMessage: String?
    
    @FocusState private var isFocused: Bool
    
    init(
        _ title: String,
        placeholder: String = "",
        text: Binding<String>,
        keyboardType: UIKeyboardType = .default,
        autocapitalization: TextAutocapitalization = .sentences,
        autocorrection: Bool = true,
        secure: Bool = false,
        errorMessage: String? = nil
    ) {
        self.title = title
        self.placeholder = placeholder
        self.text = text
        self.keyboardType = keyboardType
        self.autocapitalization = autocapitalization
        self.autocorrection = autocorrection
        self.secure = secure
        self.errorMessage = errorMessage
    }
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.system(size: 14, weight: .medium))
                .foregroundColor(.secondary)
            
            Group {
                if secure {
                    SecureField(placeholder, text: text)
                } else {
                    TextField(placeholder, text: text)
                }
            }
            .font(.system(size: 16))
            .padding()
            .background(Color(UIColor.secondarySystemBackground))
            .cornerRadius(10)
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .stroke(borderColor, lineWidth: 1)
            )
            .keyboardType(keyboardType)
            .textInputAutocapitalization(autocapitalization)
            .autocorrectionDisabled(!autocorrection)
            .focused($isFocused)
            
            if let error = errorMessage {
                Text(error)
                    .font(.system(size: 12))
                    .foregroundColor(.red)
            }
        }
    }
    
    private var borderColor: Color {
        if errorMessage != nil {
            return .red
        } else if isFocused {
            return .accentColor
        } else {
            return .clear
        }
    }
}

// MARK: - Preview

#Preview {
    VStack(spacing: 20) {
        DKTextField(
            "手机号",
            placeholder: "请输入手机号",
            text: .constant(""),
            keyboardType: .phonePad
        )
        
        DKTextField(
            "密码",
            placeholder: "请输入密码",
            text: .constant(""),
            secure: true
        )
        
        DKTextField(
            "错误示例",
            text: .constant(""),
            errorMessage: "请输入有效内容"
        )
    }
    .padding()
}
