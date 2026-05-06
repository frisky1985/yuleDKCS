/**
 * DigitalKey App - 首页 Fragment
 * 显示车辆列表，支持快速操作
 */
package com.digitalkey.app.home

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.model.VehicleModel
import com.digitalkey.app.databinding.FragmentHomeBinding
import com.digitalkey.app.home.adapter.VehicleListAdapter
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 首页 Fragment
 * 
 * 功能：
 * - 显示车辆卡片列表
 * - 显示在线状态
 * - 支持快速锁车/解锁
 * - 点击进入详细车控
 */
@AndroidEntryPoint
class HomeFragment : Fragment() {

    private var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!

    private val viewModel: VehicleControlViewModel by activityViewModels()
    private val vehicleAdapter = VehicleListAdapter(
        onItemClick = { vehicle -> onVehicleSelected(vehicle) },
        onLockClick = { vehicle -> viewModel.lockVehicle() },
        onUnlockClick = { vehicle -> viewModel.unlockVehicle() }
    )

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentHomeBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupRecyclerView()
        observeState()
    }

    private fun setupRecyclerView() {
        binding.recyclerViewVehicles.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = vehicleAdapter
        }
    }

    private fun observeState() {
        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.vehicleListState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> showLoading()
                    is UiState.Success -> showVehicles(state.data)
                    is UiState.Empty -> showEmpty()
                    is UiState.Error -> showError(state.message)
                }
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.selectedVehicle.collectLatest { vehicle ->
                vehicle?.let { updateSelectedVehicle(it) }
            }
        }
    }

    private fun showLoading() {
        binding.progressBar.visibility = View.VISIBLE
        binding.recyclerViewVehicles.visibility = View.GONE
        binding.layoutEmpty.visibility = View.GONE
    }

    private fun showVehicles(vehicles: List<VehicleModel>) {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewVehicles.visibility = View.VISIBLE
        binding.layoutEmpty.visibility = View.GONE
        vehicleAdapter.submitList(vehicles)
    }

    private fun showEmpty() {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewVehicles.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
    }

    private fun showError(message: String) {
        binding.progressBar.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
        binding.textEmptyMessage.text = message
    }

    private fun updateSelectedVehicle(vehicle: VehicleModel) {
        vehicleAdapter.setSelectedVehicle(vehicle.id)
    }

    private fun onVehicleSelected(vehicle: VehicleModel) {
        viewModel.selectVehicle(vehicle)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
