package dev.phoneunlock.unlock

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.network.PcConnection
import dev.phoneunlock.network.PcRequest
import dev.phoneunlock.network.PcResponse
import dev.phoneunlock.pairing.Endpoint
import dev.phoneunlock.pairing.PairingRecord
import java.security.MessageDigest
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class UnlockProtocolTest {

    private val pairing = PairingRecord(
        pcId = filled(16, 3),
        pcFingerprint = filled(32, 9),
        pairingId = filled(16, 4),
        endpoints = listOf(Endpoint("192.168.88.157", 47863)),
        pcName = "PC salon",
        accountName = "hanjason",
    )
    private val endpoint = pairing.endpoints.first()

    @Test
    fun signedMessageMatchesWindowsReferenceVector() {
        val request = AuthorizationRequest.parse(referenceBody(), pairing)
        val message = request.signedMessage()
        assertEquals(192, message.size)
        assertEquals("59bdd4587141fd4adfd9baf5d3dfd0d6139785f3680ed775c895da842e722a50", sha256Hex(message))
        assertEquals(AuthorizationRequest.ACTION_UNLOCK, request.action)
        assertEquals("PC salon", request.pcName)
        assertEquals("hanjason", request.accountName)
    }

    @Test
    fun matchCodeMatchesWindowsReferenceVector() {
        assertEquals("3880", AuthorizationRequest.parse(referenceBody(), pairing).matchCode())
    }

    @Test
    fun activeRequestIsReadOrAbsent() {
        val none = UnlockSession(PcConnection { PcResponse(204, null) }, pairing)
        assertEquals(null, none.active(endpoint))

        var seen: PcRequest? = null
        val pending = UnlockSession(PcConnection { request -> seen = request; PcResponse(200, referenceBody()) }, pairing)
        assertEquals("3880", pending.active(endpoint)?.matchCode())
        assertEquals("/v1/requests/active", seen?.path)
        assertFalse(seen?.post ?: true)
    }

    @Test
    fun requestFromAnotherPcOrPairingIsRejected() {
        assertRejected(referenceBody(5L to Cbor.ByteString(filled(16, 7))))
        assertRejected(referenceBody(6L to Cbor.ByteString(filled(16, 7))))
    }

    @Test
    fun malformedRequestsAreRejected() {
        assertRejected(null)
        assertRejected(referenceBody(1L to Cbor.Integer(2)))
        assertRejected(referenceBody(2L to Cbor.Integer(3)))
        assertRejected(referenceBody(3L to Cbor.ByteString(filled(15, 1))))
        assertRejected(referenceBody(7L to Cbor.Text("X-1-5")))
        assertRejected(referenceBody(11L to Cbor.Text("")))
        assertRejected(referenceBody(13L to Cbor.Integer(1)))
    }

    @Test
    fun displayedNamesLoseControlCharacters() {
        val request = AuthorizationRequest.parse(referenceBody(11L to Cbor.Text("PC\u0007 salon")), pairing)
        assertEquals("PC salon", request.pcName)
    }

    @Test
    fun parsesPcStatus() {
        val status = PcStatus.parse(statusBody("locked", true))
        assertEquals(PcStatus(7, SessionState.LOCKED, true, "PC salon"), status)
        assertEquals(SessionState.UNKNOWN, PcStatus.parse(statusBody("sleeping", false)).session)
        assertThrows(IllegalArgumentException::class.java) { PcStatus.parse(Cbor.Object(mapOf(1L to Cbor.Integer(1)))) }
    }

    @Test
    fun unlockSessionFollowsTheProtocolPaths() {
        val requests = mutableListOf<PcRequest>()
        val session = UnlockSession(
            PcConnection { request ->
                requests += request
                when {
                    request.path == "/v1/unlock" -> PcResponse(200, referenceBody())
                    request.path.endsWith("/proof") -> PcResponse(200, Cbor.Object(mapOf(1L to Cbor.Text("consumed"))))
                    else -> PcResponse(200, Cbor.Object(mapOf(1L to Cbor.Text("logon_succeeded"))))
                }
            },
            pairing,
        )

        val request = session.request(endpoint)
        assertEquals("consumed", session.prove(endpoint, request, filled(64, 1)))
        assertEquals("logon_succeeded", session.result(endpoint, request))

        assertTrue(requests[0].post && requests[0].body == null)
        assertEquals("/v1/requests/01010101010101010101010101010101/proof", requests[1].path)
        val proof = requests[1].body as Cbor.Object
        assertArrayEquals(filled(16, 1), (proof[1] as Cbor.ByteString).value)
        assertArrayEquals(filled(64, 1), (proof[2] as Cbor.ByteString).value)
        assertFalse(requests[2].post)
        assertEquals("/v1/requests/01010101010101010101010101010101/result", requests[2].path)
    }

    @Test
    fun unlockSessionReportsPcErrors() {
        val refusing = UnlockSession(PcConnection { PcResponse(409, Cbor.Object(mapOf(1L to Cbor.Text("WRONG_CONTEXT")))) }, pairing)
        assertEquals("WRONG_CONTEXT", assertThrows(UnlockFailure::class.java) { refusing.request(endpoint) }.code)

        val rejected = UnlockSession(
            PcConnection { PcResponse(200, Cbor.Object(mapOf(1L to Cbor.Text("logon_rejected"), 2L to Cbor.Text("WINDOWS_REJECTED")))) },
            pairing,
        )
        val request = AuthorizationRequest.parse(referenceBody(), pairing)
        assertEquals("WINDOWS_REJECTED", assertThrows(UnlockFailure::class.java) { rejected.result(endpoint, request) }.code)
    }

    @Test
    fun revokedPhoneIsReportedAsSuch() {
        val revoked = UnlockSession(PcConnection { PcResponse(403, Cbor.Object(mapOf(1L to Cbor.Text("REVOKED")))) }, pairing)
        assertEquals("REVOKED", assertThrows(UnlockFailure::class.java) { revoked.state(endpoint, 0) }.code)
    }

    @Test
    fun stateRequestCarriesTheKnownVersion() {
        var seen: PcRequest? = null
        val session = UnlockSession(PcConnection { request -> seen = request; PcResponse(200, statusBody("signed_in", false)) }, pairing)
        assertEquals(SessionState.SIGNED_IN, session.state(endpoint, 12).session)
        assertEquals("/v1/state", seen?.path)
        assertEquals("since=12", seen?.query)
    }

    @Test
    fun scansTheLocalSlash24WithoutItself() {
        val hosts = PcLocator.subnetHosts(byteArrayOf(192.toByte(), 168.toByte(), 43, 1), 24)
        assertEquals(253, hosts.size)
        assertEquals("192.168.43.2", hosts.first())
        assertEquals("192.168.43.254", hosts.last())
        assertFalse("192.168.43.1" in hosts)
        assertEquals(253, PcLocator.subnetHosts(byteArrayOf(10, 0, 5, 9), 16).size)
        assertTrue(PcLocator.subnetHosts(byteArrayOf(10, 0, 0, 1), 31).isEmpty())
    }

    private fun referenceBody(vararg overrides: Pair<Long, Cbor>): Cbor.Object {
        val fields = mutableMapOf<Long, Cbor>(
            1L to Cbor.Integer(1),
            2L to Cbor.Integer(2),
            3L to Cbor.ByteString(filled(16, 1)),
            4L to Cbor.ByteString(filled(32, 2)),
            5L to Cbor.ByteString(filled(16, 3)),
            6L to Cbor.ByteString(filled(16, 4)),
            7L to Cbor.Text("S-1-5-21-1-2-3-1001"),
            8L to Cbor.ByteString(filled(16, 5)),
            9L to Cbor.Integer(1_760_000_000),
            10L to Cbor.Integer(1_760_000_030),
            11L to Cbor.Text("PC salon"),
            12L to Cbor.Text("hanjason"),
        )
        fields.putAll(overrides)
        return Cbor.Object(fields)
    }

    private fun statusBody(session: String, available: Boolean) = Cbor.Object(
        mapOf(1L to Cbor.Integer(7), 2L to Cbor.Text(session), 3L to Cbor.Flag(available), 4L to Cbor.Text("PC salon")),
    )

    private fun assertRejected(body: Cbor.Object?) {
        assertThrows(IllegalArgumentException::class.java) { AuthorizationRequest.parse(body, pairing) }
    }

    private fun filled(size: Int, value: Int) = ByteArray(size) { value.toByte() }

    private fun sha256Hex(bytes: ByteArray) =
        MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}
