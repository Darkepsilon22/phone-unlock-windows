package dev.phoneunlock.security

import androidx.biometric.BiometricManager.Authenticators.BIOMETRIC_STRONG
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import java.security.Signature
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlinx.coroutines.suspendCancellableCoroutine

data class SigningPrompt(val title: String, val subtitle: String)

class SigningException(val errorCode: Int, message: String) : Exception(message) {
    companion object {
        const val MISSING_CRYPTO_OBJECT = -1
    }
}

class BiometricSigner(private val activity: FragmentActivity) {

    suspend fun sign(signature: Signature, payload: ByteArray, prompt: SigningPrompt): ByteArray =
        suspendCancellableCoroutine { continuation ->
            val callback = object : BiometricPrompt.AuthenticationCallback() {
                override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                    val authorized = result.cryptoObject?.signature
                    if (authorized == null) {
                        continuation.resumeWithException(
                            SigningException(SigningException.MISSING_CRYPTO_OBJECT, "Opération non liée à la biométrie"),
                        )
                        return
                    }
                    runCatching { authorized.update(payload); authorized.sign() }
                        .onSuccess { continuation.resume(it) }
                        .onFailure { continuation.resumeWithException(it) }
                }

                override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                    continuation.resumeWithException(SigningException(errorCode, errString.toString()))
                }
            }

            val biometricPrompt = BiometricPrompt(activity, ContextCompat.getMainExecutor(activity), callback)
            biometricPrompt.authenticate(promptInfo(prompt), BiometricPrompt.CryptoObject(signature))
            continuation.invokeOnCancellation { biometricPrompt.cancelAuthentication() }
        }

    private fun promptInfo(prompt: SigningPrompt): BiometricPrompt.PromptInfo =
        BiometricPrompt.PromptInfo.Builder()
            .setTitle(prompt.title)
            .setSubtitle(prompt.subtitle)
            .setNegativeButtonText(CANCEL_LABEL)
            .setAllowedAuthenticators(BIOMETRIC_STRONG)
            .setConfirmationRequired(true)
            .build()

    private companion object {
        const val CANCEL_LABEL = "Annuler"
    }
}
