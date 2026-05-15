/**
 * DigitalKey App - Application入口
 */
package com.digitalkey.app

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

/**
 * Application类
 * 使用Hilt进行依赖注入
 */
@HiltAndroidApp
class DigitalKeyApp : Application() {

    override fun onCreate() {
        super.onCreate()
        // 全局初始化工作可在此处进行
    }
}
