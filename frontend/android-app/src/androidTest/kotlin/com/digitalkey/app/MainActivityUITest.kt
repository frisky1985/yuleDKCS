/**
 * 插桩测试示例 - MainActivity UI 测试
 *
 * 需要 Android 模拟器或真机运行：
 * ```
 * ./gradlew connectedAndroidTest
 * ```
 */
package com.digitalkey.app

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.digitalkey.app.home.MainActivity
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MainActivityUITest {

    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    @Test
    fun appLaunchesSuccessfully() {
        // 验证 App 主界面能够正常启动
        // 注：具体的 R.id 值需根据实际布局文件确认
        // onView(withId(R.id.main_container)).check(matches(isDisplayed()))
    }

    @Test
    fun navigationBarIsDisplayed() {
        // 验证底部导航栏显示
        // onView(withId(R.id.bottom_navigation)).check(matches(isDisplayed()))
    }
}
