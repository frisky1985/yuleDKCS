Pod::Spec.new do |s|
  s.name             = 'yuledkcs'
  s.version          = '1.0.0'
  s.summary          = 'YuleDKCS Digital Key SDK for Flutter'
  s.description      = <<-DESC
A Flutter FFI plugin for YuleDKCS Digital Key system.
Supports secure key management and BLE communication with vehicles.
                       DESC
  s.homepage         = 'https://github.com/nousresearch/yuleDKCS'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Nous Research' => 'dev@nousresearch.com' }
  s.source           = { :path => '.' }
  
  s.source_files = 'Classes/**/*'
  s.dependency 'Flutter'
  s.platform = :ios, '13.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 
    'DEFINES_MODULE' => 'YES', 
    'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386',
    'OTHER_LDFLAGS' => '-framework yuledkcs',
    'FRAMEWORK_SEARCH_PATHS' => '$(PROJECT_DIR)/../ffi/build/ios'
  }
  
  s.user_target_xcconfig = {
    'FRAMEWORK_SEARCH_PATHS' => '$(PROJECT_DIR)/../ffi/build/ios'
  }

  # 包含预编译的 FFI 框架
  s.vendored_frameworks = '../../../ffi/build/ios/yuledkcs.xcframework'

  # 指定需要链接的系统框架
  s.frameworks = 'Foundation', 'CoreBluetooth', 'Security'

  # Swift 版本
  s.swift_version = '5.0'
  
  s.resource_bundles = {
    'yuledkcs_privacy' => ['Resources/PrivacyInfo.xcprivacy']
  }
end