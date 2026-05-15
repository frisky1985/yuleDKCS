/**
 * DigitalKey App - 钥匙列表 Adapter
 */
package com.digitalkey.app.home.adapter

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.digitalkey.app.R
import com.digitalkey.app.data.model.KeyModel
import com.digitalkey.app.data.model.KeyStatus
import com.digitalkey.app.databinding.ItemKeyListBinding

/**
 * 钥匙列表 RecyclerView Adapter
 */
class KeyListAdapter(
    private val onItemClick: (KeyModel) -> Unit,
    private val onShareClick: (KeyModel) -> Unit,
    private val onDeleteClick: (KeyModel) -> Unit
) : ListAdapter<KeyModel, KeyListAdapter.KeyViewHolder>(KeyDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): KeyViewHolder {
        val binding = ItemKeyListBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return KeyViewHolder(binding)
    }

    override fun onBindViewHolder(holder: KeyViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class KeyViewHolder(
        private val binding: ItemKeyListBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(key: KeyModel) {
            binding.apply {
                textKeyName.text = key.name
                textVehicleName.text = key.vehicleName
                textVehiclePlate.text = key.vehiclePlate
                textIssuer.text = root.context.getString(R.string.issued_by, key.issuerName)

                // 状态芯片
                chipStatus.text = when (key.status) {
                    KeyStatus.ACTIVE -> root.context.getString(R.string.status_active)
                    KeyStatus.INACTIVE -> root.context.getString(R.string.status_inactive)
                    KeyStatus.EXPIRED -> root.context.getString(R.string.status_expired)
                    KeyStatus.PENDING -> root.context.getString(R.string.status_pending)
                    KeyStatus.REVOKED -> root.context.getString(R.string.status_revoked)
                }
                chipStatus.setChipBackgroundColorResource(
                    when (key.status) {
                        KeyStatus.ACTIVE -> R.color.status_active
                        else -> R.color.status_inactive
                    }
                )

                // 权限数量
                val permCount = key.permissions.count { it.granted }
                textPermissionCount.text = root.context.getString(R.string.permission_count, permCount)

                // 点击事件
                root.setOnClickListener { onItemClick(key) }
                btnShare.setOnClickListener { onShareClick(key) }
                btnDelete.setOnClickListener { onDeleteClick(key) }
            }
        }
    }

    private class KeyDiffCallback : DiffUtil.ItemCallback<KeyModel>() {
        override fun areItemsTheSame(oldItem: KeyModel, newItem: KeyModel): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: KeyModel, newItem: KeyModel): Boolean {
            return oldItem == newItem
        }
    }
}
