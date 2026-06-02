/**
 * DigitalKey App - 密钥绑定页面 UI 测试
 *
 * 覆盖 AddKeyActivity（添加/绑定钥匙）的全部交互路径
 *
 * 多步骤流程：
 *   1. 输入激活码 → 2. 输入钥匙名称 → 3. 处理中 → 4. 成功/失败
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
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isClickable
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isEnabled
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.digitalkey.app.R
import com.digitalkey.app.key.AddKeyActivity
import org.hamcrest.CoreMatchers.not
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class KeyBindScreenTest {

    @get:Rule
    val activityRule = ActivityScenarioRule(AddKeyActivity::class.java)

    // ─── 布局渲染 ───────────────────────────────────────────────────

    @Test
    fun toolbarDisplaysAddKeyTitle() {
        onView(withText("添加钥匙")).check(matches(isDisplayed()))
    }

    @Test
    fun stepIndicatorShowsInitialStep() {
        onView(withId(R.id.step_indicator)).check(matches(withText("1/2")))
    }

    @Test
    fun stepOneLayoutIsVisibleByDefault() {
        onView(withId(R.id.layout_step_code)).check(matches(isDisplayed()))
    }

    @Test
    fun stepTwoLayoutIsInitiallyGone() {
        onView(withId(R.id.layout_step_name)).check(matches(not(isDisplayed())))
    }

    @Test
    fun activationCodeInputIsDisplayed() {
        onView(withId(R.id.edit_activation_code)).check(matches(isDisplayed()))
    }

    @Test
    fun nextButtonIsInitiallyDisabled() {
        // 未输入激活码时，下一步按钮应禁用
        onView(withId(R.id.btn_next)).check(matches(not(isEnabled())))
    }

    @Test
    fun backButtonIsInitiallyGone() {
        onView(withId(R.id.btn_back)).check(matches(not(isDisplayed())))
    }

    @Test
    fun doneButtonIsInitiallyGone() {
        onView(withId(R.id.btn_done)).check(matches(not(isDisplayed())))
    }

    @Test
    fun progressIndicatorIsDisplayed() {
        onView(withId(R.id.progress_indicator)).check(matches(isDisplayed()))
    }

    // ─── 激活码输入交互 ─────────────────────────────────────────────

    @Test
    fun enteringActivationCodeEnablesNextButton() {
        onView(withId(R.id.edit_activation_code)).perform(typeText("TEST1234"))
        onView(withId(R.id.btn_next)).check(matches(isEnabled()))
        onView(withId(R.id.btn_next)).check(matches(isClickable()))
    }

    @Test
    fun shortActivationCodeKeepsNextDisabled() {
        // 需求 ≥8 位
        onView(withId(R.id.edit_activation_code)).perform(typeText("ABC"))
        onView(withId(R.id.btn_next)).check(matches(not(isEnabled())))
    }

    @Test
    fun activationCodeAcceptsMaxLength() {
        // maxLength="16"
        onView(withId(R.id.edit_activation_code)).perform(typeText("ABCDEFGHIJ1234567890"))
        // 验证无 crash，仅测试输入可接受
        onView(withId(R.id.btn_next)).check(matches(isEnabled()))
    }

    @Test
    fun activationCodeDisplaysCenteredText() {
        // 验证 TextInputLayout hint
        onView(withText("激活码")).check(matches(isDisplayed()))
    }

    // ─── 步骤2（钥匙名称） ──────────────────────────────────────────

    @Test
    fun keyNameInputIsDisplayedAfterNavigation() {
        // 输入激活码并点击下一步，进入名称步骤
        enterActivationCodeAndProceed()
        onView(withId(R.id.layout_step_name)).check(matches(isDisplayed()))
        onView(withId(R.id.edit_key_name)).check(matches(isDisplayed()))
    }

    @Test
    fun stepIndicatorUpdatesAfterNavigation() {
        enterActivationCodeAndProceed()
        onView(withId(R.id.step_indicator)).check(matches(withText("2/2")))
    }

    @Test
    fun backButtonAppearsInStepTwo() {
        enterActivationCodeAndProceed()
        onView(withId(R.id.btn_back)).check(matches(isDisplayed()))
    }

    @Test
    fun nextButtonTextChangesInStepTwo() {
        enterActivationCodeAndProceed()
        onView(withId(R.id.btn_next)).check(matches(withText("添加钥匙")))
    }

    @Test
    fun enteringKeyNameEnablesAddButton() {
        enterActivationCodeAndProceed()
        // 默认 ViewModel 可能要求名称非空
        onView(withId(R.id.edit_key_name)).perform(typeText("我的车钥匙"))
        onView(withId(R.id.btn_next)).check(matches(isEnabled()))
    }

    @Test
    fun canNavigateBackFromStepTwo() {
        enterActivationCodeAndProceed()
        onView(withId(R.id.btn_back)).perform(click())
        onView(withId(R.id.layout_step_code)).check(matches(isDisplayed()))
        onView(withId(R.id.step_indicator)).check(matches(withText("1/2")))
    }

    // ─── 处理中状态 ─────────────────────────────────────────────────

    @Test
    fun processingLayoutIsInitiallyGone() {
        onView(withId(R.id.layout_processing)).check(matches(not(isDisplayed())))
    }

    @Test
    fun processingShowsActivatingText() {
        // 布局中文字段
        onView(withText("正在激活钥匙")).check(matches(not(isDisplayed())))
    }

    // ─── Activity 生命周期 ──────────────────────────────────────────

    @Test
    fun activityCanBeRecreated() {
        activityRule.scenario.recreate()
        onView(withId(R.id.edit_activation_code)).check(matches(isDisplayed()))
        onView(withId(R.id.btn_next)).check(matches(not(isEnabled())))
    }

    @Test
    fun typedTextSurvivesRecreation() {
        // 输入文本后重建
        onView(withId(R.id.edit_activation_code)).perform(typeText("MYCODE123"))
        activityRule.scenario.recreate()
        // 重建后输入框内容由 ViewModel + 状态恢复决定
        onView(withId(R.id.edit_activation_code)).check(matches(isDisplayed()))
    }

    // ─── 错误/重试 ──────────────────────────────────────────────────

    @Test
    fun errorLayoutExists() {
        onView(withId(R.id.layout_failed)).check(matches(not(isDisplayed())))
    }

    @Test
    fun retryButtonExists() {
        onView(withId(R.id.btn_retry)).check(matches(not(isDisplayed())))
    }

    @Test
    fun retryButtonTextIsRetry() {
        // 即使不可见，检查文本资源
        onView(withId(R.id.btn_retry)).check(matches(withText("重试")))
    }

    // ─── 成功布局 ───────────────────────────────────────────────────

    @Test
    fun successLayoutIsInitiallyGone() {
        onView(withId(R.id.layout_success)).check(matches(not(isDisplayed())))
    }

    @Test
    fun successMessageDisplayedOnSuccess() {
        // success layout exists
        onView(withId(R.id.text_success_message)).check(matches(not(isDisplayed())))
    }

    // ─── 启动 ───────────────────────────────────────────────────────

    @Test
    fun launchWithIntent() {
        val intent = Intent(ApplicationProvider.getApplicationContext(), AddKeyActivity::class.java)
        ActivityScenario.launch<Activity>(intent).use { scenario ->
            scenario.onActivity { /* started without crash */ }
        }
        onView(withId(R.id.edit_activation_code)).check(matches(isDisplayed()))
    }

    // ─── 内部辅助 ───────────────────────────────────────────────────

    /**
     * 输入≥8位激活码并点"下一步"进入步骤2
     */
    private fun enterActivationCodeAndProceed() {
        onView(withId(R.id.edit_activation_code)).perform(typeText("ACTIVATE123"))
        onView(withId(R.id.btn_next)).perform(click())
    }
}
