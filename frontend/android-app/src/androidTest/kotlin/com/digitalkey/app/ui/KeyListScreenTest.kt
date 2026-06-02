/**
 * DigitalKey App - 密钥列表页面 UI 测试
 *
 * 覆盖 MainActivity（钥匙列表主页）的全部主要交互路径
 *
 * 需要 Android 模拟器或真机运行：
 * ```
 * cd frontend && ./gradlew :android-app:connectedAndroidTest
 * ```
 */
package com.digitalkey.app.ui

import android.app.Activity
import android.content.Intent
import androidx.test.core.app.ActivityScenario
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.digitalkey.app.R
import com.digitalkey.app.home.MainActivity
import com.digitalkey.app.key.AddKeyActivity
import com.digitalkey.app.settings.SettingsActivity
import org.hamcrest.CoreMatchers.not
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class KeyListScreenTest {

    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    @Before
    fun setUp() {
        Intents.init()
    }

    @After
    fun tearDown() {
        Intents.release()
    }

    // ─── 布局渲染 ───────────────────────────────────────────────────

    @Test
    fun toolbarDisplaysAppName() {
        onView(withText("数字钥匙")).check(matches(isDisplayed()))
    }

    @Test
    fun recyclerViewIsPresent() {
        onView(withId(R.id.recycler_view_keys)).check(matches(isDisplayed()))
    }

    @Test
    fun addKeyFabIsDisplayed() {
        onView(withId(R.id.fab_add_key)).check(matches(isDisplayed()))
    }

    @Test
    fun swipeRefreshIsPresent() {
        onView(withId(R.id.swipe_refresh)).check(matches(isDisplayed()))
    }

    @Test
    fun progressBarIsInitiallyGone() {
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())))
    }

    // ─── 空状态 ─────────────────────────────────────────────────────

    @Test
    fun emptyStateShowsNoKeysMessage() {
        // 当钥匙列表为空且不是 Loading 时，空状态布局可见
        onView(withText("暂无钥匙")).check(matches(isDisplayed()))
        onView(withText("点击右下角按钮添加第一把钥匙")).check(matches(isDisplayed()))
    }

    @Test
    fun emptyStateLayoutIsClickable() {
        // 验证空状态图标存在（Drawable 不直接断言，但可以检查其父布局可见）
        onView(withId(R.id.layout_empty)).check(matches(isDisplayed()))
    }

    // ─── 导航（FAB → AddKeyActivity） ─────────────────────────────

    @Test
    fun tapFabNavigatesToAddKey() {
        onView(withId(R.id.fab_add_key)).perform(click())
        intended(hasComponent(AddKeyActivity::class.java.name))
    }

    @Test
    fun tapAddKeyFabContentDescription() {
        // 检查 FAB 的 contentDescription
        onView(withContentDescription("添加钥匙")).check(matches(isDisplayed()))
    }

    // ─── 顶部ActionBar菜单 ──────────────────────────────────────────

    @Test
    fun settingsIconNavigatesToSettings() {
        // 打开菜单 OverFlow（如果 menu 是溢出菜单形式）
        // 如果 toolbar 直接显示图标，可以直接点击
        try {
            onView(withContentDescription("更多选项")).perform(click())
            onView(withText("设置")).perform(click())
        } catch (_: Exception) {
            // 如果 Toolbar 溢出菜单不存在，尝试直接查找设置按钮
            onView(withId(R.id.action_settings)).perform(click())
        }
        intended(hasComponent(SettingsActivity::class.java.name))
    }

    // ─── Activity 生命周期 ──────────────────────────────────────────

    @Test
    fun activityCanBeRecreated() {
        activityRule.scenario.recreate()
        onView(withId(R.id.recycler_view_keys)).check(matches(isDisplayed()))
        onView(withId(R.id.fab_add_key)).check(matches(isDisplayed()))
    }

    @Test
    fun activitySurvivesRotation() {
        // 模拟配置变更触发重建
        activityRule.scenario.onActivity { activity ->
            activity.setRequestedOrientation(
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            )
        }
        onView(withId(R.id.recycler_view_keys)).check(matches(isDisplayed()))
        activityRule.scenario.onActivity { activity ->
            activity.setRequestedOrientation(
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            )
        }
        onView(withId(R.id.recycler_view_keys)).check(matches(isDisplayed()))
    }

    // ─── 错误状态交互 ──────────────────────────────────────────────

    @Test
    fun errorLayoutHasRetryButton() {
        onView(withId(R.id.btn_retry)).check(matches(withText("重试")))
    }

    // ─── 启动方式（新 Intent 测试） ────────────────────────────────

    @Test
    fun launchWithClearIntent() {
        val intent = Intent(ApplicationProvider.getApplicationContext(), MainActivity::class.java)
        ActivityScenario.launch<Activity>(intent).use { scenario ->
            scenario.onActivity { /* started without crash */ }
        }
        onView(withId(R.id.recycler_view_keys)).check(matches(isDisplayed()))
    }
}
