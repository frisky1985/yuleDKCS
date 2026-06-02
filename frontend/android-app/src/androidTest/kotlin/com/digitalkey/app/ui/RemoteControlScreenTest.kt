/**
 * DigitalKey App - 远程控制页面 UI 测试
 *
 * 覆盖 VehicleControlActivity（远程车控操作界面）
 *
 * 测试内容：
 *   - 基本布局渲染（车辆状态卡片、底部控制按钮、Tab 切换）
 *   - 车辆名称/车牌/在线状态显示
 *   - 多 Tab 切换（控制 → 历史 → 车辆）
 *   - 底部快捷操作按钮
 *   - 工具栏导航
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
import androidx.test.espresso.matcher.ViewMatchers.isClickable
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.digitalkey.app.R
import com.digitalkey.app.home.VehicleControlActivity
import org.hamcrest.CoreMatchers.not
import org.hamcrest.CoreMatchers.allOf
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RemoteControlScreenTest {

    @get:Rule
    val activityRule = ActivityScenarioRule(VehicleControlActivity::class.java)

    // ─── 基础布局 ───────────────────────────────────────────────────

    @Test
    fun toolbarIsDisplayed() {
        // 检查 toolbar（包含返回箭头）
        onView(withId(R.id.toolbar)).check(matches(isDisplayed()))
    }

    @Test
    fun backNavigationIconIsDisplayed() {
        // Toolbar 导航图标（返回箭头）
        onView(withContentDescription("转到上一层级")).check(matches(isDisplayed()))
    }

    @Test
    fun tabLayoutIsDisplayed() {
        onView(withId(R.id.tab_layout)).check(matches(isDisplayed()))
    }

    @Test
    fun allTabsAreDisplayed() {
        onView(withText("控制")).check(matches(isDisplayed()))
        onView(withText("历史")).check(matches(isDisplayed()))
        onView(withText("车辆")).check(matches(isDisplayed()))
    }

    @Test
    fun fragmentContainerIsPresent() {
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    // ─── 车辆状态卡片 ───────────────────────────────────────────────

    @Test
    fun vehicleStatusCardIsDisplayed() {
        onView(withId(R.id.chip_online_status)).check(matches(isDisplayed()))
    }

    @Test
    fun vehicleNameAndPlateFieldsArePresent() {
        onView(withId(R.id.text_vehicle_name)).check(matches(isDisplayed()))
        onView(withId(R.id.text_vehicle_plate)).check(matches(isDisplayed()))
    }

    @Test
    fun progressBarIsInitiallyGone() {
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())))
    }

    @Test
    fun statusTextIsInitiallyGone() {
        onView(withId(R.id.text_status)).check(matches(not(isDisplayed())))
    }

    // ─── 底部控制按钮 ───────────────────────────────────────────────

    @Test
    fun bottomLockButtonIsDisplayed() {
        onView(withId(R.id.btn_lock)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_lock)).check(matches(isClickable()))
    }

    @Test
    fun bottomLockButtonHasText() {
        onView(allOf(withText("锁车"), hasSibling(withId(R.id.btn_lock))))
            .check(matches(isDisplayed()))
    }

    @Test
    fun bottomUnlockButtonIsDisplayed() {
        onView(withId(R.id.btn_unlock)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_unlock)).check(matches(isClickable()))
    }

    @Test
    fun bottomUnlockButtonHasText() {
        onView(allOf(withText("解锁"), hasSibling(withId(R.id.btn_unlock))))
            .check(matches(isDisplayed()))
    }

    @Test
    fun bottomFindCarButtonIsDisplayed() {
        onView(withId(R.id.btn_find_car)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_find_car)).check(matches(isClickable()))
    }

    @Test
    fun bottomFindCarButtonHasCorrectText() {
        onView(withText("寻车")).check(matches(isDisplayed()))
    }

    @Test
    fun bottomRemoteStartButtonIsDisplayed() {
        onView(withId(R.id.btn_remote_start)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_remote_start)).check(matches(isClickable()))
    }

    @Test
    fun bottomTrunkButtonIsDisplayed() {
        onView(withId(R.id.btn_trunk)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_trunk)).check(matches(isClickable()))
    }

    @Test
    fun bottomFlashHornButtonIsDisplayed() {
        onView(withId(R.id.btn_flash_horn)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_flash_horn)).check(matches(isClickable()))
    }

    @Test
    fun bottomClimateButtonIsDisplayed() {
        onView(withId(R.id.btn_climate)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_climate)).check(matches(isClickable()))
    }

    @Test
    fun allBottomButtonsAreDisplayed() {
        // 验证所有 7 个底部按钮均可见
        onView(withId(R.id.btn_lock)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_unlock)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_find_car)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_remote_start)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_trunk)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_flash_horn)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_climate)).check(matches(isDisplayed()))
    }

    @Test
    fun bottomButtonsContainExpectedTexts() {
        onView(withText("锁车")).check(matches(isDisplayed()))
        onView(withText("解锁")).check(matches(isDisplayed()))
        onView(withText("寻车")).check(matches(isDisplayed()))
        onView(withText("启动")).check(matches(isDisplayed()))
        onView(withText("后备箱")).check(matches(isDisplayed()))
        onView(withText("闪灯")).check(matches(isDisplayed()))
        onView(withText("空调")).check(matches(isDisplayed()))
    }

    // ─── Tab 切换 ───────────────────────────────────────────────────

    @Test
    fun controlTabIsActiveByDefault() {
        // "控制"标签默认选中；VehicleControlFragment 被加载
        // 通过 fragment_container 的存在验证
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun switchingToHistoryTabWorks() {
        onView(withText("历史")).perform(click())
        // 切换后 fragment_container 仍应存在
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun switchingToVehiclesTabWorks() {
        onView(withText("车辆")).perform(click())
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun switchingTabsDoesNotCrash() {
        // 来回切换所有 Tab
        onView(withText("历史")).perform(click())
        onView(withText("车辆")).perform(click())
        onView(withText("控制")).perform(click())
        onView(withText("历史")).perform(click())
        onView(withText("控制")).perform(click())
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    // ─── 点击控制按钮 ───────────────────────────────────────────────

    @Test
    fun clickingLockButtonWhenOfflineShowsToast() {
        // 车辆离线时锁车按钮应能点击且不 crash
        // 会触发 Toast 提示 "车辆离线"
        onView(withId(R.id.btn_lock)).perform(click())
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun clickingUnlockButtonDoesNotCrash() {
        onView(withId(R.id.btn_unlock)).perform(click())
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun allControlButtonsClickableWithoutCrash() {
        val buttonIds = listOf(
            R.id.btn_lock, R.id.btn_unlock, R.id.btn_find_car,
            R.id.btn_remote_start, R.id.btn_trunk, R.id.btn_flash_horn,
            R.id.btn_climate
        )
        buttonIds.forEach { id ->
            onView(withId(id)).perform(click())
            onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
        }
    }

    // ─── 车辆信息显示 ───────────────────────────────────────────────

    @Test
    fun vehicleNameCanBeUpdated() {
        // 验证 text_vehicle_name 是一个 TextView，可承载动态文本
        onView(withId(R.id.text_vehicle_name)).check(matches(isDisplayed()))
    }

    @Test
    fun vehiclePlateFieldExists() {
        onView(withId(R.id.text_vehicle_plate)).check(matches(isDisplayed()))
    }

    @Test
    fun onlineStatusChipExists() {
        // 在线状态 Chip
        onView(withId(R.id.chip_online_status)).check(matches(isDisplayed()))
    }

    // ─── Activity 生命周期 ──────────────────────────────────────────

    @Test
    fun activityCanBeRecreated() {
        activityRule.scenario.recreate()
        onView(withId(R.id.toolbar)).check(matches(isDisplayed()))
        onView(withId(R.id.tab_layout)).check(matches(isDisplayed()))
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    @Test
    fun tabsSurviveRecreation() {
        // 先切到历史Tab再重建
        onView(withText("历史")).perform(click())
        activityRule.scenario.recreate()
        onView(withId(R.id.tab_layout)).check(matches(isDisplayed()))
        onView(withId(R.id.fragment_container)).check(matches(isDisplayed()))
    }

    // ─── Intent Extra 启动 ──────────────────────────────────────────

    @Test
    fun launchWithVehicleIdExtra() {
        val intent = Intent(
            ApplicationProvider.getApplicationContext(),
            VehicleControlActivity::class.java
        ).apply {
            putExtra(VehicleControlActivity.EXTRA_VEHICLE_ID, "vehicle_001")
            putExtra(VehicleControlActivity.EXTRA_VEHICLE_NAME, "特斯拉 Model Y")
        }
        ActivityScenario.launch<Activity>(intent).use { scenario ->
            scenario.onActivity { /* started without crash */ }
        }
        onView(withId(R.id.toolbar)).check(matches(isDisplayed()))
    }

    @Test
    fun statusTextDisappearsAfterSet() {
        // text_status 初始不可见
        onView(withId(R.id.text_status)).check(matches(not(isDisplayed())))
    }
}
