package dev.phoneunlock.encoding

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class CborTest {

    @Test
    fun encodesShortestIntegerHeads() {
        assertArrayEquals(bytes(0x17), Cbor.encode(Cbor.Integer(23)))
        assertArrayEquals(bytes(0x18, 0x18), Cbor.encode(Cbor.Integer(24)))
        assertArrayEquals(bytes(0x19, 0x01, 0x00), Cbor.encode(Cbor.Integer(256)))
        assertArrayEquals(bytes(0x1A, 0x00, 0x01, 0x00, 0x00), Cbor.encode(Cbor.Integer(65536)))
    }

    @Test
    fun sortsMapKeys() {
        val map = Cbor.Object(linkedMapOf(10L to Cbor.Integer(1), 2L to Cbor.Flag(true)))
        assertArrayEquals(bytes(0xA2, 0x02, 0xF5, 0x0A, 0x01), Cbor.encode(map))
    }

    @Test
    fun roundTripsNestedValues() {
        val original = Cbor.Object(
            mapOf(
                1L to Cbor.Integer(1),
                2L to Cbor.ByteString(bytes(1, 2, 3)),
                3L to Cbor.Text("PC de test é"),
                4L to Cbor.Items(listOf(Cbor.Text("10.0.2.15"), Cbor.Integer(47863))),
            ),
        )
        val encoded = Cbor.encode(original)
        assertEquals(original, Cbor.decode(encoded))
        assertArrayEquals(encoded, Cbor.encode(Cbor.decode(encoded)))
    }

    @Test
    fun rejectsNonShortestHeads() {
        assertRejected(bytes(0x18, 0x17))
        assertRejected(bytes(0x19, 0x00, 0xFF))
    }

    @Test
    fun rejectsUnsortedOrDuplicateKeys() {
        assertRejected(bytes(0xA2, 0x02, 0x01, 0x01, 0x01))
        assertRejected(bytes(0xA2, 0x01, 0x01, 0x01, 0x02))
    }

    @Test
    fun rejectsIndefiniteLengthsAndTrailingBytes() {
        assertRejected(bytes(0x5F, 0x41, 0x00, 0xFF))
        assertRejected(bytes(0x01, 0x02))
    }

    @Test
    fun rejectsTruncatedInput() {
        assertRejected(bytes(0x43, 0x01, 0x02))
        assertRejected(bytes(0xA1, 0x01))
        assertRejected(bytes())
    }

    @Test
    fun rejectsTextKeysNegativeIntegersTagsAndInvalidUtf8() {
        assertRejected(bytes(0xA1, 0x61, 0x61, 0x01))
        assertRejected(bytes(0x20))
        assertRejected(bytes(0xC1, 0x01))
        assertRejected(bytes(0x62, 0xC3, 0x28))
    }

    @Test
    fun rejectsExcessiveNesting() {
        assertRejected(ByteArray(10) { 0x81.toByte() } + bytes(0x01))
    }

    private fun assertRejected(encoded: ByteArray) {
        assertThrows(IllegalArgumentException::class.java) { Cbor.decode(encoded) }
    }

    private fun bytes(vararg values: Int): ByteArray = ByteArray(values.size) { values[it].toByte() }
}
