/**
 * DigitalKey App - 车控操作 Fragment
 */
package com.digitalkey.app.home

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import com.digitalkey.app.R
import com.digitalkey.app.data.model.ControlStatus
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.model.VehicleCommand
import com.digitalkey.app.data.model.VehicleModel
import com.digitalkey.app.databinding.FragmentVehicleControlBinding
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 车控操作 Fragment
 * 
 * 功能：
 * - 显示当前选中的车辆信息
 * - 显示车辆在线状态
 * - 快捷操作按钮：锁车、解锁、寻车、远程启动、后备箱
 * - 显示命令执行结果
 */
@AndroidEntryPoint
class VehicleControlFragment : Fragment() {

    private var _binding: FragmentVehicleControlBinding? = null
    private val binding get() = _binding!!

    private val viewModel: VehicleControlViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentVehicleControlBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupControlButtons()
        observeState()
    }

    private fun setupControlButtons() {
        binding.btnLock.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.LOCK)
        }
        binding.btnUnlock.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.UNLOCK)
        }
        binding.btnFindCar.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.FIND_CAR)
        }
        binding.btnRemoteStart.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.REMOTE_START)
        }
        binding.btnTrunk.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.OPEN_TRUNK)
        }
        binding.btnFlashLights.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.FLASH_LIGHTS)
        }
        binding.btnClimate.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.START_CLIMATE)
        }
        binding.btnWindows.setOnClickListener {
            if (checkVehicleOnline()) sendCommand(VehicleCommand.CLOSE_WINDOWS)
        }
    }

    private fun checkVehicleOnline(): Boolean {
        if (!viewModel.isVehicleOnline()) {
            Toast.makeText(requireContext(), R.string.vehicle_offline, Toast.LENGTH_SHORT).show()
            return false
        }
        return true
    }

    private fun sendCommand(command: VehicleCommand) {
        viewModel.sendCommand(command)
    }

    private fun observeState() {
        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.selectedVehicle.collectLatest { vehicle ->
                vehicle?.let { updateVehicleUI(it) }
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.commandState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> showLoading()
                    is UiState.Success -> showResult(state.data.status, state.data.message)
                    is UiState.Error -> showResult(ControlStatus.FAILED, state.message)
                    else -> hideLoading()
                }
            }
        }
    }

    private fun updateVehicleUI(vehicle: VehicleModel) {
        binding.textVehicleName.text = vehicle.vehicleName
        binding.textVehiclePlate.text = vehicle.vehiclePlate
        binding.textVehicleInfo.text = "${vehicle.brand} ${vehicle.model} ${vehicle.year}款"

        if (vehicle.isConnected) {
            binding.cardStatus.setCardBackgroundColor(
                requireContext().getColor(R.color.status_active_bg)
            )
            binding.textOnlineStatus.text = getString(R.string.online)
            binding.textOnlineStatus.setTextColor(requireContext().getColor(R.color.status_active))
            enableControls(true)
        } else {
            binding.cardStatus.setCardBackgroundColor(
                requireContext().getColor(R.color.status_inactive_bg)
            )
            binding.textOnlineStatus.text = getString(R.string.offline)
            binding.textOnlineStatus.setTextColor(requireContext().getColor(R.color.status_inactive))
            enableControls(false)
        }
    }

    private fun enableControls(enabled: Boolean) {
        binding.btnLock.isEnabled = enabled
        binding.btnUnlock.isEnabled = enabled
        binding.btnFindCar.isEnabled = enabled
        binding.btnRemoteStart.isEnabled = enabled
        binding.btnTrunk.isEnabled = enabled
        binding.btnFlashLights.isEnabled = enabled
        binding.btnClimate.isEnabled = enabled
        binding.btnWindows.isEnabled = enabled
    }

    private fun showLoading() {
        binding.progressBar.visibility = View.VISIBLE
        binding.textResult.visibility = View.GONE
    }

    private fun hideLoading() {
        binding.progressBar.visibility = View.GONE
    }

    private fun showResult(status: ControlStatus, message: String) {
        binding.progressBar.visibility = View.GONE
        binding.textResult.visibility = View.VISIBLE
        binding.textResult.text = message

        val colorRes = if (status == ControlStatus.SUCCESS) {
            R.color.status_active
        } else {
            R.color.status_inactive
        }
        binding.textResult.setTextColor(requireContext().getColor(colorRes))

        // 3秒后自动隐藏
        binding.textResult.postDelayed({
            if (_binding != null) {
                binding.textResult.visibility = View.GONE
            }
        }, 3000)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
