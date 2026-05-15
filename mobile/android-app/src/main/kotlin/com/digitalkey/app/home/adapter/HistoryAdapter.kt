/**
 * DigitalKey App - 操作历史 Adapter
 */
package com.digitalkey.app.home.adapter

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.digitalkey.app.R
import com.digitalkey.app.data.model.ControlStatus
import com.digitalkey.app.data.model.HistoryRecord
import com.digitalkey.app.data.model.VehicleCommand
import com.digitalkey.app.databinding.ItemHistoryBinding
import java.text.SimpleDateFormat
import java.util.*

/**
 * 操作历史 RecyclerView Adapter
 */
class HistoryAdapter : ListAdapter<HistoryRecord, HistoryAdapter.HistoryViewHolder>(HistoryDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HistoryViewHolder {
        val binding = ItemHistoryBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return HistoryViewHolder(binding)
    }

    override fun onBindViewHolder(holder: HistoryViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class HistoryViewHolder(
        private val binding: ItemHistoryBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(record: HistoryRecord) {
            binding.apply {
                // 操作图标和名称
                val (iconRes, nameRes) = getCommandIconAndName(record.command)
                imgCommandIcon.setImageResource(iconRes)
                textCommandName.text = root.context.getString(nameRes)

                // 车辆信息
                textVehicleName.text = record.vehicleName

                // 状态
                when (record.status) {
                    ControlStatus.SUCCESS -> {
                        chipStatus.text = root.context.getString(R.string.success)
                        chipStatus.setChipBackgroundColorResource(R.color.status_active)
                        textResultMessage.setTextColor(
                            ContextCompat.getColor(root.context, R.color.status_active)
                        )
                    }
                    ControlStatus.FAILED -> {
                        chipStatus.text = root.context.getString(R.string.failed)
                        chipStatus.setChipBackgroundColorResource(R.color.status_inactive)
                        textResultMessage.setTextColor(
                            ContextCompat.getColor(root.context, R.color.status_inactive)
                        )
                    }
                    ControlStatus.PENDING -> {
                        chipStatus.text = root.context.getString(R.string.pending)
                        chipStatus.setChipBackgroundColorResource(R.color.status_pending)
                        textResultMessage.setTextColor(
                            ContextCompat.getColor(root.context, R.color.status_pending)
                        )
                    }
                    ControlStatus.OFFLINE, ControlStatus.TIMEOUT -> {
                        chipStatus.text = root.context.getString(R.string.failed)
                        chipStatus.setChipBackgroundColorResource(R.color.status_inactive)
                        textResultMessage.setTextColor(
                            ContextCompat.getColor(root.context, R.color.status_inactive)
                        )
                    }
                }

                // 消息
                textResultMessage.text = record.message

                // 时间
                textTimestamp.text = formatTimestamp(record.timestamp)
            }
        }

        private fun getCommandIconAndName(command: VehicleCommand): Pair<Int, Int> {
            return when (command) {
                VehicleCommand.LOCK -> R.drawable.ic_lock to R.string.cmd_lock
                VehicleCommand.UNLOCK -> R.drawable.ic_unlock to R.string.cmd_unlock
                VehicleCommand.START_ENGINE -> R.drawable.ic_engine to R.string.cmd_start_engine
                VehicleCommand.STOP_ENGINE -> R.drawable.ic_engine to R.string.cmd_stop_engine
                VehicleCommand.OPEN_TRUNK, VehicleCommand.CLOSE_TRUNK -> R.drawable.ic_trunk to R.string.cmd_trunk
                VehicleCommand.FIND_CAR -> R.drawable.ic_find_car to R.string.cmd_find_car
                VehicleCommand.REMOTE_START -> R.drawable.ic_remote_start to R.string.cmd_remote_start
                VehicleCommand.FLASH_LIGHTS, VehicleCommand.HONK_HORN -> R.drawable.ic_flash to R.string.cmd_flash
                VehicleCommand.START_CLIMATE, VehicleCommand.STOP_CLIMATE, VehicleCommand.SET_CLIMATE_TEMP -> R.drawable.ic_climate to R.string.cmd_climate
                VehicleCommand.OPEN_WINDOWS, VehicleCommand.CLOSE_WINDOWS -> R.drawable.ic_window to R.string.cmd_window
                else -> R.drawable.ic_car to R.string.cmd_other
            }
        }

        private fun formatTimestamp(isoString: String): String {
            return try {
                val inputFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US).apply {
                    timeZone = TimeZone.getTimeZone("UTC")
                }
                val outputFormat = SimpleDateFormat("MM-dd HH:mm", Locale.getDefault())
                val date = inputFormat.parse(isoString)
                outputFormat.format(date!!)
            } catch (e: Exception) {
                isoString
            }
        }
    }

    private class HistoryDiffCallback : DiffUtil.ItemCallback<HistoryRecord>() {
        override fun areItemsTheSame(oldItem: HistoryRecord, newItem: HistoryRecord): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: HistoryRecord, newItem: HistoryRecord): Boolean {
            return oldItem == newItem
        }
    }
}
