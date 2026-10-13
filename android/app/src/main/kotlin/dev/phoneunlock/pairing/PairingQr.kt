package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor
import java.util.Base64

class PairingQr private constructor(
    val pcId: ByteArray,
    val pcFingerprint: ByteArray,
    val token: ByteArray,
    val endpoints: List<Endpoint>,
) {
    companion object {
        const val MAX_ENDPOINTS = 8
        private const val PREFIX = "phoneunlock:1:"
        private const val VERSION_FIELD = 1L
        private const val PC_ID_FIELD = 2L
        private const val FINGERPRINT_FIELD = 3L
        private const val TOKEN_FIELD = 4L
        private const val ENDPOINTS_FIELD = 5L
        private val FIELDS = setOf(VERSION_FIELD, PC_ID_FIELD, FINGERPRINT_FIELD, TOKEN_FIELD, ENDPOINTS_FIELD)

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
                pcId = fixedBytes(fields[PC_ID_FIELD], PairingProtocol.PC_ID_SIZE),
                pcFingerprint = fixedBytes(fields[FINGERPRINT_FIELD], PairingProtocol.FINGERPRINT_SIZE),
                token = fixedBytes(fields[TOKEN_FIELD], PairingProtocol.TOKEN_SIZE),
                endpoints = Endpoint.listFromCbor(fields[ENDPOINTS_FIELD], MAX_ENDPOINTS),
            )
        }
    }
}

internal fun fixedBytes(value: Cbor?, size: Int): ByteArray {
    val bytes = requireNotNull(value as? Cbor.ByteString) { "Champ binaire attendu" }.value
    require(bytes.size == size) { "Taille de champ inattendue" }
    return bytes
}
