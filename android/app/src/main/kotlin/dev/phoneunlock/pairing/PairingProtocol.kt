package dev.phoneunlock.pairing

import java.security.MessageDigest
import java.util.Locale

object PairingProtocol {
    const val VERSION = 1L
    const val PC_ID_SIZE = 16
    const val FINGERPRINT_SIZE = 32
    const val TOKEN_SIZE = 32
    const val PAIRING_ID_SIZE = 16
    private const val TRANSCRIPT_DOMAIN = "PhoneUnlock-v1-pair"
    private const val CONFIRMATION_DOMAIN = "PhoneUnlock-v1-pair-confirm"
    private const val CODE_MODULUS = 1_000_000L
    private const val CODE_BYTES = 4

    fun fingerprint(subjectPublicKeyInfo: ByteArray): ByteArray = sha256(subjectPublicKeyInfo)

    fun transcript(
        pcId: ByteArray,
        pcFingerprint: ByteArray,
        transportFingerprint: ByteArray,
        authorizationFingerprint: ByteArray,
        token: ByteArray,
        pairingId: ByteArray,
    ): ByteArray = sha256(
        domain(TRANSCRIPT_DOMAIN) + pcId + pcFingerprint + transportFingerprint + authorizationFingerprint + token + pairingId,
    )

    fun comparisonCode(transcript: ByteArray): String {
        val prefix = transcript.take(CODE_BYTES).fold(0L) { value, byte -> (value shl Byte.SIZE_BITS) or (byte.toLong() and 0xFF) }
        return String.format(Locale.ROOT, "%06d", prefix % CODE_MODULUS)
    }

    fun confirmationMessage(transcript: ByteArray): ByteArray = domain(CONFIRMATION_DOMAIN) + transcript

    private fun domain(name: String): ByteArray = name.toByteArray(Charsets.US_ASCII) + 0.toByte()

    private fun sha256(bytes: ByteArray): ByteArray = MessageDigest.getInstance("SHA-256").digest(bytes)
}
