# Consumer ProGuard rules for YuleDKCS SDK

# Keep public API
-keep public class com.yuledkcs.YuleDKCS {
    public *;
}

-keep public class com.yuledkcs.KeyManager {
    public *;
}

-keep public class com.yuledkcs.BLEManager {
    public *;
}

-keep public class com.yuledkcs.CryptoWrapper {
    public *;
}

# Keep model classes
-keep public class com.yuledkcs.model.** {
    *;
}

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Retrofit
-keepattributes Signature
-keepattributes Exceptions
-keepattributes *Annotation*

-keep class retrofit2.** { *; }
-keepclassmembers,allowobfuscation interface * {
    @retrofit2.http.* <methods>;
}

# Gson
-keep class com.google.gson.** { *; }
-keep class com.google.gson.reflect.** { *; }

# BLE library
-keep class no.nordicsemi.android.ble.** { *; }

# Kotlin coroutines
-keepnames class kotlinx.coroutines.internal.MainDispatcherFactory {}
-keepnames class kotlinx.coroutines.CoroutineExceptionHandler {}
-keepnames class kotlinx.coroutines.android.AndroidExceptionPreHandler {}
-keepnames class kotlinx.coroutines.android.AndroidDispatcherFactory {}
