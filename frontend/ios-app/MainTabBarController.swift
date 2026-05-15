import UIKit

class MainTabBarController: UITabBarController {
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupViewControllers()
        configureTabBar()
    }
    
    // MARK: - Setup
    
    private func setupViewControllers() {
        let keyListVC = KeyListViewController()
        let keyListNav = UINavigationController(rootViewController: keyListVC)
        keyListNav.tabBarItem = UITabBarItem(
            title: "钥匙",
            image: UIImage(systemName: "key"),
            tag: 0
        )
        
        let vehicleControlVC = VehicleControlViewController()
        let vehicleControlNav = UINavigationController(rootViewController: vehicleControlVC)
        vehicleControlNav.tabBarItem = UITabBarItem(
            title: "车控",
            image: UIImage(systemName: "car"),
            tag: 1
        )
        
        let settingsVC = SettingsViewController()
        let settingsNav = UINavigationController(rootViewController: settingsVC)
        settingsNav.tabBarItem = UITabBarItem(
            title: "设置",
            image: UIImage(systemName: "gearshape"),
            tag: 2
        )
        
        viewControllers = [keyListNav, vehicleControlNav, settingsNav]
    }
    
    private func configureTabBar() {
        tabBar.tintColor = .systemBlue
        tabBar.isTranslucent = true
        
        if #available(iOS 15.0, *) {
            let appearance = UITabBarAppearance()
            appearance.configureWithDefaultBackground()
            tabBar.standardAppearance = appearance
            tabBar.scrollEdgeAppearance = appearance
        }
    }
}
