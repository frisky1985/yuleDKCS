/**
 * DigitalKey App - 主界面 Activity
 * 列出所有钥匙，支持跳转详情、添加、分享
 */
package com.digitalkey.app.home

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import com.digitalkey.app.R
import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.UiState
import com.digitalkey.app.databinding.ActivityMainBinding
import com.digitalkey.app.home.adapter.KeyListAdapter
import com.digitalkey.app.key.AddKeyActivity
import com.digitalkey.app.key.KeyDetailActivity
import com.digitalkey.app.key.KeyListViewModel
import com.digitalkey.app.key.ShareKeyActivity
import com.digitalkey.app.settings.SettingsActivity
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * 主界面 Activity
 * 
 * 功能：
 * - 显示钥匙列表 (RecyclerView)
 * - 下拉刷新
 * - 点击钥匙进入详情
 * - FAB 添加钥匙
 * - 右上角设置
 */
@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: KeyListViewModel by viewModels()

    private val keyListAdapter = KeyListAdapter(
        onItemClick = { key -> navigateToKeyDetail(key) },
        onShareClick = { key -> navigateToShareKey(key) },
        onDeleteClick = { key -> showDeleteConfirm(key) }
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupToolbar()
        setupRecyclerView()
        setupSwipeRefresh()
        setupFab()
        observeState()
    }

    override fun onResume() {
        super.onResume()
        viewModel.refreshKeys()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbar)
        binding.toolbar.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.action_settings -> {
                    startActivity(Intent(this, SettingsActivity::class.java))
                    true
                }
                R.id.action_refresh -> {
                    viewModel.refreshKeys()
                    true
                }
                else -> false
            }
        }
    }

    private fun setupRecyclerView() {
        binding.recyclerViewKeys.apply {
            layoutManager = LinearLayoutManager(this@MainActivity)
            adapter = keyListAdapter
            setHasFixedSize(true)
        }
    }

    private fun setupSwipeRefresh() {
        binding.swipeRefresh.setOnRefreshListener {
            viewModel.refreshKeys()
        }
        binding.swipeRefresh.setColorSchemeResources(
            R.color.primary,
            R.color.secondary,
            R.color.accent
        )
    }

    private fun setupFab() {
        binding.fabAddKey.setOnClickListener {
            startActivity(Intent(this, AddKeyActivity::class.java))
        }
    }

    private fun observeState() {
        lifecycleScope.launch {
            viewModel.keyListState.collectLatest { state ->
                when (state) {
                    is UiState.Loading -> showLoading()
                    is UiState.Success -> showKeyList(state.data)
                    is UiState.Error -> showError(state.message)
                    is UiState.Empty -> showEmpty()
                }
            }
        }

        lifecycleScope.launch {
            viewModel.isRefreshing.collectLatest { isRefreshing ->
                binding.swipeRefresh.isRefreshing = isRefreshing
            }
        }
    }

    private fun showLoading() {
        binding.progressBar.visibility = View.VISIBLE
        binding.recyclerViewKeys.visibility = View.GONE
        binding.layoutEmpty.visibility = View.GONE
        binding.layoutError.visibility = View.GONE
    }

    private fun showKeyList(keys: List<KeyModel>) {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewKeys.visibility = View.VISIBLE
        binding.layoutEmpty.visibility = View.GONE
        binding.layoutError.visibility = View.GONE
        keyListAdapter.submitList(keys)
    }

    private fun showEmpty() {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewKeys.visibility = View.GONE
        binding.layoutEmpty.visibility = View.VISIBLE
        binding.layoutError.visibility = View.GONE
    }

    private fun showError(message: String) {
        binding.progressBar.visibility = View.GONE
        binding.recyclerViewKeys.visibility = View.GONE
        binding.layoutEmpty.visibility = View.GONE
        binding.layoutError.visibility = View.VISIBLE
        binding.textErrorMessage.text = message
        binding.btnRetry.setOnClickListener { viewModel.loadKeys() }
    }

    private fun navigateToKeyDetail(key: KeyModel) {
        val intent = Intent(this, KeyDetailActivity::class.java).apply {
            putExtra(KeyDetailActivity.EXTRA_KEY_ID, key.id)
            putExtra(KeyDetailActivity.EXTRA_KEY_NAME, key.name)
        }
        startActivity(intent)
    }

    private fun navigateToShareKey(key: KeyModel) {
        val intent = Intent(this, ShareKeyActivity::class.java).apply {
            putExtra(ShareKeyActivity.EXTRA_KEY_ID, key.id)
            putExtra(ShareKeyActivity.EXTRA_KEY_NAME, key.name)
        }
        startActivity(intent)
    }

    private fun showDeleteConfirm(key: KeyModel) {
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.delete_key_title)
            .setMessage(getString(R.string.delete_key_message, key.name))
            .setPositiveButton(R.string.delete) { _, _ ->
                viewModel.deleteKey(key.id) { success ->
                    Toast.makeText(
                        this,
                        if (success) R.string.delete_key_success else R.string.delete_key_failed,
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }
}
