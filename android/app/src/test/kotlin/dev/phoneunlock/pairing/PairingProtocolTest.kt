package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import java.util.Base64
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class PairingProtocolTest {

    @Test
    fun transcriptAndCodeMatchWindowsReferenceVector() {
        val transcript = PairingProtocol.transcript(
            pcId = filled(16, 1),
            pcFingerprint = filled(32, 2),
            transportFingerprint = filled(32, 3),
            authorizationFingerprint = filled(32, 4),
            token = filled(32, 5),
            pairingId = filled(16, 6),
        )
        assertEquals("a5525aa572b00afb421f320c95677ee35dca2645caeae64b4d7473bd6761e803", hex(transcript))
        assertEquals("637797", PairingProtocol.comparisonCode(transcript))
    }

    @Test
    fun comparisonCodeKeepsLeadingZeros() {
        assertEquals("000000", PairingProtocol.comparisonCode(ByteArray(32)))
        assertEquals("967295", PairingProtocol.comparisonCode(filled(4, 0xFF) + ByteArray(28)))
    }

    @Test
    fun confirmationMessageIsDomainSeparated() {
        val message = PairingProtocol.confirmationMessage(ByteArray(32))
        val domain = "PhoneUnlock-v1-pair-confirm".toByteArray(Charsets.US_ASCII)
        assertArrayEquals(domain + 0.toByte() + ByteArray(32), message)
    }

    @Test
    fun parsesWindowsReferenceQr() {
        val qr = checkNotNull(PairingQr.parse(REFERENCE_QR))
        assertArrayEquals(filled(16, 0x11), qr.pcId)
        assertArrayEquals(ByteArray(32), qr.pcFingerprint)
        assertArrayEquals(filled(32, 0x22), qr.token)
        assertEquals(listOf(Endpoint("10.0.0.2", 47863)), qr.endpoints)
    }

    @Test
    fun rejectsForeignOrDamagedQr() {
        assertNull(PairingQr.parse("https://example.com"))
        assertNull(PairingQr.parse("phoneunlock:2:" + REFERENCE_QR.removePrefix("phoneunlock:1:")))
        assertNull(PairingQr.parse(REFERENCE_QR.dropLast(4)))
        assertNull(PairingQr.parse("phoneunlock:1:***"))
    }

    @Test
    fun rejectsQrWithWrongFieldsOrValues() {
        assertNull(PairingQr.parse(qrWith(VERSION to Cbor.Integer(2))))
        assertNull(PairingQr.parse(qrWith(PC_ID to Cbor.ByteString(ByteArray(15)))))
        assertNull(PairingQr.parse(qrWith(ENDPOINTS to Cbor.Items(emptyList()))))
        assertNull(PairingQr.parse(qrWith(ENDPOINTS to endpoints("bad host", 47863))))
        assertNull(PairingQr.parse(qrWith(ENDPOINTS to endpoints("10.0.0.2", 0))))
        assertNull(PairingQr.parse(qrWith(ENDPOINTS to endpoints("10.0.0.2", 65536))))
        assertNull(PairingQr.parse(qrWith(6L to Cbor.Integer(0))))
    }

    @Test
    fun acceptsValidQrBuiltFromFields() {
        assertNotNull(PairingQr.parse(qrWith()))
    }

    private fun qrWith(vararg overrides: Pair<Long, Cbor>): String {
        val fields = mutableMapOf(
            VERSION to Cbor.Integer(1),
            PC_ID to Cbor.ByteString(filled(16, 0x11)),
            3L to Cbor.ByteString(ByteArray(32)),
            4L to Cbor.ByteString(filled(32, 0x22)),
            ENDPOINTS to endpoints("10.0.0.2", 47863),
        )
        fields.putAll(overrides)
        val encoded = Cbor.encode(Cbor.Object(fields))
        return "phoneunlock:1:" + Base64.getUrlEncoder().withoutPadding().encodeToString(encoded)
    }

    private fun endpoints(host: String, port: Long) =
        Cbor.Items(listOf(Cbor.Items(listOf(Cbor.Text(host), Cbor.Integer(port)))))

    private fun filled(size: Int, value: Int) = ByteArray(size) { value.toByte() }

    private fun hex(bytes: ByteArray) = bytes.joinToString("") { "%02x".format(it) }

    private companion object {
        const val VERSION = 1L
        const val PC_ID = 2L
        const val ENDPOINTS = 5L
        const val REFERENCE_QR = "phoneunlock:1:pQEBAlARERERERERERERERERERERA1ggAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEWCAi" +
            "IiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIgWBgmgxMC4wLjAuMhm69w"
    }
}
