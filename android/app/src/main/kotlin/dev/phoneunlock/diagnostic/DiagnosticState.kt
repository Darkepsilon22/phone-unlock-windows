package dev.phoneunlock.diagnostic

import dev.phoneunlock.security.BiometricStatus
import dev.phoneunlock.security.KeyProtection

data class DiagnosticState(
    val device: String = "",
    val biometric: BiometricStatus = BiometricStatus.UNSUPPORTED,
    val keyProtection: KeyProtection? = null,
    val lastResult: String? = null,
    val busy: Boolean = false,
) {
    val canCreateKey: Boolean get() = !busy && biometric == BiometricStatus.AVAILABLE
    val canUseKey: Boolean get() = !busy && keyProtection != null
}
