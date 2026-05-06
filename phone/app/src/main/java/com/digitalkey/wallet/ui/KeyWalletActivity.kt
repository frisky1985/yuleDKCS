package com.digitalkey.wallet.ui

import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.digitalkey.wallet.Models
import com.digitalkey.wallet.R
import com.digitalkey.wallet.api.HubApiClient
import com.digitalkey.wallet.ble.BleManager
import com.digitalkey.wallet.nfc.NfcManager
import com.digitalkey.wallet.uwb.UwbManager
import kotlinx.coroutines.launch

/**
 * 主界面 - 数字钥匙钱包
 * 显示所有已绑定的车辆密钥，支持一键解锁/闭锁
 */
class KeyWalletActivity : AppCompatActivity() {

    private lateinit var recyclerView: RecyclerView
    private lateinit var keyAdapter: DigitalKeyAdapter
    private lateinit var bleManager: BleManager
    private lateinit var nfcManager: NfcManager
    private lateinit var apiClient: HubApiClient

    private var uwbManager: UwbManager? = null
    private var keys = mutableListOf<Models.DigitalKey>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // setContentView(R.layout.activity_key_wallet)

        // Init managers
        bleManager = BleManager(this)
        nfcManager = NfcManager(this)
        apiClient = HubApiClient("https://hub.digitalkey.local")

        // Setup RecyclerView
        keyAdapter = DigitalKeyAdapter(keys) { key, action ->
            executeAction(key, action)
        }

        // Load keys from cloud
        loadKeys()

        // Setup BLE auto-connect
        bleManager.onConnected = {
            runOnUiThread { updateConnectionStatus("BLE已连接") }
            // Start UWB ranging after BLE connected
            uwbManager?.startRanging(sessionId = 1)
        }
        bleManager.onDisconnected = {
            runOnUiThread { updateConnectionStatus("BLE已断开") }
            uwbManager?.stopRanging()
        }
        bleManager.onDataReceived = { data ->
            // Handle protocol response
        }

        // Setup NFC
        nfcManager.onTagDiscovered = { tag, isoDep ->
            // NFC tag detected - authenticate
            nfcAuthenticate(isoDep)
        }
    }

    override fun onResume() {
        super.onResume()
        nfcManager.enableForegroundDispatch()
    }

    override fun onPause() {
        super.onPause()
        nfcManager.disableForegroundDispatch()
    }

    /**
     * 加载密钥列表
     */
    private fun loadKeys() {
        lifecycleScope.launch {
            val result = apiClient.listKeys()
            result.onSuccess { response ->
                keys.clear()
                keys.addAll(response.keys)
                keyAdapter.notifyDataSetChanged()
            }.onFailure { e ->
                Toast.makeText(this@KeyWalletActivity, "加载失败: ${e.message}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    /**
     * 执行控制命令
     */
    private fun executeAction(key: Models.DigitalKey, action: Models.ControlAction) {
        lifecycleScope.launch {
            // Check if BLE connected for local control
            if (bleManager.isConnected()) {
                // Local control via BLE (faster, no cloud round-trip)
                executeLocalAction(key, action)
            } else {
                // Remote control via cloud
                executeRemoteAction(key, action)
            }
        }
    }

    /**
     * 本地控制 (BLE直接通信)
     */
    private fun executeLocalAction(key: Models.DigitalKey, action: Models.ControlAction) {
        val protocol = bleManager.getCurrentProtocol()
        val frame = when (protocol) {
            Models.Protocol.CCC_DK3 -> buildCCCFrame(action)
            Models.Protocol.ICCOA_DK40 -> buildICCOAFrame(action)
            Models.Protocol.ICCE -> buildICCEFrame(action)
            else -> null
        }

        frame?.let { bleManager.sendData(it) }
    }

    /**
     * 远程控制 (通过云端)
     */
    private suspend fun executeRemoteAction(key: Models.DigitalKey, action: Models.ControlAction) {
        val result = apiClient.sendCommand(key.vehicleId, action, key.keyId)
        result.onSuccess { response ->
            if (response.result_code == 0) {
                Toast.makeText(this, "命令已发送", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "执行失败: ${response.error_msg}", Toast.LENGTH_SHORT).show()
            }
        }.onFailure { e ->
            Toast.makeText(this, "网络错误: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    /**
     * NFC认证
     */
    private fun nfcAuthenticate(isoDep: android.nfc.tech.IsoDep) {
        lifecycleScope.launch(Dispatchers.IO) {
            // 1. Select app
            val protocol = nfcManager.selectCccApp(isoDep)
                ?: return@launch

            // 2. Get challenge from vehicle
            // 3. Sign challenge with SE
            // 4. Send signed response
            // val signedChallenge = seManager.sign(challenge)
            // nfcManager.nfcAuthenticate(isoDep, signedChallenge)
        }
    }

    // ── Protocol frame builders ──

    private fun buildCCCFrame(action: Models.ControlAction): ByteArray {
        // CCC Digital Key 3.0 frame format
        return byteArrayOf(
            0x00,  // CLA
            0x20,  // INS = Control
            actionToCCCCode(action),  // P1
            0x00,  // P2
            0x00   // Le
        )
    }

    private fun buildICCOAFrame(action: Models.ControlAction): ByteArray {
        // ICCOA DK4.0 frame
        return byteArrayOf(
            0xC0, 0x1C,  // Magic
            0x04,        // Version
            0x30,        // Ctrl
            0x00, 0x00,  // Msg ID
            0x00, 0x02,  // Payload len
            actionToICCOACode(action), 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Session token
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // HMAC
        )
    }

    private fun buildICCEFrame(action: Models.ControlAction): ByteArray {
        // ICCE frame (similar to ICCOA with edge trigger)
        return buildICCOAFrame(action)
    }

    private fun actionToCCCCode(action: Models.ControlAction): Byte = when (action) {
        Models.ControlAction.UNLOCK -> 0x01
        Models.ControlAction.LOCK -> 0x02
        Models.ControlAction.ENGINE_ON -> 0x03
        Models.ControlAction.ENGINE_OFF -> 0x04
        Models.ControlAction.TRUNK_OPEN -> 0x05
        Models.ControlAction.FIND -> 0x0A
        else -> 0x00
    }

    private fun actionToICCOACode(action: Models.ControlAction): Byte = when (action) {
        Models.ControlAction.UNLOCK -> 0x02
        Models.ControlAction.LOCK -> 0x01
        Models.ControlAction.ENGINE_ON -> 0x03
        Models.ControlAction.ENGINE_OFF -> 0x04
        Models.ControlAction.TRUNK_OPEN -> 0x05
        Models.ControlAction.FIND -> 0x0A
        else -> 0x00
    }

    private fun updateConnectionStatus(status: String) {
        Toast.makeText(this, status, Toast.LENGTH_SHORT).show()
    }

    // ── RecyclerView Adapter ──
    inner class DigitalKeyAdapter(
        private val keys: List<Models.DigitalKey>,
        private val onAction: (Models.DigitalKey, Models.ControlAction) -> Unit
    ) : RecyclerView.Adapter<KeyViewHolder>() {

        override fun onCreateViewHolder(parent: android.view.ViewGroup, viewType: Int): KeyViewHolder {
            val view = android.view.LayoutInflater.from(parent.context)
                .inflate(android.R.layout.simple_list_item_2, parent, false)
            return KeyViewHolder(view)
        }

        override fun onBindViewHolder(holder: KeyViewHolder, position: Int) {
            val key = keys[position]
            holder.bind(key, onAction)
        }

        override fun getItemCount() = keys.size
    }

    class KeyViewHolder(view: android.view.View) : RecyclerView.ViewHolder(view) {
        fun bind(key: Models.DigitalKey, onAction: (Models.DigitalKey, Models.ControlAction) -> Unit) {
            // Bind key data to view
        }
    }
}
