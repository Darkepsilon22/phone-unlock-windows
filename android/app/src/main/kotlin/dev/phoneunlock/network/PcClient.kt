package dev.phoneunlock.network

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.pairing.Endpoint
import java.io.ByteArrayOutputStream
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

class PcClient(private val socketFactory: SSLSocketFactory) {

    fun post(endpoint: Endpoint, path: String, body: Cbor): PcResponse {
        val payload = Cbor.encode(body)
        val url = URI(SCHEME, null, endpoint.host, endpoint.port, path, null, null).toURL()
        val connection = url.openConnection() as HttpsURLConnection
        try {
            connection.sslSocketFactory = socketFactory
            connection.hostnameVerifier = PinnedKeyReplacesHostnameCheck
            connection.connectTimeout = CONNECT_TIMEOUT_MS
            connection.readTimeout = READ_TIMEOUT_MS
            connection.requestMethod = "POST"
            connection.doOutput = true
            connection.setRequestProperty("Content-Type", CBOR_TYPE)
            connection.setRequestProperty("Accept", CBOR_TYPE)
            connection.setFixedLengthStreamingMode(payload.size)
            connection.outputStream.use { it.write(payload) }

            val status = connection.responseCode
            val stream = if (status in SUCCESS_STATUS) connection.inputStream else connection.errorStream
            val bytes = stream?.use(::readLimited) ?: ByteArray(0)
            return PcResponse(status, if (bytes.isEmpty()) null else Cbor.decode(bytes) as? Cbor.Object)
        } finally {
            connection.disconnect()
        }
    }

    private fun readLimited(input: InputStream): ByteArray {
        val out = ByteArrayOutputStream()
        val buffer = ByteArray(BUFFER_SIZE)
        while (true) {
            val read = input.read(buffer)
            if (read < 0) {
                return out.toByteArray()
            }
            require(out.size() + read <= MAX_RESPONSE_SIZE) { "Réponse du PC trop volumineuse" }
            out.write(buffer, 0, read)
        }
    }

    private companion object {
        const val SCHEME = "https"
        const val CBOR_TYPE = "application/cbor"
        const val CONNECT_TIMEOUT_MS = 5_000
        const val READ_TIMEOUT_MS = 10_000
        const val BUFFER_SIZE = 4096
        const val MAX_RESPONSE_SIZE = 16 * 1024
    }
}
