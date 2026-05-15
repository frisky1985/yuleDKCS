/**
 * DigitalKey App - 设置页面 Activity
 */
package com.digitalkey.app.settings

import android.content.Intent
import android.os.Bundle
import android.view.MenuItem
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import com.digitalkey.app.BuildConfig
import com.digitalkey.app.databinding.ActivitySettingsBinding
import com.digitalkey.app.home.MainActivity
import dagger.hilt.android.AndroidEntryPoint

/**
 * 设置页面 Activity
 * 
 * 功能：
 * - 深色模式切换
 * - 推送通知开关
 * - 生物识别开关
 * - 清除缓存
 * - 关于信息
 * - 版本号显示
 * - 退出登录
 */
@AndroidEntryPoint
class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupViews()
        loadSettings()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowHomeEnabled(true)
            title = getString(R.string.settings)
        }
    }

    private fun setupViews() {
        // 深色模式切换
        binding.switchDarkMode.setOnCheckedChangeListener { _, isChecked ->
            val mode = if (isChecked) {
                AppCompatDelegate.MODE_NIGHT_YES
            } else {
                AppCompatDelegate.MODE_NIGHT_NO
            }
            AppCompatDelegate.setDefaultNightMode(mode)
            savePreference("dark_mode", isChecked)
        }

        // 推送通知
        binding.switchNotifications.setOnCheckedChangeListener { _, isChecked ->
            savePreference("notifications", isChecked)
        }

        // 生物识别
        binding.switchBiometric.setOnCheckedChangeListener { _, isChecked ->
            savePreference("biometric", isChecked)
        }

        // 清除缓存
        binding.btnClearCache.setOnClickListener {
            clearCache()
        }

        // 关于我们
        binding.layoutAbout.setOnClickListener {
            showAboutDialog()
        }

        // 用户协议
        binding.layoutPrivacy.setOnClickListener {
            // 打开隐私政策页面
        }

        // 用户服务协议
        binding.layoutTerms.setOnClickListener {
            // 打开用户协议页面
        }

        // 退出登录
        binding.btnLogout.setOnClickListener {
            showLogoutConfirm()
        }

        // 版本号
        binding.textVersion.text = getString(
            R.string.version_format,
            BuildConfig.VERSION_NAME,
            BuildConfig.VERSION_CODE
        )
    }

    private fun loadSettings() {
        val prefs = getSharedPreferences("settings", MODE_PRIVATE)

        // 加载深色模式
        binding.switchDarkMode.isChecked = prefs.getBoolean("dark_mode", false)

        // 加载通知设置
        binding.switchNotifications.isChecked = prefs.getBoolean("notifications", true)

        // 加载生物识别设置
        binding.switchBiometric.isChecked = prefs.getBoolean("biometric", false)
    }

    private fun savePreference(key: String, value: Boolean) {
        getSharedPreferences("settings", MODE_PRIVATE)
            .edit()
            .putBoolean(key, value)
            .apply()
    }

    private fun clearCache() {
        try {
            cacheDir.deleteRecursively()
            android.widget.Toast.makeText(
                this,
                getString(com.digitalkey.app.R.string.cache_cleared),
                android.widget.Toast.LENGTH_SHORT
            ).show()
        } catch (e: Exception) {
            android.widget.Toast.makeText(
                this,
                getString(com.digitalkey.app.R.string.cache_clear_failed),
                android.widget.Toast.LENGTH_SHORT
            ).show()
        }
    }

    private fun showAboutDialog() {
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.about_us)
            .setMessage(getString(R.string.about_content))
            .setPositiveButton(R.string.ok, null)
            .show()
    }

    private fun showLogoutConfirm() {
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.logout)
            .setMessage(R.string.logout_confirm)
            .setPositiveButton(R.string.logout) { _, _ ->
                performLogout()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun performLogout() {
        // 清除登录状态
        getSharedPreferences("auth", MODE_PRIVATE).edit().clear().apply()

        // 跳转到登录页面
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        }
        startActivity(intent)
        finish()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            android.R.id.home -> {
                onBackPressedDispatcher.onBackPressed()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }
}
