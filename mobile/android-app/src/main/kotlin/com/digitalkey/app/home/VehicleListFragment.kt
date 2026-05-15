/**
 * DigitalKey App - 车辆列表 Fragment
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
import com.digitalkey.app.R
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.data.model.VehicleModel
import com.digitalkey.app.databinding.FragmentVehicleListBinding
import com.digitalkey.app.home.adapter.VehicleListAdapter
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 车辆列表 Fragment
 * 
 * 功能：
 * - 显示所有车辆列表
 * - 显示在线/离线状态
 * - 显示最后在线时间
 * - 点击切换当前车辆
 */
@AndroidEntryPoint
class VehicleListFragment : Fragment() {

    private var _binding: FragmentVehicleListBinding? = null
    private val binding get() = _binding!!

    private val viewModel: VehicleControlViewModel by activityViewModels()
    private val vehicleAdapter = VehicleListAdapter(
        onItemClick = { vehicle -> selectVehicle(vehicle) },
        onLockClick = { vehicle -> viewModel.lockVehicle() },
        onUnlockClick = { vehicle -> viewModel.unlockVehicle() }
    )

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentVehicleListBinding.inflate(inflater, container, false)
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

        binding.swipeRefresh.setOnRefreshListener {
            viewModel.loadVehicles()
        }
        binding.swipeRefresh.setColorSchemeResources(R.color.primary)
    }

    private fun observeState() {
        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.vehicleListState.collectLatest { state ->
                binding.swipeRefresh.isRefreshing = false
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
                vehicle?.let { vehicleAdapter.setSelectedVehicle(it.id) }
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
        binding.recyclerViewVehicles.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
        binding.textEmptyMessage.text = message
    }

    private fun selectVehicle(vehicle: VehicleModel) {
        viewModel.selectVehicle(vehicle)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
