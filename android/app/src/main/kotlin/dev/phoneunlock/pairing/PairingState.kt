package dev.phoneunlock.pairing

import dev.phoneunlock.security.BiometricStatus

sealed interface PairingState {
    data class Home(val pairing: PairingRecord?, val biometric: BiometricStatus, val keyProtection: String?) : PairingState {
        val canPair: Boolean get() = biometric == BiometricStatus.AVAILABLE
    }

    data class Scanning(val hint: String?) : PairingState

    data object Connecting : PairingState

    data class Comparing(val code: String, val pcName: String, val accountName: String, val hint: String?) : PairingState

    data object Confirming : PairingState

    data class Paired(val pcName: String) : PairingState

    data class Failed(val message: String) : PairingState
}
