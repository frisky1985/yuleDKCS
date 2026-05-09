# ProGuard rules for YuleDKCS SDK

# Keep all public APIs
-keep public class com.yuledkcs.* { public *; }
-keep public class com.yuledkcs.model.* { *; }

# Native library
-keepclasseswithmembernames class * {
    native <methods>;
}

# JNI
-keep class com.yuledkcs.NativeLib {
    native <methods>;
}
