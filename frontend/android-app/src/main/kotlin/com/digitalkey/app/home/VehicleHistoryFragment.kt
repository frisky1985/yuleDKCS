/**
 * DigitalKey App - 操作历史 Fragment
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
import com.digitalkey.app.data.model.ControlStatus
import com.digitalkey.app.data.model.HistoryRecord
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.databinding.FragmentHistoryBinding
import com.digitalkey.app.home.adapter.HistoryAdapter
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 操作历史 Fragment
 * 
 * 功能：
 * - 显示所有车控操作历史
 * - 显示操作时间、类型、结果
 * - 支持按钥匙筛选
 * - 下拉刷新
 */
@AndroidEntryPoint
class VehicleHistoryFragment : Fragment() {

    private var _binding: FragmentHistoryBinding? = null
    private val binding get() = _binding!!

    private val viewModel: VehicleControlViewModel by activityViewModels()
    private lateinit var historyAdapter: HistoryAdapter

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentHistoryBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupRecyclerView()
        observeState()
        viewModel.loadHistory()
    }

    private fun setupRecyclerView() {
        historyAdapter = HistoryAdapter()
        binding.recyclerViewHistory.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = historyAdapter
        }

        binding.swipeRefresh.setOnRefreshListener {
            viewModel.loadHistory()
        }
        binding.swipeRefresh.setColorSchemeResources(R.color.primary)
    }

    private fun observeState() {
        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.historyState.collectLatest { state ->
                binding.swipeRefresh.isRefreshing = false
                when (state) {
                    is UiState.Loading -> showLoading()
                    is UiState.Success -> showHistory(state.data)
                    is UiState.Empty -> showEmpty()
                    is UiState.Error -> showError(state.message)
                }
            }
        }
    }

    private fun showLoading() {
        binding.progressBar.visibility = View.VISIBLE
        binding.recyclerViewHistory.visibility = View.GONE
        binding.layoutEmpty.visibility = View.GONE
    }

    private fun showHistory(records: List<HistoryRecord>) {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewHistory.visibility = View.VISIBLE
        binding.layoutEmpty.visibility = View.GONE
        historyAdapter.submitList(records)
    }

    private fun showEmpty() {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewHistory.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
        binding.textEmptyMessage.text = getString(R.string.no_history)
    }

    private fun showError(message: String) {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewHistory.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
        binding.textEmptyMessage.text = message
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
