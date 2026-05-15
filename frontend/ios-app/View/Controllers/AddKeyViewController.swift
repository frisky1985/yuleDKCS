import SwiftUI
import UIKit

struct AddKeyView: View {
    @StateObject private var viewModel = KeyShareViewModel()
    @State private var showingSuccess = false
    @Environment(\.presentationMode) var presentationMode
    
    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Instructions
                instructionsCard
                
                // Vehicle Selection
                if !viewModel.vehicles.isEmpty {
                    vehicleSelectionCard
                }
                
                // Form
                formCard
                
                // Submit Button
                submitButton
            }
            .padding()
        }
        .background(Color(UIColor.systemGroupedBackground))
        .navigationTitle("添加钥匙")
        .navigationBarTitleDisplayMode(.large)
        .alert("成功", isPresented: $showingSuccess) {
            Button("确定") {
                presentationMode.wrappedValue.dismiss()
            }
        } message: {
            Text("钥匙分享请求已发送")
        }
    }
    
    private var instructionsCard: some View {
        DKCard(
            title: "添加方式",
            icon: "info.circle",
            iconColor: .blue
        ) {
            VStack(alignment: .leading, spacing: 12) {
                instructionRow(
                    icon: "qrcode.viewfinder",
                    text: "扫描车辆二维码快速绑定"
                )
                
                instructionRow(
                    icon: "link",
                    text: "通过分享链接添加钥匙"
                )
                
                instructionRow(
                    icon: "person.badge.plus",
                    text: "填写信息手动添加"
                )
            }
        }
    }
    
    private func instructionRow(icon: String, text: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .font(.system(size: 16, weight: .medium))
                .foregroundColor(.accentColor)
                .frame(width: 24)
            
            Text(text)
                .font(.system(size: 15))
                .foregroundColor(.primary)
        }
    }
    
    private var vehicleSelectionCard: some View {
        DKCard(title: "选择车辆", icon: "car") {
            VStack(spacing: 12) {
                ForEach(viewModel.vehicles) { vehicle in
                    vehicleSelectionRow(vehicle: vehicle)
                }
            }
        }
    }
    
    private func vehicleSelectionRow(vehicle: Vehicle) -> some View {
        Button(action: {
            viewModel.selectVehicle(vehicle)
        }) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(vehicle.nickname ?? vehicle.model)
                        .font(.system(size: 16, weight: .medium))
                        .foregroundColor(.primary)
                    
                    Text("\(vehicle.brand) \(vehicle.model)")
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                if viewModel.selectedVehicle?.id == vehicle.id {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 22))
                        .foregroundColor(.accentColor)
                }
            }
            .padding()
            .background(Color(UIColor.tertiarySystemGroupedBackground))
            .cornerRadius(10)
        }
    }
    
    private var formCard: some View {
        DKCard(title: "填写信息", icon: "pencil") {
            VStack(spacing: 20) {
                DKTextField(
                    "接收者手机号",
                    placeholder: "请输入手机号",
                    text: $viewModel.recipientPhone,
                    keyboardType: .phonePad
                )
                
                DKTextField(
                    "接收者姓名 (选填)",
                    placeholder: "请输入姓名",
                    text: $viewModel.recipientName
                )
                
                VStack(alignment: .leading, spacing: 12) {
                    Text("钥匙类型")
                        .font(.system(size: 14, weight: .medium))
                        .foregroundColor(.secondary)
                    
                    Picker("钥匙类型", selection: $viewModel.selectedKeyType) {
                        Text("亲友钥匙").tag(KeyType.friend)
                        Text("临时钥匙").tag(KeyType.temporary)
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }
                
                VStack(alignment: .leading, spacing: 12) {
                    Text("权限选择")
                        .font(.system(size: 14, weight: .medium))
                        .foregroundColor(.secondary)
                    
                    VStack(spacing: 8) {
                        ForEach(KeyShareViewModel.availablePermissions, id: \.0) { permission in
                            permissionToggle(
                                id: permission.0,
                                label: permission.1
                            )
                        }
                    }
                }
                
                DKTextField(
                    "留言 (选填)",
                    placeholder: "添加备注信息",
                    text: $viewModel.message
                )
            }
        }
    }
    
    private func permissionToggle(id: String, label: String) -> some View {
        Button(action: {
            viewModel.togglePermission(id)
        }) {
            HStack {
                Text(label)
                    .font(.system(size: 15))
                    .foregroundColor(.primary)
                
                Spacer()
                
                Image(systemName: viewModel.isPermissionSelected(id) ? "checkmark.square.fill" : "square")
                    .font(.system(size: 22))
                    .foregroundColor(viewModel.isPermissionSelected(id) ? .accentColor : .secondary)
            }
            .padding(.vertical, 4)
        }
    }
    
    private var submitButton: some View {
        DKButton(
            "发送分享请求",
            icon: "paperplane.fill",
            style: .primary,
            size: .large
        ) {
            viewModel.createShareRequest { success in
                if success {
                    showingSuccess = true
                }
            }
        }
        .disabled(viewModel.selectedVehicle == nil || viewModel.recipientPhone.isEmpty)
        .opacity(viewModel.selectedVehicle == nil || viewModel.recipientPhone.isEmpty ? 0.5 : 1.0)
    }
}

class AddKeyViewController: UIHostingController<AddKeyView> {
    init() {
        super.init(rootView: AddKeyView())
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "添加钥匙"
    }
}

// MARK: - Preview

#Preview {
    NavigationView {
        AddKeyView()
    }
}
