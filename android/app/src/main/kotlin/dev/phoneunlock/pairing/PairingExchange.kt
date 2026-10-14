package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.network.PcConnection
import dev.phoneunlock.network.PcRequest
import dev.phoneunlock.network.PcResponse
import java.io.IOException
import java.security.cert.CertificateException
import javax.net.ssl.SSLHandshakeException

class PairingFailure(val reason: Reason) : Exception(reason.name) {
    enum class Reason { PC_UNREACHABLE, WRONG_PC, EXPIRED, REFUSED, PC_NOT_CONFIRMED, INVALID_RESPONSE }
}

class PairingChallenge(
    val endpoint: Endpoint,
    val pairingId: ByteArray,
    val pcName: String,
    val accountName: String,
    val transcript: ByteArray,
) {
    val code: String get() = PairingProtocol.comparisonCode(transcript)
}

class PairingExchange(
    private val qr: PairingQr,
    private val client: PcConnection,
    private val transportSpki: ByteArray,
    private val authorizationSpki: ByteArray,
) {

    fun requestChallenge(phoneName: String): PairingChallenge {
        val request = Cbor.Object(
            mapOf(
                TOKEN_FIELD to Cbor.ByteString(qr.token),
                AUTHORIZATION_KEY_FIELD to Cbor.ByteString(authorizationSpki),
                PHONE_NAME_FIELD to Cbor.Text(phoneName),
            ),
        )
        var lastError: IOException? = null
        for (endpoint in qr.endpoints) {
            val response = try {
                client.send(PcRequest(endpoint, PAIR_PATH, post = true, body = request))
            } catch (handshake: SSLHandshakeException) {
                if (handshake.hasCause<CertificateException>()) throw PairingFailure(PairingFailure.Reason.WRONG_PC)
                lastError = handshake
                continue
            } catch (unreachable: IOException) {
                lastError = unreachable
                continue
            }
            return challengeFrom(endpoint, response)
        }
        throw PairingFailure(PairingFailure.Reason.PC_UNREACHABLE).apply { lastError?.let(::initCause) }
    }

    fun confirm(challenge: PairingChallenge, rawSignature: ByteArray) {
        val request = Cbor.Object(
            mapOf(
                CONFIRM_PAIRING_ID_FIELD to Cbor.ByteString(challenge.pairingId),
                SIGNATURE_FIELD to Cbor.ByteString(rawSignature),
            ),
        )
        val response = try {
            client.send(PcRequest(challenge.endpoint, CONFIRM_PATH, post = true, body = request))
        } catch (unreachable: IOException) {
            throw PairingFailure(PairingFailure.Reason.PC_UNREACHABLE).apply { initCause(unreachable) }
        }
        if (!response.successful) {
            throw failureFor(response)
        }
        if ((response.body?.get(STATE_FIELD) as? Cbor.Text)?.value != PAIRED_STATE) {
            throw PairingFailure(PairingFailure.Reason.INVALID_RESPONSE)
        }
    }

    private fun challengeFrom(endpoint: Endpoint, response: PcResponse): PairingChallenge {
        if (!response.successful) {
            throw failureFor(response)
        }
        val body = response.body ?: throw PairingFailure(PairingFailure.Reason.INVALID_RESPONSE)
        val pairingId = (body[PAIRING_ID_FIELD] as? Cbor.ByteString)?.value
        val pcName = (body[PC_NAME_FIELD] as? Cbor.Text)?.value
        val accountName = (body[ACCOUNT_NAME_FIELD] as? Cbor.Text)?.value
        if (pairingId?.size != PairingProtocol.PAIRING_ID_SIZE || pcName.isNullOrBlank() || accountName.isNullOrBlank()) {
            throw PairingFailure(PairingFailure.Reason.INVALID_RESPONSE)
        }
        val transcript = PairingProtocol.transcript(
            pcId = qr.pcId,
            pcFingerprint = qr.pcFingerprint,
            transportFingerprint = PairingProtocol.fingerprint(transportSpki),
            authorizationFingerprint = PairingProtocol.fingerprint(authorizationSpki),
            token = qr.token,
            pairingId = pairingId,
        )
        return PairingChallenge(endpoint, pairingId, pcName, accountName, transcript)
    }

    private fun failureFor(response: PcResponse): PairingFailure = PairingFailure(
        when (response.errorCode) {
            "WRONG_CONTEXT" -> PairingFailure.Reason.PC_NOT_CONFIRMED
            "EXPIRED" -> PairingFailure.Reason.EXPIRED
            "INVALID_PROOF" -> PairingFailure.Reason.REFUSED
            else -> PairingFailure.Reason.INVALID_RESPONSE
        },
    )

    private inline fun <reified T : Throwable> Throwable.hasCause(): Boolean =
        generateSequence(this as Throwable?) { it.cause }.any { it is T }

    private companion object {
        const val PAIR_PATH = "/v1/pair"
        const val CONFIRM_PATH = "/v1/pair/confirm"
        const val TOKEN_FIELD = 1L
        const val AUTHORIZATION_KEY_FIELD = 2L
        const val PHONE_NAME_FIELD = 3L
        const val PAIRING_ID_FIELD = 1L
        const val PC_NAME_FIELD = 2L
        const val ACCOUNT_NAME_FIELD = 3L
        const val CONFIRM_PAIRING_ID_FIELD = 1L
        const val SIGNATURE_FIELD = 2L
        const val STATE_FIELD = 1L
        const val PAIRED_STATE = "paired"
    }
}
