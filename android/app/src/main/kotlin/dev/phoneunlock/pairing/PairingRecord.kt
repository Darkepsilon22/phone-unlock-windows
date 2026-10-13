package dev.phoneunlock.pairing

import dev.phoneunlock.encoding.Cbor

class PairingRecord(
    val pcId: ByteArray,
    val pcFingerprint: ByteArray,
    val pairingId: ByteArray,
    val endpoints: List<Endpoint>,
    val pcName: String,
    val accountName: String,
) {
    fun encode(): ByteArray = Cbor.encode(
        Cbor.Object(
            mapOf(
                PC_ID_FIELD to Cbor.ByteString(pcId),
                FINGERPRINT_FIELD to Cbor.ByteString(pcFingerprint),
                PAIRING_ID_FIELD to Cbor.ByteString(pairingId),
                ENDPOINTS_FIELD to Cbor.Items(endpoints.map(Endpoint::toCbor)),
                PC_NAME_FIELD to Cbor.Text(pcName),
                ACCOUNT_NAME_FIELD to Cbor.Text(accountName),
            ),
        ),
    )

    companion object {
        private const val PC_ID_FIELD = 1L
        private const val FINGERPRINT_FIELD = 2L
        private const val PAIRING_ID_FIELD = 3L
        private const val ENDPOINTS_FIELD = 4L
        private const val PC_NAME_FIELD = 5L
        private const val ACCOUNT_NAME_FIELD = 6L

        fun decode(bytes: ByteArray): PairingRecord? = try {
            val fields = requireNotNull(Cbor.decode(bytes) as? Cbor.Object) { "Association mal formée" }
            PairingRecord(
                pcId = fixedBytes(fields[PC_ID_FIELD], PairingProtocol.PC_ID_SIZE),
                pcFingerprint = fixedBytes(fields[FINGERPRINT_FIELD], PairingProtocol.FINGERPRINT_SIZE),
                pairingId = fixedBytes(fields[PAIRING_ID_FIELD], PairingProtocol.PAIRING_ID_SIZE),
                endpoints = Endpoint.listFromCbor(fields[ENDPOINTS_FIELD], PairingQr.MAX_ENDPOINTS),
                pcName = requireNotNull(fields[PC_NAME_FIELD] as? Cbor.Text) { "Nom du PC attendu" }.value,
                accountName = requireNotNull(fields[ACCOUNT_NAME_FIELD] as? Cbor.Text) { "Nom du compte attendu" }.value,
            )
        } catch (invalid: IllegalArgumentException) {
            null
        }
    }
}
