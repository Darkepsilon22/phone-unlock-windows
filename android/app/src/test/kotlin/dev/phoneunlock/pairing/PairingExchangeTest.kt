package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.network.PcConnection
import dev.phoneunlock.network.PcResponse
import java.io.IOException
import java.net.ConnectException
import java.security.cert.CertificateException
import java.util.Base64
import javax.net.ssl.SSLHandshakeException
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class PairingExchangeTest {

    private val first = Endpoint("10.0.0.2", 47863)
    private val second = Endpoint("192.168.1.20", 47863)
    private val qr = checkNotNull(PairingQr.parse(qrText(first, second)))
    private val transportSpki = ByteArray(91) { 7 }
    private val authorizationSpki = ByteArray(91) { 9 }

    @Test
    fun sendsTokenKeyAndNameThenComputesTheSharedCode() {
        val requests = mutableListOf<Pair<String, Cbor.Object>>()
        val exchange = exchange { endpoint, path, body ->
            requests += path to body as Cbor.Object
            assertEquals(first, endpoint)
            challengeResponse()
        }

        val challenge = exchange.requestChallenge("Pixel 5")

        val (path, body) = requests.single()
        assertEquals("/v1/pair", path)
        assertEquals(Cbor.ByteString(qr.token), body[1])
        assertEquals(Cbor.ByteString(authorizationSpki), body[2])
        assertEquals(Cbor.Text("Pixel 5"), body[3])
        assertEquals("PC salon", challenge.pcName)
        assertEquals("hanjason", challenge.accountName)
        val expected = PairingProtocol.transcript(
            qr.pcId,
            qr.pcFingerprint,
            PairingProtocol.fingerprint(transportSpki),
            PairingProtocol.fingerprint(authorizationSpki),
            qr.token,
            PAIRING_ID,
        )
        assertArrayEquals(expected, challenge.transcript)
        assertEquals(PairingProtocol.comparisonCode(expected), challenge.code)
    }

    @Test
    fun triesTheNextAddressWhenOneIsUnreachable() {
        val tried = mutableListOf<Endpoint>()
        val exchange = exchange { endpoint, _, _ ->
            tried += endpoint
            if (endpoint == first) throw ConnectException("injoignable")
            challengeResponse()
        }

        assertEquals(second, exchange.requestChallenge("Pixel 5").endpoint)
        assertEquals(listOf(first, second), tried)
    }

    @Test
    fun stopsAtOnceWhenThePcKeyDoesNotMatch() {
        val tried = mutableListOf<Endpoint>()
        val exchange = exchange { endpoint, _, _ ->
            tried += endpoint
            throw SSLHandshakeException("refus").apply { initCause(CertificateException("autre PC")) }
        }

        assertReason(PairingFailure.Reason.WRONG_PC) { exchange.requestChallenge("Pixel 5") }
        assertEquals(listOf(first), tried)
    }

    @Test
    fun reportsUnreachableWhenEveryAddressFails() {
        val exchange = exchange { _, _, _ -> throw IOException("réseau") }
        assertReason(PairingFailure.Reason.PC_UNREACHABLE) { exchange.requestChallenge("Pixel 5") }
    }

    @Test
    fun mapsPcErrorsToReasons() {
        assertReason(PairingFailure.Reason.REFUSED) { exchange(error(403, "INVALID_PROOF")).requestChallenge("Pixel 5") }
        assertReason(PairingFailure.Reason.EXPIRED) { exchange(error(410, "EXPIRED")).requestChallenge("Pixel 5") }
        assertReason(PairingFailure.Reason.INVALID_RESPONSE) { exchange(error(403, "UNSUPPORTED_TLS")).requestChallenge("Pixel 5") }
        assertReason(PairingFailure.Reason.INVALID_RESPONSE) { exchange { _, _, _ -> PcResponse(200, null) }.requestChallenge("Pixel 5") }
    }

    @Test
    fun rejectsChallengeWithWrongPairingIdSize() {
        val response = PcResponse(200, challengeBody(ByteArray(15)))
        assertReason(PairingFailure.Reason.INVALID_RESPONSE) { exchange { _, _, _ -> response }.requestChallenge("Pixel 5") }
    }

    @Test
    fun confirmsWithPairingIdAndSignature() {
        val challenge = exchange { _, _, _ -> challengeResponse() }.requestChallenge("Pixel 5")
        val signature = ByteArray(64) { 3 }
        var sent: Cbor.Object? = null
        val exchange = exchange { endpoint, path, body ->
            assertEquals(first, endpoint)
            assertEquals("/v1/pair/confirm", path)
            sent = body as Cbor.Object
            PcResponse(200, Cbor.Object(mapOf(1L to Cbor.Text("paired"))))
        }

        exchange.confirm(challenge, signature)

        assertEquals(Cbor.ByteString(PAIRING_ID), sent?.get(1))
        assertEquals(Cbor.ByteString(signature), sent?.get(2))
    }

    @Test
    fun asksToConfirmOnThePcFirst() {
        val challenge = exchange { _, _, _ -> challengeResponse() }.requestChallenge("Pixel 5")
        assertReason(PairingFailure.Reason.PC_NOT_CONFIRMED) { exchange(error(409, "WRONG_CONTEXT")).confirm(challenge, ByteArray(64)) }
        assertReason(PairingFailure.Reason.INVALID_RESPONSE) {
            exchange { _, _, _ -> PcResponse(200, Cbor.Object(mapOf(1L to Cbor.Text("pending")))) }.confirm(challenge, ByteArray(64))
        }
    }

    private fun exchange(connection: PcConnection) = PairingExchange(qr, connection, transportSpki, authorizationSpki)

    private fun error(status: Int, code: String) = PcConnection { _, _, _ -> PcResponse(status, Cbor.Object(mapOf(1L to Cbor.Text(code)))) }

    private fun challengeResponse() = PcResponse(200, challengeBody(PAIRING_ID))

    private fun challengeBody(pairingId: ByteArray) = Cbor.Object(
        mapOf(1L to Cbor.ByteString(pairingId), 2L to Cbor.Text("PC salon"), 3L to Cbor.Text("hanjason")),
    )

    private fun assertReason(expected: PairingFailure.Reason, block: () -> Unit) {
        assertEquals(expected, assertThrows(PairingFailure::class.java) { block() }.reason)
    }

    private fun qrText(vararg endpoints: Endpoint): String {
        val encoded = Cbor.encode(
            Cbor.Object(
                mapOf(
                    1L to Cbor.Integer(1),
                    2L to Cbor.ByteString(ByteArray(16) { 0x11 }),
                    3L to Cbor.ByteString(ByteArray(32) { 0x33 }),
                    4L to Cbor.ByteString(ByteArray(32) { 0x22 }),
                    5L to Cbor.Items(endpoints.map(Endpoint::toCbor)),
                ),
            ),
        )
        return "phoneunlock:1:" + Base64.getUrlEncoder().withoutPadding().encodeToString(encoded)
    }

    private companion object {
        val PAIRING_ID = ByteArray(16) { 6 }
    }
}
