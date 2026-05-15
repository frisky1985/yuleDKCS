import SwiftUI
import UIKit

class KeyListViewController: UIHostingController<KeyListView> {
    private let viewModel = KeyListViewModel()
    
    init() {
        let listView = KeyListView(viewModel: viewModel)
        super.init(rootView: listView)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        configureNavigationBar()
        viewModel.loadData()
    }
    
    private func configureNavigationBar() {
        title = "我的钥匙"
        
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "plus"),
            style: .plain,
            target: self,
            action: #selector(addKeyTapped)
        )
    }
    
    @objc private func addKeyTapped() {
        let addKeyVC = AddKeyViewController()
        navigationController?.pushViewController(addKeyVC, animated: true)
    }
}

struct KeyListView: View {
    @ObservedObject var viewModel: KeyListViewModel
    @State private var showingAddKey = false
    
    var body: some View {
        ZStack {
            Color(UIColor.systemGroupedBackground)
                .edgesIgnoringSafeArea(.all)
            
            if viewModel.isLoading {
                LoadingView(message: "加载中...")
            } else if viewModel.vehicles.isEmpty {
                emptyStateView
            } else {
                mainContent
            }
        }
        .alert("错误", isPresented: .constant(viewModel.errorMessage != nil)) {
            Button("确定") {
                viewModel.errorMessage = nil
            }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
    }
    
    private var emptyStateView: some View {
        VStack(spacing: 20) {
            Image(systemName: "key.slash")
                .font(.system(size: 60))
                .foregroundColor(.secondary)
            
            Text("暂无数字钥匙")
                .font(.system(size: 18, weight: .medium))
                .foregroundColor(.primary)
            
            Text("点击右上角 + 添加您的第一把钥匙")
                .font(.system(size: 14))
                .foregroundColor(.secondary)
        }
    }
    
    private var mainContent: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Share Requests Section
                if !viewModel.shareRequests.isEmpty {
                    shareRequestsSection
                }
                
                // Vehicles Section
                vehiclesSection
            }
            .padding()
        }
        .refreshable {
            viewModel.loadData()
        }
    }
    
    private var shareRequestsSection: some View {
        DKCard(
            title: "分享请求",
            subtitle: "\(viewModel.shareRequests.count) 条待处理",
            icon: "person.badge.plus",
            iconColor: .orange
        ) {
            VStack(spacing: 12) {
                ForEach(viewModel.shareRequests.filter { $0.status == .pending }) { request in
                    shareRequestRow(request)
                }
            }
        }
    }
    
    private func shareRequestRow(_ request: KeyShareRequest) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(request.requesterName)
                        .font(.system(size: 16, weight: .semibold))
                    
                    Text(request.vehicleInfo)
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Text(request.keyType.displayName)
                    .font(.system(size: 12, weight: .medium))
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(Color.accentColor.opacity(0.1))
                    .cornerRadius(6)
            }
            
            if let message = request.message {
                Text(message)
                    .font(.system(size: 14))
                    .foregroundColor(.secondary)
                    .padding(.top, 4)
            }
            
            HStack(spacing: 12) {
                DKButton("拒绝", style: .outline, size: .small) {
                    viewModel.rejectRequest(request)
                }
                
                DKButton("批准", style: .primary, size: .small) {
                    viewModel.approveRequest(request)
                }
            }
            .padding(.top, 8)
        }
        .padding()
        .background(Color(UIColor.tertiarySystemGroupedBackground))
        .cornerRadius(12)
    }
    
    private var vehiclesSection: some View {
        VStack(spacing: 16) {
            ForEach(viewModel.vehicles) { vehicle in
                NavigationLink(destination: KeyDetailView(vehicle: vehicle)) {
                    KeyCardView(vehicle: vehicle)
                }
                .buttonStyle(PlainButtonStyle())
            }
        }
    }
}

// MARK: - Preview

#Preview {
    NavigationView {
        KeyListView(viewModel: KeyListViewModel())
    }
}
