import SwiftUI
import UIKit

struct SettingsView: View {
    @State private var notificationsEnabled = true
    @State private var biometricEnabled = true
    @State private var autoUnlock = true
    @State private var distanceThreshold = 5.0
    
    var body: some View {
        List {
            // Account Section
            Section(header: Text("账户")) {
                HStack {
                    Image(systemName: "person.circle.fill")
                        .font(.system(size: 40))
                        .foregroundColor(.accentColor)
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("用户名")
                            .font(.system(size: 17, weight: .medium))
                        
                        Text("user@example.com")
                            .font(.system(size: 14))
                            .foregroundColor(.secondary)
                    }
                    
                    Spacer()
                    
                    Image(systemName: "chevron.right")
                        .font(.system(size: 14))
                        .foregroundColor(.secondary)
                }
                .padding(.vertical, 8)
            }
            
            // Notifications Section
            Section(header: Text("通知")) {
                Toggle("推送通知", isOn: $notificationsEnabled)
                Toggle("钥匙状态提醒", isOn: .constant(true))
                    .disabled(!notificationsEnabled)
                Toggle("分享请求通知", isOn: .constant(true))
                    .disabled(!notificationsEnabled)
            }
            
            // Security Section
            Section(header: Text("安全")) {
                Toggle("生物识别", isOn: $biometricEnabled)
                Toggle("自动解锁", isOn: $autoUnlock)
                
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("解锁距离")
                        
                        Spacer()
                        
                        Text("\(Int(distanceThreshold)) 米")
                            .foregroundColor(.secondary)
                    }
                    
                    Slider(value: $distanceThreshold, in: 1...10, step: 1)
                        .disabled(!autoUnlock)
                }
            }
            
            // About Section
            Section(header: Text("关于")) {
                HStack {
                    Text("版本")
                    Spacer()
                    Text("1.0.0")
                        .foregroundColor(.secondary)
                }
                
                NavigationLink(destination: Text("使用条款")) {
                    Text("使用条款")
                }
                
                NavigationLink(destination: Text("隐私政策")) {
                    Text("隐私政策")
                }
                
                NavigationLink(destination: Text("帮助中心")) {
                    Text("帮助中心")
                }
            }
            
            // Debug Section
            Section(header: Text("开发")) {
                NavigationLink(destination: DebugView()) {
                    Text("调试信息")
                }
                
                Button("清除缓存") {
                    // Clear cache
                }
                .foregroundColor(.red)
            }
            
            // Logout
            Section {
                Button("退出登录") {
                    // Logout
                }
                .foregroundColor(.red)
                .frame(maxWidth: .infinity, alignment: .center)
            }
        }
        .navigationTitle("设置")
        .navigationBarTitleDisplayMode(.large)
    }
}

struct DebugView: View {
    var body: some View {
        List {
            Section(header: Text("SDK 信息")) {
                infoRow("版本", "1.0.0")
                infoRow("构建", "1")
                infoRow("环境", "生产")
            }
            
            Section(header: Text("设备信息")) {
                infoRow("系统版本", UIDevice.current.systemVersion)
                infoRow("设备型号", UIDevice.current.model)
                infoRow("设备名称", UIDevice.current.name)
            }
            
            Section(header: Text("权限状态")) {
                permissionRow("NFC", enabled: true)
                permissionRow("蓝牙", enabled: true)
                permissionRow("定位", enabled: true)
                permissionRow("通知", enabled: true)
            }
            
            Section(header: Text("连接状态")) {
                infoRow("BLE", "已连接")
                infoRow("NFC", "就绪")
                infoRow("UWB", "支持")
            }
        }
        .navigationTitle("调试信息")
    }
    
    private func infoRow(_ label: String, _ value: String) -> some View {
        HStack {
            Text(label)
                .foregroundColor(.primary)
            
            Spacer()
            
            Text(value)
                .foregroundColor(.secondary)
        }
    }
    
    private func permissionRow(_ name: String, enabled: Bool) -> some View {
        HStack {
            Text(name)
            
            Spacer()
            
            Image(systemName: enabled ? "checkmark.circle.fill" : "xmark.circle.fill")
                .foregroundColor(enabled ? .green : .red)
        }
    }
}

class SettingsViewController: UIHostingController<SettingsView> {
    init() {
        super.init(rootView: SettingsView())
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "设置"
    }
}

// MARK: - Preview

#Preview {
    NavigationView {
        SettingsView()
    }
}
