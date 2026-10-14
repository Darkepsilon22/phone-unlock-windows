package dev.phoneunlock.unlock

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import dev.phoneunlock.pairing.PairingRecord

@Composable
fun UnlockPanel(controller: UnlockController, record: PairingRecord, lifecycleOwner: LifecycleOwner) {
    val state by controller.state.collectAsState()

    LaunchedEffect(record) {
        lifecycleOwner.repeatOnLifecycle(Lifecycle.State.STARTED) { controller.watch(record) }
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(state.status?.pcName ?: record.pcName, fontWeight = FontWeight.Bold)
            Text("Compte Windows : ${record.accountName}")
            Text(statusLabel(state), style = MaterialTheme.typography.titleMedium)
        }
    }

    when (val progress = state.progress) {
        is UnlockProgress.Working -> {
            CircularProgressIndicator()
            Text(progress.message)
        }
        UnlockProgress.Succeeded -> Outcome("Session ouverte.", controller::dismissResult)
        is UnlockProgress.Failed -> Outcome(progress.message, controller::dismissResult)
        UnlockProgress.Idle -> Unit
    }

    Button(onClick = controller::unlock, enabled = state.canUnlock, modifier = Modifier.fillMaxWidth()) {
        Text("Déverrouiller")
    }
}

@Composable
private fun Outcome(message: String, onDismiss: () -> Unit) {
    Text(message)
    OutlinedButton(onClick = onDismiss, modifier = Modifier.fillMaxWidth()) {
        Text("OK")
    }
}

private fun statusLabel(state: UnlockUiState): String = when {
    state.reachable -> when (state.status?.session) {
        SessionState.LOCKED -> if (state.status.unlockAvailable) "Verrouillé" else "Verrouillé (écran en veille ?)"
        SessionState.LOGON_SCREEN -> if (state.status.unlockAvailable) "Écran de connexion" else "Démarrage en cours"
        SessionState.SIGNED_IN -> "Session ouverte"
        else -> "État inconnu"
    }
    state.searching -> "Recherche du PC…"
    else -> "PC injoignable"
}
