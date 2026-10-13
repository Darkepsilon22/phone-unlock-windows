package dev.phoneunlock.security

object EcdsaSignature {
    const val RAW_SIZE = 64
    private const val COORDINATE_SIZE = RAW_SIZE / 2
    private const val SEQUENCE_TAG = 0x30
    private const val INTEGER_TAG = 0x02
    private const val LONG_FORM_FLAG = 0x80

    fun derToRaw(der: ByteArray): ByteArray {
        val reader = DerReader(der)
        val sequenceLength = reader.readHeader(SEQUENCE_TAG)
        require(sequenceLength == reader.remaining) { "Longueur de séquence incohérente" }
        val r = reader.readValue(INTEGER_TAG)
        val s = reader.readValue(INTEGER_TAG)
        require(reader.remaining == 0) { "Données en trop après la signature" }
        return toFixedSize(r) + toFixedSize(s)
    }

    private fun toFixedSize(integer: ByteArray): ByteArray {
        val magnitude = integer.dropWhile { it == 0.toByte() }.toByteArray()
        require(magnitude.isNotEmpty() && magnitude.size <= COORDINATE_SIZE) { "Entier hors limites" }
        return ByteArray(COORDINATE_SIZE - magnitude.size) + magnitude
    }

    private class DerReader(private val bytes: ByteArray) {
        private var position = 0

        val remaining: Int get() = bytes.size - position

        fun readHeader(expectedTag: Int): Int {
            require(remaining >= 2) { "Signature tronquée" }
            require(next() == expectedTag) { "Balise DER inattendue" }
            val length = next()
            require(length and LONG_FORM_FLAG == 0) { "Longueur DER non prise en charge" }
            require(length <= remaining) { "Signature tronquée" }
            return length
        }

        fun readValue(expectedTag: Int): ByteArray {
            val length = readHeader(expectedTag)
            return bytes.copyOfRange(position, position + length).also { position += length }
        }

        private fun next(): Int = bytes[position++].toInt() and 0xFF
    }
}
