package dev.phoneunlock.security

import android.content.Context
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricManager.Authenticators.BIOMETRIC_STRONG

enum class BiometricStatus(val label: String) {
    AVAILABLE("Biométrie forte disponible"),
    NONE_ENROLLED("Aucune empreinte enregistrée"),
    NO_HARDWARE("Aucun capteur biométrique fort"),
    UNAVAILABLE("Capteur momentanément indisponible"),
    SECURITY_UPDATE_REQUIRED("Mise à jour de sécurité requise"),
    UNSUPPORTED("Biométrie forte non prise en charge");

    companion object {
        fun of(context: Context): BiometricStatus =
            when (BiometricManager.from(context).canAuthenticate(BIOMETRIC_STRONG)) {
                BiometricManager.BIOMETRIC_SUCCESS -> AVAILABLE
                BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED -> NONE_ENROLLED
                BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE -> NO_HARDWARE
                BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE -> UNAVAILABLE
                BiometricManager.BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED -> SECURITY_UPDATE_REQUIRED
                else -> UNSUPPORTED
            }
    }
}
