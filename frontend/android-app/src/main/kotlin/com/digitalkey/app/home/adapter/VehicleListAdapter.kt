/**
 * DigitalKey App - 车辆列表 Adapter
 */
package com.digitalkey.app.home.adapter

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.digitalkey.app.R
import com.digitalkey.app.data.model.VehicleModel
import com.digitalkey.app.databinding.ItemVehicleBinding

/**
 * 车辆列表 RecyclerView Adapter
 */
class VehicleListAdapter(
    private val onItemClick: (VehicleModel) -> Unit,
    private val onLockClick: (VehicleModel) -> Unit,
    private val onUnlockClick: (VehicleModel) -> Unit
) : ListAdapter<VehicleModel, VehicleListAdapter.VehicleViewHolder>(VehicleDiffCallback()) {

    private var selectedVehicleId: String? = null

    fun setSelectedVehicle(vehicleId: String) {
        val oldId = selectedVehicleId
        selectedVehicleId = vehicleId
        currentList.forEachIndexed { index, vehicle ->
            if (vehicle.id == oldId || vehicle.id == vehicleId) {
                notifyItemChanged(index)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VehicleViewHolder {
        val binding = ItemVehicleBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return VehicleViewHolder(binding)
    }

    override fun onBindViewHolder(holder: VehicleViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class VehicleViewHolder(
        private val binding: ItemVehicleBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(vehicle: VehicleModel) {
            binding.apply {
                // 车辆信息
                textVehicleName.text = "${vehicle.brand} ${vehicle.model}"
                textVehicleInfo.text = "${vehicle.year}款 | ${vehicle.color}"
                textVehiclePlate.text = vehicle.plate

                // 在线状态
                if (vehicle.isConnected) {
                    chipOnlineStatus.text = root.context.getString(R.string.online)
                    chipOnlineStatus.setChipBackgroundColorResource(R.color.status_active)
                    btnLock.isEnabled = true
                    btnUnlock.isEnabled = true
                } else {
                    chipOnlineStatus.text = root.context.getString(R.string.offline)
                    chipOnlineStatus.setChipBackgroundColorResource(R.color.status_inactive)
                    btnLock.isEnabled = false
                    btnUnlock.isEnabled = false
                }

                // 选中状态
                val isSelected = vehicle.id == selectedVehicleId
                root.strokeWidth = if (isSelected) 4 else 0
                root.strokeColor = ContextCompat.getColor(root.context, R.color.primary)

                // 点击事件
                root.setOnClickListener { onItemClick(vehicle) }
                btnLock.setOnClickListener { onLockClick(vehicle) }
                btnUnlock.setOnClickListener { onUnlockClick(vehicle) }
            }
        }
    }

    private class VehicleDiffCallback : DiffUtil.ItemCallback<VehicleModel>() {
        override fun areItemsTheSame(oldItem: VehicleModel, newItem: VehicleModel): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: VehicleModel, newItem: VehicleModel): Boolean {
            return oldItem == newItem
        }
    }
}
