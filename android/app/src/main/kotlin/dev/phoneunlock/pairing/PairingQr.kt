package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import java.util.Base64

data class Endpoint(val host: String, val port: Int)

class PairingQr private constructor(
    val pcId: ByteArray,
    val pcFingerprint: ByteArray,
    val token: ByteArray,
    val endpoints: List<Endpoint>,
) {
    companion object {
        private const val PREFIX = "phoneunlock:1:"
        private const val VERSION_FIELD = 1L
        private const val PC_ID_FIELD = 2L
        private const val FINGERPRINT_FIELD = 3L
        private const val TOKEN_FIELD = 4L
        private const val ENDPOINTS_FIELD = 5L
        private const val MAX_ENDPOINTS = 8
        private const val MAX_HOST_LENGTH = 253
        private const val MAX_PORT = 65535
        private val FIELDS = setOf(VERSION_FIELD, PC_ID_FIELD, FINGERPRINT_FIELD, TOKEN_FIELD, ENDPOINTS_FIELD)
        private val HOST_PATTERN = Regex("[A-Za-z0-9.-]+")

        fun parse(text: String): PairingQr? = try {
            require(text.startsWith(PREFIX)) { "Ce QR code n'est pas un QR d'association PhoneUnlock" }
            fromCbor(Cbor.decode(Base64.getUrlDecoder().decode(text.removePrefix(PREFIX))))
        } catch (invalid: IllegalArgumentException) {
            null
        }

        private fun fromCbor(value: Cbor): PairingQr {
            val fields = requireNotNull(value as? Cbor.Object) { "QR mal formé" }
            require(fields.entries.keys == FIELDS) { "Champs du QR inattendus" }
            require((fields[VERSION_FIELD] as? Cbor.Integer)?.value == PairingProtocol.VERSION) { "Version non prise en charge" }
            return PairingQr(
                pcId = bytes(fields[PC_ID_FIELD], PairingProtocol.PC_ID_SIZE),
                pcFingerprint = bytes(fields[FINGERPRINT_FIELD], PairingProtocol.FINGERPRINT_SIZE),
                token = bytes(fields[TOKEN_FIELD], PairingProtocol.TOKEN_SIZE),
                endpoints = endpoints(fields[ENDPOINTS_FIELD]),
            )
        }

        private fun bytes(value: Cbor?, size: Int): ByteArray {
            val bytes = requireNotNull(value as? Cbor.ByteString) { "Champ binaire attendu" }.value
            require(bytes.size == size) { "Taille de champ inattendue" }
            return bytes
        }

        private fun endpoints(value: Cbor?): List<Endpoint> {
            val items = requireNotNull(value as? Cbor.Items) { "Liste d'adresses attendue" }.items
            require(items.size in 1..MAX_ENDPOINTS) { "Nombre d'adresses invalide" }
            return items.map(::endpoint)
        }

        private fun endpoint(value: Cbor): Endpoint {
            val parts = requireNotNull(value as? Cbor.Items) { "Adresse mal formée" }.items
            require(parts.size == 2) { "Adresse mal formée" }
            val host = requireNotNull(parts[0] as? Cbor.Text) { "Hôte attendu" }.value
            val port = requireNotNull(parts[1] as? Cbor.Integer) { "Port attendu" }.value
            require(host.length <= MAX_HOST_LENGTH && HOST_PATTERN.matches(host)) { "Hôte invalide" }
            require(port in 1..MAX_PORT) { "Port invalide" }
            return Endpoint(host, port.toInt())
        }
    }
}
