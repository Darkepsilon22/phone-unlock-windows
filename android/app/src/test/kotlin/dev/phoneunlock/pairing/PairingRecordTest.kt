package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PairingRecordTest {

    @Test
    fun survivesEncodingRoundTrip() {
        val record = PairingRecord(
            pcId = ByteArray(16) { 1 },
            pcFingerprint = ByteArray(32) { 2 },
            pairingId = ByteArray(16) { 3 },
            endpoints = listOf(Endpoint("192.168.88.159", 47863)),
            pcName = "PC salon",
            accountName = "hanjason",
        )

        val decoded = checkNotNull(PairingRecord.decode(record.encode()))

        assertArrayEquals(record.pcId, decoded.pcId)
        assertArrayEquals(record.pcFingerprint, decoded.pcFingerprint)
        assertArrayEquals(record.pairingId, decoded.pairingId)
        assertEquals(record.endpoints, decoded.endpoints)
        assertEquals(record.pcName, decoded.pcName)
        assertEquals(record.accountName, decoded.accountName)
    }

    @Test
    fun rejectsDamagedFiles() {
        assertNull(PairingRecord.decode(ByteArray(0)))
        assertNull(PairingRecord.decode(byteArrayOf(0x01)))
        assertNull(PairingRecord.decode(Cbor.encode(Cbor.Object(mapOf(1L to Cbor.ByteString(ByteArray(16)))))))
    }
}
