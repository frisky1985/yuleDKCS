/**
 * DigitalKey App - 车控操作 Activity
 * 集成 VehicleListFragment + VehicleControlFragment + HistoryFragment
 */
package com.digitalkey.app.home

import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.digitalkey.app.R
import com.digitalkey.app.data.model.ControlStatus
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.model.VehicleCommand
import com.digitalkey.app.data.model.VehicleModel
import com.digitalkey.app.databinding.ActivityVehicleControlBinding
import com.google.android.material.tabs.TabLayout
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 车控操作 Activity
 * 
 * 使用 TabLayout 集成三个功能页面：
 * - 车辆列表
 * - 车控操作
 * - 操作历史
 */
@AndroidEntryPoint
class VehicleControlActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_VEHICLE_ID = "extra_vehicle_id"
        const val EXTRA_VEHICLE_NAME = "extra_vehicle_name"
    }

    private lateinit var binding: ActivityVehicleControlBinding
    private val viewModel: VehicleControlViewModel by viewModels()

    private var vehicleId: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityVehicleControlBinding.inflate(layoutInflater)
        setContentView(binding.root)

        vehicleId = intent.getStringExtra(EXTRA_VEHICLE_ID)

        setupToolbar()
        setupTabs()
        setupControlButtons()
        observeState()

        // 加载车辆数据
        viewModel.loadVehicles()

        // 默认显示车控页面
        if (savedInstanceState == null) {
            showFragment(VehicleControlFragment())
        }
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        supportActionBar?.apply {
            setDisplayHomeAsUpEnabled(true)
            setDisplayShowHomeEnabled(true)
            title = intent.getStringExtra(EXTRA_VEHICLE_NAME) ?: getString(R.string.vehicle_control)
        }
    }

    private fun setupTabs() {
        binding.tabLayout.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab?) {
                when (tab?.position) {
                    0 -> showFragment(VehicleControlFragment())
                    1 -> showFragment(VehicleHistoryFragment())
                    2 -> showFragment(VehicleListFragment())
                }
            }

            override fun onTabUnselected(tab: TabLayout.Tab?) {}
            override fun onTabReselected(tab: TabLayout.Tab?) {}
        })
    }

    private fun showFragment(fragment: Fragment) {
        supportFragmentManager.beginTransaction()
            .replace(R.id.fragment_container, fragment)
            .commit()
    }

    private fun setupControlButtons() {
        // 锁车
        binding.btnLock.setOnClickListener {
            sendCommand(VehicleCommand.LOCK, getString(R.string.locking))
        }

        // 解锁
        binding.btnUnlock.setOnClickListener {
            sendCommand(VehicleCommand.UNLOCK, getString(R.string.unlocking))
        }

        // 寻车
        binding.btnFindCar.setOnClickListener {
            sendCommand(VehicleCommand.FIND_CAR, getString(R.string.finding_car))
        }

        // 远程启动
        binding.btnRemoteStart.setOnClickListener {
            sendCommand(VehicleCommand.REMOTE_START, getString(R.string.remote_starting))
        }

        // 后备箱
        binding.btnTrunk.setOnClickListener {
            sendCommand(VehicleCommand.OPEN_TRUNK, getString(R.string.opening_trunk))
        }

        // 闪灯鸣笛
        binding.btnFlashHorn.setOnClickListener {
            sendCommand(VehicleCommand.FLASH_LIGHTS, getString(R.string.flashing))
        }

        // 空调
        binding.btnClimate.setOnClickListener {
            sendCommand(VehicleCommand.START_CLIMATE, getString(R.string.climate_on))
        }
    }

    private fun sendCommand(command: VehicleCommand, loadingMsg: String) {
        if (!viewModel.isVehicleOnline()) {
            Toast.makeText(this, R.string.vehicle_offline, Toast.LENGTH_SHORT).show()
            return
        }

        binding.textStatus.text = loadingMsg
        binding.textStatus.visibility = View.VISIBLE
        viewModel.sendCommand(command)
    }

    private fun observeState() {
        lifecycleScope.launch {
            viewModel.selectedVehicle.collectLatest { vehicle ->
                vehicle?.let {
                    updateVehicleStatus(it)
                }
            }
        }

        lifecycleScope.launch {
            viewModel.commandState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> {
                        binding.progressBar.visibility = View.VISIBLE
                    }
                    is UiState.Success -> {
                        binding.progressBar.visibility = View.GONE
                        showCommandResult(state.data.status, state.data.message)
                    }
                    is UiState.Error -> {
                        binding.progressBar.visibility = View.GONE
                        showCommandResult(ControlStatus.FAILED, state.message)
                    }
                    else -> {
                        binding.progressBar.visibility = View.GONE
                    }
                }
            }
        }
    }

    private fun updateVehicleStatus(vehicle: VehicleModel) {
        binding.textVehicleName.text = vehicle.vehicleName
        binding.textVehiclePlate.text = vehicle.vehiclePlate

        if (vehicle.isConnected) {
            binding.chipOnlineStatus.text = getString(R.string.online)
            binding.chipOnlineStatus.setChipBackgroundColorResource(R.color.status_active)
        } else {
            binding.chipOnlineStatus.text = getString(R.string.offline)
            binding.chipOnlineStatus.setChipBackgroundColorResource(R.color.status_inactive)
        }
    }

    private fun showCommandResult(status: ControlStatus, message: String) {
        binding.textStatus.text = message
        binding.textStatus.setTextColor(
            getColor(
                if (status == ControlStatus.SUCCESS) R.color.status_active
                else R.color.status_inactive
            )
        )

        // 3秒后隐藏状态
        binding.textStatus.postDelayed({
            binding.textStatus.visibility = View.GONE
        }, 3000)
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
