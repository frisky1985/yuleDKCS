#import "YuleDkcsPlugin.h"
#import <yuledkcs/yuledkcs.h>

@implementation YuleDkcsPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  // FFI plugin - no method channel needed
  // All calls go through dart:ffi
}

@end
