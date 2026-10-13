package dev.phoneunlock.security

import java.security.KeyPairGenerator
import java.security.Signature
import java.security.spec.ECGenParameterSpec
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class EcdsaSignatureTest {

    @Test
    fun convertsRealSignaturesToSixtyFourBytes() {
        val keyPair = KeyPairGenerator.getInstance("EC").apply { initialize(ECGenParameterSpec("secp256r1")) }.generateKeyPair()
        repeat(50) {
            val der = Signature.getInstance("SHA256withECDSA").run {
                initSign(keyPair.private)
                update(byteArrayOf(it.toByte()))
                sign()
            }
            assertEquals(EcdsaSignature.RAW_SIZE, EcdsaSignature.derToRaw(der).size)
        }
    }

    @Test
    fun padsShortIntegersAndDropsSignPadding() {
        val r = ByteArray(32) { 0x7F }
        val s = byteArrayOf(0x00) + ByteArray(32) { 0x80.toByte() }
        val der = byteArrayOf(0x30, (2 + r.size + 2 + s.size).toByte(), 0x02, r.size.toByte()) + r +
            byteArrayOf(0x02, s.size.toByte()) + s

        val raw = EcdsaSignature.derToRaw(der)

        assertArrayEquals(r, raw.copyOfRange(0, 32))
        assertArrayEquals(ByteArray(32) { 0x80.toByte() }, raw.copyOfRange(32, 64))
    }

    @Test
    fun rejectsTrailingBytes() {
        val der = byteArrayOf(0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01, 0x00)
        assertThrows(IllegalArgumentException::class.java) { EcdsaSignature.derToRaw(der) }
    }

    @Test
    fun rejectsTruncatedInput() {
        assertThrows(IllegalArgumentException::class.java) { EcdsaSignature.derToRaw(byteArrayOf(0x30, 0x44, 0x02)) }
    }
}
