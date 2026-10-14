package dev.phoneunlock.unlock

import dev.phoneunlock.encoding.Cbor
import dev.phoneunlock.pairing.PairingProtocol
import dev.phoneunlock.pairing.PairingRecord
import java.security.MessageDigest
import java.util.Locale

class AuthorizationRequest private constructor(private val fields: Map<Long, Cbor>) {

    val requestId: ByteArray get() = (fields.getValue(REQUEST_ID_FIELD) as Cbor.ByteString).value

    val action: Long get() = (fields.getValue(ACTION_FIELD) as Cbor.Integer).value

    val pcName: String get() = displayText(fields.getValue(PC_NAME_FIELD))

    val accountName: String get() = displayText(fields.getValue(ACCOUNT_NAME_FIELD))

    fun matchCode(): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(Cbor.encode(Cbor.Object(fields)))
        val value = ((digest[0].toInt() and 0xFF) shl Byte.SIZE_BITS) or (digest[1].toInt() and 0xFF)
        return String.format(Locale.ROOT, "%04d", value % MATCH_CODE_MODULUS)
    }

    fun signedMessage(): ByteArray =
        DOMAIN.toByteArray(Charsets.US_ASCII) + 0.toByte() + Cbor.encode(Cbor.Object(fields + (DECISION_FIELD to ALLOW)))

    companion object {
        const val ACTION_LOGON = 1L
        const val ACTION_UNLOCK = 2L
        private const val DOMAIN = "PhoneUnlock-v1-authorize"
        private const val MATCH_CODE_MODULUS = 10_000
        private const val VERSION_FIELD = 1L
        private const val ACTION_FIELD = 2L
        private const val REQUEST_ID_FIELD = 3L
        private const val NONCE_FIELD = 4L
        private const val PC_ID_FIELD = 5L
        private const val PAIRING_ID_FIELD = 6L
        private const val ACCOUNT_SID_FIELD = 7L
        private const val CONTEXT_FIELD = 8L
        private const val ISSUED_AT_FIELD = 9L
        private const val EXPIRES_AT_FIELD = 10L
        private const val PC_NAME_FIELD = 11L
        private const val ACCOUNT_NAME_FIELD = 12L
        private const val DECISION_FIELD = 13L
        private const val REQUEST_ID_SIZE = 16
        private const val NONCE_SIZE = 32
        private const val CONTEXT_SIZE = 16
        private const val MAX_SID_LENGTH = 184
        private const val MAX_NAME_LENGTH = 64
        private val ALLOW = Cbor.Integer(1)
        private val FIELDS = (VERSION_FIELD..ACCOUNT_NAME_FIELD).toSet()

        fun parse(body: Cbor.Object?, pairing: PairingRecord): AuthorizationRequest {
            val fields = requireNotNull(body) { "Demande absente" }.entries
            require(fields.keys == FIELDS) { "Champs de la demande inattendus" }
            require((fields[VERSION_FIELD] as? Cbor.Integer)?.value == PairingProtocol.VERSION) { "Version non prise en charge" }
            require((fields[ACTION_FIELD] as? Cbor.Integer)?.value in setOf(ACTION_LOGON, ACTION_UNLOCK)) { "Action inconnue" }
            requireBytes(fields[REQUEST_ID_FIELD], REQUEST_ID_SIZE)
            requireBytes(fields[NONCE_FIELD], NONCE_SIZE)
            requireBytes(fields[CONTEXT_FIELD], CONTEXT_SIZE)
            require(requireBytes(fields[PC_ID_FIELD], PairingProtocol.PC_ID_SIZE).contentEquals(pairing.pcId)) { "Autre PC" }
            require(requireBytes(fields[PAIRING_ID_FIELD], PairingProtocol.PAIRING_ID_SIZE).contentEquals(pairing.pairingId)) {
                "Autre association"
            }
            require(requireText(fields[ACCOUNT_SID_FIELD], MAX_SID_LENGTH).startsWith("S-1-")) { "SID invalide" }
            require(fields[ISSUED_AT_FIELD] is Cbor.Integer && fields[EXPIRES_AT_FIELD] is Cbor.Integer) { "Dates attendues" }
            requireText(fields[PC_NAME_FIELD], MAX_NAME_LENGTH)
            requireText(fields[ACCOUNT_NAME_FIELD], MAX_NAME_LENGTH)
            return AuthorizationRequest(fields)
        }

        private fun requireBytes(value: Cbor?, size: Int): ByteArray {
            val bytes = requireNotNull(value as? Cbor.ByteString) { "Champ binaire attendu" }.value
            require(bytes.size == size) { "Taille de champ inattendue" }
            return bytes
        }

        private fun requireText(value: Cbor?, maxLength: Int): String {
            val text = requireNotNull(value as? Cbor.Text) { "Texte attendu" }.value
            require(text.isNotEmpty() && text.length <= maxLength) { "Longueur de texte invalide" }
            return text
        }

        private fun displayText(value: Cbor): String = (value as Cbor.Text).value.filterNot(Char::isISOControl).trim()
    }
}
