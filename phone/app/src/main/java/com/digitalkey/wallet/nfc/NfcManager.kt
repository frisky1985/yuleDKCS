package com.digitalkey.wallet.nfc

import android.app.Activity
import android.content.Intent
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.IsoDep
import android.os.Build
import android.util.Log
import com.digitalkey.wallet.Models
import java.io.ByteArrayOutputStream

/**
 * NFC数字钥匙管理器
 * 支持:
 * - ISO 14443 Type A/B 读写 (配对/认证)
 * - Host Card Emulation (HCE) 模拟卡片 (被动启动)
 */
class NfcManager(activity: Activity) {

    companion object {
        private const val TAG = "NfcManager"

        // CCC Digital Key AID
        private const val CCC_AID = "A00000080400010101"
        // ICCOA AID
        private const val ICCOA_AID = "A00000080400010201"

        // APDU commands
        private const val CLA = 0x00
        private const val INS_SELECT = 0xA4
        private const val INS_GET_DATA = 0xCA
        private const val INS_PUT_DATA = 0xDA
        private const val INS_AUTH = 0x88
        private const val INS_PAIR = 0x80
    }

    private var nfcAdapter: NfcAdapter? = null
    private var activity: Activity = activity
    var onTagDiscovered: ((Tag, IsoDep) -> Unit)? = null
    var onNfcError: ((String) -> Unit)? = null

    init {
        nfcAdapter = NfcAdapter.getDefaultAdapter(activity)
    }

    /**
     * 启用前台调度 (App在前台时拦截NFC标签)
     */
    fun enableForegroundDispatch() {
        nfcAdapter?.enableReaderMode(
            activity,
            { tag ->
                val isoDep = IsoDep.get(tag) ?: return@enableReaderMode
                isoDep.connect()
                Log.i(TAG, "NFC tag discovered: ${tag.id.toHexString()}")
                onTagDiscovered?.invoke(tag, isoDep)
            },
            NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B,
            null
        )
    }

    fun disableForegroundDispatch() {
        nfcAdapter?.disableReaderMode(activity)
    }

    /**
     * 选择CCC Digital Key应用
     */
    fun selectCccApp(isoDep: IsoDep): ByteArray? {
        val selectApdu = buildApdu(
            cla = CLA,
            ins = INS_SELECT,
            p1 = 0x04,  // Select by AID
            p2 = 0x00,
            data = CCC_AID.hexToByteArray()
        )
        return transceive(isoDep, selectApdu)
    }

    /**
     * 选择ICCOA应用
     */
    fun selectIccoaApp(isoDep: IsoDep): ByteArray? {
        val selectApdu = buildApdu(
            cla = CLA,
            ins = INS_SELECT,
            p1 = 0x04,
            p2 = 0x00,
            data = ICCOA_AID.hexToByteArray()
        )
        return transceive(isoDep, selectApdu)
    }

    /**
     * NFC配对 (绑定阶段)
     * 发送设备公钥 → 车端NFC读取 → 建立配对
     */
    fun nfcPair(isoDep: IsoDep, devicePublicKey: ByteArray): PairingResult {
        // 1. Select app
        val selectResp = selectCccApp(isoDep)
            ?: return PairingResult.Error("Select app failed")

        // 2. Send pairing command
        val pairApdu = buildApdu(
            cla = CLA,
            ins = INS_PAIR,
            p1 = 0x01,  // Pairing phase 1
            p2 = 0x00,
            data = devicePublicKey
        )
        val pairResp = transceive(isoDep, pairApdu)
            ?: return PairingResult.Error("Pairing command failed")

        // 3. Parse vehicle public key from response
        // Response: [vehicle_pubkey(64)] + [signature(64)]
        if (pairResp.size < 64) {
            return PairingResult.Error("Invalid pairing response")
        }

        val vehiclePubKey = pairResp.copyOfRange(0, 64)
        val signature = if (pairResp.size > 64) pairResp.copyOfRange(64, minOf(128, pairResp.size)) else ByteArray(0)

        return PairingResult.Success(vehiclePubKey, signature)
    }

    /**
     * NFC认证 (日常使用)
     * 挑战-响应协议: 车端发挑战 → 手机SE签名 → 车端验证
     */
    fun nfcAuthenticate(isoDep: IsoDep, signedChallenge: ByteArray): Boolean {
        val authApdu = buildApdu(
            cla = CLA,
            ins = INS_AUTH,
            p1 = 0x01,
            p2 = 0x00,
            data = signedChallenge
        )
        val resp = transceive(isoDep, authApdu) ?: return false
        // SW 9000 = success
        return resp.size >= 2 && resp[resp.size - 2] == 0x90.toByte() && resp[resp.size - 1] == 0x00.toByte()
    }

    /**
     * 发送APDU并接收响应
     */
    private fun transceive(isoDep: IsoDep, apdu: ByteArray): ByteArray? {
        return try {
            val response = isoDep.transceive(apdu)
            Log.d(TAG, "APDU response: ${response.size} bytes, SW=${response.toHexString()}")
            response
        } catch (e: Exception) {
            Log.e(TAG, "APDU transceive failed", e)
            onNfcError?.invoke(e.message ?: "Transceive failed")
            null
        }
    }

    /**
     * 构建APDU命令
     */
    private fun buildApdu(cla: Int, ins: Int, p1: Int, p2: Int, data: ByteArray? = null): ByteArray {
        val baos = ByteArrayOutputStream()
        baos.write(cla)
        baos.write(ins)
        baos.write(p1)
        baos.write(p2)
        if (data != null && data.isNotEmpty()) {
            baos.write(data.size)
            baos.write(data)
        }
        baos.write(0x00) // Le
        return baos.toByteArray()
    }

    // ── Result types ──
    sealed class PairingResult {
        data class Success(
            val vehiclePublicKey: ByteArray,
            val signature: ByteArray
        ) : PairingResult()
        data class Error(val message: String) : PairingResult()
    }

    // ── Extension functions ──
    private fun ByteArray.toHexString(): String =
        joinToString("") { "%02X".format(it) }

    private fun String.hexToByteArray(): ByteArray =
        chunked(2).map { it.toInt(16).toByte() }.toByteArray()
}
