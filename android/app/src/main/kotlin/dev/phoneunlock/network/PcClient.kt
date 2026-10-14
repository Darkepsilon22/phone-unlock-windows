package dev.phoneunlock.network

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.pairing.Endpoint
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.URI
import javax.net.ssl.HttpsURLConnection
import javax.net.ssl.SSLSocketFactory

private val SUCCESS_STATUS = 200..299

class PcResponse(val status: Int, val body: Cbor.Object?) {
    val successful: Boolean get() = status in SUCCESS_STATUS

    val errorCode: String? get() = (body?.get(ERROR_FIELD) as? Cbor.Text)?.value

    private companion object {
        const val ERROR_FIELD = 1L
    }
}

class PcRequest(
    val endpoint: Endpoint,
    val path: String,
    val post: Boolean,
    val body: Cbor? = null,
    val query: String? = null,
)

fun interface PcConnection {
    fun send(request: PcRequest): PcResponse
}

class PcClient(private val socketFactory: SSLSocketFactory, private val readTimeoutMs: Int = READ_TIMEOUT_MS) : PcConnection {

    override fun send(request: PcRequest): PcResponse {
        val endpoint = request.endpoint
        val url = URI(SCHEME, null, endpoint.host, endpoint.port, request.path, request.query, null).toURL()
        val connection = url.openConnection() as HttpsURLConnection
        try {
            connection.sslSocketFactory = socketFactory
            connection.hostnameVerifier = PinnedKeyReplacesHostnameCheck
            connection.connectTimeout = CONNECT_TIMEOUT_MS
            connection.readTimeout = readTimeoutMs
            connection.setRequestProperty("Accept", CBOR_TYPE)
            if (request.post) {
                val payload = request.body?.let(Cbor::encode) ?: ByteArray(0)
                connection.requestMethod = "POST"
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", CBOR_TYPE)
                connection.setFixedLengthStreamingMode(payload.size)
                connection.outputStream.use { it.write(payload) }
            }

            val status = connection.responseCode
            val stream = if (status in SUCCESS_STATUS) connection.inputStream else connection.errorStream
            val bytes = stream?.use(::readLimited) ?: ByteArray(0)
            return PcResponse(status, decodeObject(bytes))
        } finally {
            connection.disconnect()
        }
    }

    private fun decodeObject(bytes: ByteArray): Cbor.Object? = try {
        Cbor.decode(bytes) as? Cbor.Object
    } catch (malformed: IllegalArgumentException) {
        null
    }

    private fun readLimited(input: InputStream): ByteArray {
        val out = ByteArrayOutputStream()
        val buffer = ByteArray(BUFFER_SIZE)
        while (true) {
            val read = input.read(buffer)
            if (read < 0) {
                return out.toByteArray()
            }
            if (out.size() + read > MAX_RESPONSE_SIZE) {
                throw IOException("Réponse du PC trop volumineuse")
            }
            out.write(buffer, 0, read)
        }
    }

    companion object {
        const val READ_TIMEOUT_MS = 10_000
        private const val SCHEME = "https"
        private const val CBOR_TYPE = "application/cbor"
        private const val CONNECT_TIMEOUT_MS = 5_000
        private const val BUFFER_SIZE = 4096
        private const val MAX_RESPONSE_SIZE = 16 * 1024
    }
}
