package dev.phoneunlock.unlock

import android.security.keystore.KeyPermanentlyInvalidatedException
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import dev.phoneunlock.network.PcClient
import dev.phoneunlock.network.pinnedSocketFactory
import dev.phoneunlock.pairing.Endpoint
import dev.phoneunlock.pairing.PairingRecord
import dev.phoneunlock.pairing.PairingStore
import dev.phoneunlock.security.AuthorizationKey
import dev.phoneunlock.security.BiometricSigner
import dev.phoneunlock.security.EcdsaSignature
import dev.phoneunlock.security.SigningException
import dev.phoneunlock.security.SigningPrompt
import dev.phoneunlock.security.TransportKey
import java.io.IOException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

sealed interface UnlockProgress {
    data object Idle : UnlockProgress
    data class Working(val message: String) : UnlockProgress
    data object Succeeded : UnlockProgress
    data class Failed(val message: String) : UnlockProgress
}

class PendingRequest(val request: AuthorizationRequest, val code: String)

data class UnlockUiState(
    val status: PcStatus? = null,
    val reachable: Boolean = false,
    val searching: Boolean = true,
    val progress: UnlockProgress = UnlockProgress.Idle,
    val pending: PendingRequest? = null,
) {
    val canUnlock: Boolean
        get() = reachable && status?.unlockAvailable == true && progress !is UnlockProgress.Working
}

class UnlockController(private val activity: FragmentActivity) {

    private class Link(val record: PairingRecord, val watch: UnlockSession, val action: UnlockSession, val locator: PcLocator)

    private val store = PairingStore(activity.filesDir)
    private val transportKey = TransportKey()
    private val authorizationKey = AuthorizationKey.forDevice(activity.packageManager)
    private val signer = BiometricSigner(activity)
    private val mutableState = MutableStateFlow(UnlockUiState())

    @Volatile
    private var endpoint: Endpoint? = null

    @Volatile
    private var link: Link? = null

    @Volatile
    private var refusedRequestId: ByteArray? = null

    val state: StateFlow<UnlockUiState> = mutableState.asStateFlow()

    suspend fun watch(record: PairingRecord) {
        val current = withContext(Dispatchers.IO) { linkFor(record) }
        link = current
        endpoint = null
        var version = 0L
        while (true) {
            val target = endpoint ?: locate(current)
            if (target == null) {
                mutableState.update { it.copy(reachable = false, searching = false) }
                delay(RETRY_DELAY_MS)
                continue
            }
            try {
                val status = withContext(Dispatchers.IO) { current.watch.state(target, version) }
                version = status.version
                val pending = if (status.unlockAvailable) withContext(Dispatchers.IO) { pendingFor(current, target) } else null
                mutableState.update { it.copy(status = status, reachable = true, searching = false, pending = pending) }
            } catch (lost: IOException) {
                endpoint = null
                version = 0L
                mutableState.update { it.copy(reachable = false, searching = true) }
                delay(RETRY_DELAY_MS)
            } catch (refused: UnlockFailure) {
                mutableState.update { it.copy(reachable = false, searching = false) }
                delay(RETRY_DELAY_MS)
            }
        }
    }

    fun unlock() {
        val current = link ?: return
        val target = endpoint ?: return
        if (!mutableState.value.canUnlock) {
            return
        }
        authorize(current, target) { current.action.request(target) }
    }

    fun approvePending() {
        val current = link ?: return
        val target = endpoint ?: return
        val pending = mutableState.value.pending ?: return
        authorize(current, target) { pending.request }
    }

    fun refusePending() {
        refusedRequestId = mutableState.value.pending?.request?.requestId
        mutableState.update { it.copy(pending = null) }
    }

    private fun authorize(current: Link, target: Endpoint, obtain: () -> AuthorizationRequest) {
        if (mutableState.value.progress is UnlockProgress.Working) {
            return
        }
        mutableState.update { it.copy(pending = null) }
        activity.lifecycleScope.launch {
            try {
                showProgress("Demande au PC…")
                val request = withContext(Dispatchers.IO) { obtain() }
                showProgress("Confirme avec ton empreinte…")
                val prompt = SigningPrompt("Déverrouiller ${request.pcName}", "Compte Windows : ${request.accountName}")
                val signature = signer.sign(authorizationKey.signatureForOneOperation(), request.signedMessage(), prompt)
                showProgress("Ouverture de la session…")
                val outcome = withContext(Dispatchers.IO) {
                    current.action.prove(target, request, EcdsaSignature.derToRaw(signature))
                    awaitOutcome(current.action, target, request)
                }
                mutableState.update {
                    it.copy(progress = if (outcome == LOGON_SUCCEEDED) UnlockProgress.Succeeded else UnlockProgress.Failed(RESULT_UNKNOWN))
                }
            } catch (failure: Exception) {
                mutableState.update { it.copy(progress = UnlockProgress.Failed(describe(failure))) }
            }
        }
    }

    private fun pendingFor(current: Link, target: Endpoint): PendingRequest? {
        if (mutableState.value.progress is UnlockProgress.Working) {
            return null
        }
        val request = try {
            current.action.active(target)
        } catch (unreachable: IOException) {
            null
        } catch (refused: UnlockFailure) {
            null
        } catch (malformed: IllegalArgumentException) {
            null
        } ?: return null
        if (refusedRequestId?.contentEquals(request.requestId) == true) {
            return null
        }
        return PendingRequest(request, request.matchCode())
    }

    fun dismissResult() {
        mutableState.update { it.copy(progress = UnlockProgress.Idle) }
    }

    private fun awaitOutcome(session: UnlockSession, target: Endpoint, request: AuthorizationRequest): String {
        repeat(RESULT_POLLS) {
            val state = session.result(target, request)
            if (state != CONSUMED) {
                return state
            }
        }
        return CONSUMED
    }

    private fun linkFor(record: PairingRecord): Link {
        val factory = pinnedSocketFactory(transportKey, record.pcFingerprint)
        val watch = UnlockSession(PcClient(factory, LONG_POLL_READ_MS), record)
        val action = UnlockSession(PcClient(factory), record)
        return Link(record, watch, action, PcLocator { candidate -> responds(watch, candidate) })
    }

    private suspend fun locate(current: Link): Endpoint? {
        mutableState.update { it.copy(searching = true) }
        val found = current.locator.locate(current.record.endpoints) ?: return null
        endpoint = found
        if (found != current.record.endpoints.first()) {
            withContext(Dispatchers.IO) { store.save(current.record.withPreferredEndpoint(found)) }
        }
        return found
    }

    private fun responds(session: UnlockSession, candidate: Endpoint): Boolean = try {
        session.state(candidate, 0L)
        true
    } catch (unreachable: IOException) {
        false
    } catch (refused: UnlockFailure) {
        false
    }

    private fun showProgress(message: String) {
        mutableState.update { it.copy(progress = UnlockProgress.Working(message)) }
    }

    private fun describe(error: Exception): String = when (error) {
        is UnlockFailure -> when (error.code) {
            "WRONG_CONTEXT" -> "Le PC n'affiche plus l'écran de verrouillage de ce compte."
            "RATE_LIMITED" -> "Trop de demandes. Attends une minute."
            "EXPIRED", "CANCELLED" -> "Demande expirée ou remplacée. Recommence."
            "REVOKED" -> "Ce téléphone n'est plus associé à ce PC."
            "INVALID_PROOF" -> "Le PC a refusé la signature."
            "WINDOWS_REJECTED" -> "Windows a refusé le mot de passe enregistré. Relance provision sur le PC."
            else -> "Réponse du PC incompréhensible (${error.code})."
        }
        is SigningException -> "Empreinte non confirmée : ${error.message}"
        is KeyPermanentlyInvalidatedException -> "Les empreintes du téléphone ont changé. Refais l'association."
        is IOException -> "PC injoignable pendant le déverrouillage."
        else -> "Erreur inattendue : ${error.javaClass.simpleName} ${error.message.orEmpty()}"
    }

    private companion object {
        const val LONG_POLL_READ_MS = 35_000
        const val RETRY_DELAY_MS = 3_000L
        const val RESULT_POLLS = 3
        const val CONSUMED = "consumed"
        const val LOGON_SUCCEEDED = "logon_succeeded"
        const val RESULT_UNKNOWN = "Le PC n'a pas confirmé l'ouverture de la session."
    }
}
