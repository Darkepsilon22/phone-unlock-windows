package dev.phoneunlock.pairing

import android.os.Build
import android.provider.Settings
import android.security.keystore.KeyPermanentlyInvalidatedException
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import dev.phoneunlock.network.ClientIdentity
import dev.phoneunlock.network.PcClient
import dev.phoneunlock.network.PinnedServerTrust
import dev.phoneunlock.network.Tls13SocketFactory
import dev.phoneunlock.security.AuthorizationKey
import dev.phoneunlock.security.BiometricSigner
import dev.phoneunlock.security.BiometricStatus
import dev.phoneunlock.security.EcdsaSignature
import dev.phoneunlock.security.SigningException
import dev.phoneunlock.security.SigningPrompt
import dev.phoneunlock.security.TransportKey
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class PairingController(private val activity: FragmentActivity) {

    private class Session(val qr: PairingQr, val exchange: PairingExchange, val challenge: PairingChallenge)

    private val store = PairingStore(activity.filesDir)
    private val authorizationKey = AuthorizationKey.forDevice(activity.packageManager)
    private val transportKey = TransportKey()
    private val signer = BiometricSigner(activity)
    private val mutableState = MutableStateFlow<PairingState>(homeState())
    private var session: Session? = null

    val state: StateFlow<PairingState> = mutableState.asStateFlow()

    fun showHome() {
        session = null
        mutableState.value = homeState()
    }

    fun startScan() {
        mutableState.value = PairingState.Scanning(null)
    }

    fun onQrScanned(text: String) {
        if (mutableState.value !is PairingState.Scanning) {
            return
        }
        val qr = PairingQr.parse(text)
        if (qr == null) {
            mutableState.value = PairingState.Scanning(UNKNOWN_QR)
            return
        }
        mutableState.value = PairingState.Connecting
        activity.lifecycleScope.launch {
            try {
                val exchange = withContext(Dispatchers.IO) { prepareExchange(qr) }
                val challenge = withContext(Dispatchers.IO) { exchange.requestChallenge(phoneName()) }
                session = Session(qr, exchange, challenge)
                mutableState.value = PairingState.Comparing(challenge.code, challenge.pcName, challenge.accountName, null)
            } catch (failure: Exception) {
                fail(failure)
            }
        }
    }

    fun confirmCodesMatch() {
        val current = session ?: return
        val comparing = mutableState.value as? PairingState.Comparing ?: return
        mutableState.value = PairingState.Confirming
        activity.lifecycleScope.launch {
            try {
                val message = PairingProtocol.confirmationMessage(current.challenge.transcript)
                val signature = signer.sign(authorizationKey.signatureForOneOperation(), message, PROMPT)
                withContext(Dispatchers.IO) {
                    current.exchange.confirm(current.challenge, EcdsaSignature.derToRaw(signature))
                    store.save(recordOf(current))
                }
                session = null
                mutableState.value = PairingState.Paired(current.challenge.pcName)
            } catch (notConfirmed: PairingFailure) {
                if (notConfirmed.reason == PairingFailure.Reason.PC_NOT_CONFIRMED) {
                    mutableState.value = comparing.copy(hint = CONFIRM_ON_PC_FIRST)
                } else {
                    fail(notConfirmed)
                }
            } catch (cancelled: SigningException) {
                mutableState.value = comparing.copy(hint = "Empreinte non confirmée : ${cancelled.message}")
            } catch (failure: Exception) {
                fail(failure)
            }
        }
    }

    fun rejectCodes() {
        session = null
        mutableState.value = PairingState.Failed(CODES_DIFFER)
    }

    private fun homeState(): PairingState.Home {
        val keyProtection = if (authorizationKey.exists()) authorizationKey.protection().label else null
        return PairingState.Home(store.load(), BiometricStatus.of(activity), keyProtection)
    }

    private fun prepareExchange(qr: PairingQr): PairingExchange {
        transportKey.ensureExists()
        ensureAuthorizationKey()
        val certificate = transportKey.certificate()
        val socketFactory = Tls13SocketFactory.create(
            ClientIdentity(transportKey.privateKey(), certificate),
            PinnedServerTrust(qr.pcFingerprint),
        )
        val authorizationSpki = checkNotNull(authorizationKey.publicKey()) { "Clé d'autorisation absente" }.encoded
        return PairingExchange(qr, PcClient(socketFactory), certificate.publicKey.encoded, authorizationSpki)
    }

    private fun ensureAuthorizationKey() {
        val usable = authorizationKey.exists() && try {
            authorizationKey.signatureForOneOperation()
            true
        } catch (invalidated: KeyPermanentlyInvalidatedException) {
            false
        }
        if (!usable) {
            authorizationKey.create()
        }
    }

    private fun recordOf(session: Session) = PairingRecord(
        pcId = session.qr.pcId,
        pcFingerprint = session.qr.pcFingerprint,
        pairingId = session.challenge.pairingId,
        endpoints = session.qr.endpoints,
        pcName = session.challenge.pcName,
        accountName = session.challenge.accountName,
    )

    private fun phoneName(): String {
        val name = Settings.Global.getString(activity.contentResolver, Settings.Global.DEVICE_NAME)
            ?.filterNot(Char::isISOControl)
            ?.trim()
        return (if (name.isNullOrEmpty()) Build.MODEL else name).take(MAX_PHONE_NAME_LENGTH)
    }

    private fun fail(error: Exception) {
        session = null
        mutableState.value = PairingState.Failed(describe(error))
    }

    private fun describe(error: Exception): String = when (error) {
        is PairingFailure -> when (error.reason) {
            PairingFailure.Reason.PC_UNREACHABLE ->
                "PC injoignable. Vérifie que le téléphone et le PC sont sur le même Wi-Fi ou le même partage de connexion."
            PairingFailure.Reason.WRONG_PC -> "Ce n'est pas le PC du QR code. Association interrompue sans rien envoyer."
            PairingFailure.Reason.EXPIRED -> "Le QR code a expiré ou l'association a été refusée sur le PC. Relance pair sur le PC."
            PairingFailure.Reason.REFUSED -> "Le PC a refusé la demande. Relance pair sur le PC et scanne le nouveau QR code."
            PairingFailure.Reason.PC_NOT_CONFIRMED -> CONFIRM_ON_PC_FIRST
            PairingFailure.Reason.INVALID_RESPONSE -> "Réponse du PC incompréhensible."
        }
        is KeyPermanentlyInvalidatedException -> "Les empreintes du téléphone ont changé. Recommence l'association."
        else -> "Erreur inattendue : ${error.javaClass.simpleName} ${error.message.orEmpty()}"
    }

    private companion object {
        const val MAX_PHONE_NAME_LENGTH = 32
        const val UNKNOWN_QR = "Ce QR code n'est pas un QR d'association PhoneUnlock."
        const val CONFIRM_ON_PC_FIRST = "Réponds d'abord o sur le PC, puis confirme à nouveau ici."
        const val CODES_DIFFER = "Association annulée. Réponds n sur le PC : un autre appareil a peut-être intercepté la connexion."
        val PROMPT = SigningPrompt("Associer ce téléphone", "Pose ton doigt pour confirmer l'association avec le PC")
    }
}
