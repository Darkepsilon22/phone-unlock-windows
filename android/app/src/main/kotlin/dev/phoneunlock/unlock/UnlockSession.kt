package dev.phoneunlock.unlock

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.network.PcConnection
import dev.phoneunlock.network.PcRequest
import dev.phoneunlock.network.PcResponse
import dev.phoneunlock.pairing.Endpoint
import dev.phoneunlock.pairing.PairingRecord

class UnlockFailure(val code: String) : Exception(code)

class UnlockSession(private val connection: PcConnection, private val pairing: PairingRecord) {

    fun state(endpoint: Endpoint, since: Long): PcStatus =
        PcStatus.parse(checked(connection.send(PcRequest(endpoint, STATE_PATH, post = false, query = "since=$since"))))

    fun request(endpoint: Endpoint): AuthorizationRequest =
        AuthorizationRequest.parse(checked(connection.send(PcRequest(endpoint, UNLOCK_PATH, post = true))), pairing)

    fun active(endpoint: Endpoint): AuthorizationRequest? {
        val response = connection.send(PcRequest(endpoint, ACTIVE_PATH, post = false))
        if (response.status == NO_CONTENT) {
            return null
        }
        return AuthorizationRequest.parse(checked(response), pairing)
    }

    fun prove(endpoint: Endpoint, request: AuthorizationRequest, rawSignature: ByteArray): String {
        val proof = Cbor.Object(mapOf(1L to Cbor.ByteString(request.requestId), 2L to Cbor.ByteString(rawSignature)))
        return stateOf(connection.send(PcRequest(endpoint, requestPath(request, PROOF_SUFFIX), post = true, body = proof)))
    }

    fun result(endpoint: Endpoint, request: AuthorizationRequest): String =
        stateOf(connection.send(PcRequest(endpoint, requestPath(request, RESULT_SUFFIX), post = false)))

    private fun stateOf(response: PcResponse): String {
        val body = checked(response)
        (body[ERROR_FIELD] as? Cbor.Text)?.let { throw UnlockFailure(it.value) }
        return (body[STATE_FIELD] as? Cbor.Text)?.value ?: throw UnlockFailure(INVALID_RESPONSE)
    }

    private fun checked(response: PcResponse): Cbor.Object {
        if (!response.successful) {
            throw UnlockFailure(response.errorCode ?: INVALID_RESPONSE)
        }
        return response.body ?: throw UnlockFailure(INVALID_RESPONSE)
    }

    private fun requestPath(request: AuthorizationRequest, suffix: String): String =
        REQUESTS_PREFIX + request.requestId.joinToString("") { "%02x".format(it) } + suffix

    companion object {
        const val INVALID_RESPONSE = "INVALID_RESPONSE"
        private const val STATE_PATH = "/v1/state"
        private const val UNLOCK_PATH = "/v1/unlock"
        private const val REQUESTS_PREFIX = "/v1/requests/"
        private const val ACTIVE_PATH = "/v1/requests/active"
        private const val NO_CONTENT = 204
        private const val PROOF_SUFFIX = "/proof"
        private const val RESULT_SUFFIX = "/result"
        private const val STATE_FIELD = 1L
        private const val ERROR_FIELD = 2L
    }
}
