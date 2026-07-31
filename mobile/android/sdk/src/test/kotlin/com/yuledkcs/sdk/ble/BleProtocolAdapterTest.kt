package com.yuledkcs.sdk.ble

import org.junit.Assert.*
import org.junit.Test

class BleProtocolAdapterTest {

    @Test
    fun `CCC adapter parses command response`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)

        val success = adapter.parseCommandResponse(byteArrayOf(0x00))
        assertTrue(success.success)

        val failure = adapter.parseCommandResponse(byteArrayOf(0x10))
        assertFalse(failure.success)
        assertEquals(16, failure.errorCode)
    }

    @Test
    fun `CCC adapter builds unlock command`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)
        val session = SessionContext(keyId = "key-1", vehicleId = "VH-1")

        val command = adapter.buildUnlockCommand("key-1", session)
        assertEquals(0x01.toByte(), command[0])
        assertEquals(8, command.size)
    }

    @Test
    fun `CCC adapter parses vehicle status`() {
        val adapter = BleProtocolAdapterFactory.makeAdapter(BleProtocolType.CCC)

        val status = adapter.parseVehicleStatus(byteArrayOf(0x01, 0x00, 0x50))
        assertTrue(status.locked)
        assertFalse(status.engineOn)
        assertEquals(80, status.batteryPct)
    }

    @Test
    fun `all adapters parse responses`() {
        for (type in BleProtocolType.entries) {
            val adapter = BleProtocolAdapterFactory.makeAdapter(type)
            val result = adapter.parseCommandResponse(byteArrayOf(0x00))
            assertTrue("${type.name} should succeed on 0x00", result.success)
        }
    }

    @Test
    fun `service uuids match spec`() {
        assertEquals("0000FFD1-0000-1000-8000-00805F9B34FB", BleUuids.CCC_SERVICE.toString())
        assertEquals("0000FEF5-0000-1000-8000-00805F9B34FB", BleUuids.ICCOA_SERVICE.toString())
        assertEquals("0000FEFA-0000-1000-8000-00805F9B34FB", BleUuids.ICCE_SERVICE.toString())
    }
}
