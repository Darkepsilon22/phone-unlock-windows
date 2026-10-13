package dev.phoneunlock.security

import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.math.BigInteger
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.PrivateKey
import java.security.cert.X509Certificate
import java.security.spec.ECGenParameterSpec
import java.util.Date
import java.util.concurrent.TimeUnit
import javax.security.auth.x500.X500Principal

class TransportKey {

    fun ensureExists() {
        if (!keyStore().containsAlias(ALIAS)) {
            generate()
        }
    }

    fun privateKey(): PrivateKey = checkNotNull(keyStore().getKey(ALIAS, null) as PrivateKey?) { "Clé de transport absente" }

    fun certificate(): X509Certificate =
        checkNotNull(keyStore().getCertificate(ALIAS) as X509Certificate?) { "Certificat de transport absent" }

    private fun generate() {
        val now = System.currentTimeMillis()
        val spec = KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_SIGN)
            .setAlgorithmParameterSpec(ECGenParameterSpec(CURVE))
            .setDigests(KeyProperties.DIGEST_NONE, KeyProperties.DIGEST_SHA256)
            .setCertificateSubject(X500Principal(SUBJECT))
            .setCertificateSerialNumber(BigInteger.ONE)
            .setCertificateNotBefore(Date(now - TimeUnit.DAYS.toMillis(1)))
            .setCertificateNotAfter(Date(now + TimeUnit.DAYS.toMillis(VALIDITY_DAYS)))
            .build()
        KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, PROVIDER)
            .apply { initialize(spec) }
            .generateKeyPair()
    }

    private fun keyStore(): KeyStore = KeyStore.getInstance(PROVIDER).apply { load(null) }

    private companion object {
        const val PROVIDER = "AndroidKeyStore"
        const val ALIAS = "phoneunlock.transport"
        const val CURVE = "secp256r1"
        const val SUBJECT = "CN=PhoneUnlock Phone"
        const val VALIDITY_DAYS = 3650L
    }
}
