package dev.phoneunlock.diagnostic

import android.os.Build
import android.os.SystemClock
import android.security.keystore.KeyPermanentlyInvalidatedException
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import dev.phoneunlock.security.AuthorizationKey
import dev.phoneunlock.security.BiometricSigner
import dev.phoneunlock.security.BiometricStatus
import dev.phoneunlock.security.EcdsaSignature
import dev.phoneunlock.security.SigningException
import dev.phoneunlock.security.SigningPrompt
import java.security.SecureRandom
import java.security.Signature
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

class DiagnosticController(private val activity: FragmentActivity) {

    private val key = AuthorizationKey.forDevice(activity.packageManager)
    private val signer = BiometricSigner(activity)
    private val random = SecureRandom()
    private val mutableState = MutableStateFlow(DiagnosticState())

    val state: StateFlow<DiagnosticState> = mutableState.asStateFlow()

    fun refresh() {
        mutableState.update {
            it.copy(
                device = "${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})",
                biometric = BiometricStatus.of(activity),
                keyProtection = if (key.exists()) key.protection() else null,
            )
        }
    }

    fun createKey() = run("Création de la clé") {
        val protection = key.create()
        "Clé créée : ${protection.label}"
    }

    fun deleteKey() = run("Suppression de la clé") {
        key.delete()
        "Clé supprimée"
    }

    fun signWithBiometric() = run("Signature biométrique") {
        val payload = testPayload()
        val startedAt = SystemClock.elapsedRealtime()
        val der = signer.sign(key.signatureForOneOperation(), payload, PROMPT)
        val elapsed = SystemClock.elapsedRealtime() - startedAt
        val raw = EcdsaSignature.derToRaw(der)
        val verdict = if (verify(payload, der)) "valide" else "INVALIDE"
        "Signature $verdict, ${raw.size} octets r‖s, $elapsed ms"
    }

    fun signWithoutBiometric() = run("Signature sans biométrie") {
        val refused = runCatching {
            key.signatureForOneOperation().apply { update(testPayload()) }.sign()
        }.isFailure
        if (refused) "Refusée comme attendu : la clé exige l'empreinte à chaque opération"
        else "ANOMALIE : signature obtenue sans biométrie"
    }

    private fun run(action: String, block: suspend () -> String) {
        mutableState.update { it.copy(busy = true, lastResult = "$action…") }
        activity.lifecycleScope.launch {
            val result = runCatching { block() }.getOrElse { describe(it) }
            mutableState.update { it.copy(busy = false, lastResult = result) }
            refresh()
        }
    }

    private fun describe(error: Throwable): String = when (error) {
        is SigningException -> "Biométrie interrompue : ${error.message} (code ${error.errorCode})"
        is KeyPermanentlyInvalidatedException -> "Clé invalidée par un changement d'empreinte : nouvelle association requise"
        else -> "Erreur : ${error.javaClass.simpleName} ${error.message.orEmpty()}"
    }

    private fun testPayload(): ByteArray =
        DIAGNOSTIC_DOMAIN.toByteArray() + byteArrayOf(0) + ByteArray(NONCE_SIZE).also(random::nextBytes)

    private fun verify(payload: ByteArray, der: ByteArray): Boolean {
        val publicKey = key.publicKey() ?: return false
        return Signature.getInstance(AuthorizationKey.SIGNATURE_ALGORITHM).run {
            initVerify(publicKey)
            update(payload)
            verify(der)
        }
    }

    private companion object {
        const val DIAGNOSTIC_DOMAIN = "PhoneUnlock-v1-diagnostic"
        const val NONCE_SIZE = 32
        val PROMPT = SigningPrompt("Test de signature", "Pose ton doigt pour signer un message de test")
    }
}
