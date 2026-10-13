package dev.phoneunlock.security

import android.content.pm.PackageManager
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyInfo
import android.security.keystore.KeyProperties
import android.security.keystore.StrongBoxUnavailableException
import java.security.KeyFactory
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.PrivateKey
import java.security.PublicKey
import java.security.Signature
import java.security.spec.ECGenParameterSpec

class AuthorizationKey(private val strongBoxAvailable: Boolean) {

    fun exists(): Boolean = keyStore().containsAlias(ALIAS)

    fun create(): KeyProtection {
        delete()
        try {
            generate(useStrongBox = strongBoxAvailable)
        } catch (unavailable: StrongBoxUnavailableException) {
            generate(useStrongBox = false)
        }
        return protection()
    }

    fun delete() {
        val store = keyStore()
        if (store.containsAlias(ALIAS)) {
            store.deleteEntry(ALIAS)
        }
    }

    fun protection(): KeyProtection {
        val key = privateKey() ?: return KeyProtection.UNKNOWN
        val info = KeyFactory.getInstance(key.algorithm, PROVIDER).getKeySpec(key, KeyInfo::class.java)
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            protectionFromSecurityLevel(info.securityLevel)
        } else {
            legacyProtection(info)
        }
    }

    fun signatureForOneOperation(): Signature {
        val key = privateKey() ?: throw IllegalStateException("Clé d'autorisation absente")
        return Signature.getInstance(SIGNATURE_ALGORITHM).apply { initSign(key) }
    }

    fun publicKey(): PublicKey? = keyStore().getCertificate(ALIAS)?.publicKey

    private fun privateKey(): PrivateKey? = keyStore().getKey(ALIAS, null) as PrivateKey?

    private fun generate(useStrongBox: Boolean) {
        val spec = KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_SIGN)
            .setAlgorithmParameterSpec(ECGenParameterSpec(CURVE))
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setUserAuthenticationRequired(true)
            .setUserAuthenticationParameters(PER_OPERATION, KeyProperties.AUTH_BIOMETRIC_STRONG)
            .setInvalidatedByBiometricEnrollment(true)
            .setIsStrongBoxBacked(useStrongBox)
            .build()
        KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, PROVIDER)
            .apply { initialize(spec) }
            .generateKeyPair()
    }

    private fun protectionFromSecurityLevel(level: Int): KeyProtection = when (level) {
        KeyProperties.SECURITY_LEVEL_STRONGBOX -> KeyProtection.STRONGBOX
        KeyProperties.SECURITY_LEVEL_TRUSTED_ENVIRONMENT -> KeyProtection.TRUSTED_ENVIRONMENT
        KeyProperties.SECURITY_LEVEL_SOFTWARE -> KeyProtection.SOFTWARE
        else -> KeyProtection.UNKNOWN
    }

    @Suppress("DEPRECATION")
    private fun legacyProtection(info: KeyInfo): KeyProtection =
        if (info.isInsideSecureHardware) KeyProtection.TRUSTED_ENVIRONMENT else KeyProtection.SOFTWARE

    private fun keyStore(): KeyStore = KeyStore.getInstance(PROVIDER).apply { load(null) }

    companion object {
        const val SIGNATURE_ALGORITHM = "SHA256withECDSA"
        private const val PROVIDER = "AndroidKeyStore"
        private const val ALIAS = "phoneunlock.authorization"
        private const val CURVE = "secp256r1"
        private const val PER_OPERATION = 0

        fun forDevice(packageManager: PackageManager) =
            AuthorizationKey(packageManager.hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE))
    }
}
