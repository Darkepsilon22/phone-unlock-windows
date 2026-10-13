package dev.phoneunlock.network

import dev.phoneunlock.pairing.PairingProtocol
import java.security.cert.CertificateException
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import org.junit.Assert.assertThrows
import org.junit.Test

class PinnedServerTrustTest {

    private val pc = certificate("pc.crt")
    private val other = certificate("other.crt")
    private val trust = PinnedServerTrust(PairingProtocol.fingerprint(pc.publicKey.encoded))

    @Test
    fun acceptsTheExpectedPc() {
        trust.checkServerTrusted(arrayOf(pc), AUTH_TYPE)
    }

    @Test
    fun rejectsAnotherPc() {
        assertThrows(CertificateException::class.java) { trust.checkServerTrusted(arrayOf(other), AUTH_TYPE) }
    }

    @Test
    fun onlyTheLeafCertificateCounts() {
        assertThrows(CertificateException::class.java) { trust.checkServerTrusted(arrayOf(other, pc), AUTH_TYPE) }
    }

    @Test
    fun rejectsMissingCertificate() {
        assertThrows(CertificateException::class.java) { trust.checkServerTrusted(emptyArray(), AUTH_TYPE) }
        assertThrows(CertificateException::class.java) { trust.checkServerTrusted(null, AUTH_TYPE) }
    }

    @Test
    fun neverTrustsIncomingClients() {
        assertThrows(CertificateException::class.java) { trust.checkClientTrusted(arrayOf(pc), AUTH_TYPE) }
    }

    private fun certificate(name: String): X509Certificate =
        checkNotNull(javaClass.getResourceAsStream("/pinning/$name")).use {
            CertificateFactory.getInstance("X.509").generateCertificate(it) as X509Certificate
        }

    private companion object {
        const val AUTH_TYPE = "ECDHE_ECDSA"
    }
}
