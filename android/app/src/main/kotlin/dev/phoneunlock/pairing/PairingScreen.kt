package dev.phoneunlock.pairing

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.LifecycleOwner
import dev.phoneunlock.scanner.QrScanner
import dev.phoneunlock.unlock.UnlockController
import dev.phoneunlock.unlock.UnlockPanel

@Composable
fun PairingScreen(controller: PairingController, unlock: UnlockController, lifecycleOwner: LifecycleOwner) {
    val state by controller.state.collectAsState()

    BackHandler(enabled = state !is PairingState.Home && state !is PairingState.Confirming) { controller.showHome() }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text("PhoneUnlock", style = MaterialTheme.typography.headlineSmall)
        when (val current = state) {
            is PairingState.Home -> Home(current, controller::startScan, unlock, lifecycleOwner)
            is PairingState.Scanning -> Scanning(current, lifecycleOwner, controller::onQrScanned, controller::showHome)
            PairingState.Connecting -> Progress("Connexion au PC…")
            is PairingState.Comparing -> Comparing(current, controller::confirmCodesMatch, controller::rejectCodes)
            PairingState.Confirming -> Progress("Confirmation de l'association…")
            is PairingState.Paired -> Outcome("Association réussie avec ${current.pcName}.", "Terminer", controller::showHome)
            is PairingState.Failed -> Outcome(current.message, "Retour", controller::showHome)
        }
    }
}

@Composable
private fun Home(state: PairingState.Home, onPair: () -> Unit, unlock: UnlockController, lifecycleOwner: LifecycleOwner) {
    val pairing = state.pairing
    if (pairing != null) {
        UnlockPanel(unlock, pairing, lifecycleOwner)
    }
    if (pairing == null || !state.canPair) {
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (pairing == null) {
                    Text("Aucun PC associé.")
                }
                if (!state.canPair) {
                    Text("Empreinte indisponible : ${state.biometric.label}", color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
    state.keyProtection?.let { Text("Clé d'autorisation : $it", style = MaterialTheme.typography.bodySmall) }
    if (pairing == null) {
        Button(onClick = onPair, enabled = state.canPair, modifier = Modifier.fillMaxWidth()) {
            Text("Associer un PC")
        }
    } else {
        OutlinedButton(onClick = onPair, enabled = state.canPair, modifier = Modifier.fillMaxWidth()) {
            Text("Associer à nouveau")
        }
    }
}

@Composable
private fun Scanning(
    state: PairingState.Scanning,
    lifecycleOwner: LifecycleOwner,
    onScanned: (String) -> Unit,
    onCancel: () -> Unit,
) {
    Text("Sur le PC, lance PhoneUnlockService.exe pair en administrateur, puis scanne le QR code affiché.")
    state.hint?.let { Text(it, color = MaterialTheme.colorScheme.error) }
    QrScanner(lifecycleOwner, onScanned, Modifier.fillMaxWidth().aspectRatio(1f))
    OutlinedButton(onClick = onCancel, modifier = Modifier.fillMaxWidth()) {
        Text("Annuler")
    }
}

@Composable
private fun Comparing(state: PairingState.Comparing, onMatch: () -> Unit, onMismatch: () -> Unit) {
    Text("Vérifie que le PC affiche exactement le même code :")
    Text(
        state.code.chunked(CODE_GROUP).joinToString(" "),
        fontSize = 40.sp,
        fontFamily = FontFamily.Monospace,
        fontWeight = FontWeight.Bold,
        textAlign = TextAlign.Center,
        modifier = Modifier.fillMaxWidth(),
    )
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("PC : ${state.pcName}")
            Text("Compte Windows : ${state.accountName}")
        }
    }
    Text("1. Si le code est identique, réponds o sur le PC.\n2. Puis confirme ici avec ton empreinte.")
    state.hint?.let { Text(it, color = MaterialTheme.colorScheme.error) }
    Button(onClick = onMatch, modifier = Modifier.fillMaxWidth()) {
        Text("Le code est identique : confirmer")
    }
    OutlinedButton(onClick = onMismatch, modifier = Modifier.fillMaxWidth()) {
        Text("Le code est différent")
    }
}

@Composable
private fun Progress(message: String) {
    CircularProgressIndicator()
    Text(message)
}

@Composable
private fun Outcome(message: String, action: String, onAction: () -> Unit) {
    Text(message)
    Button(onClick = onAction, modifier = Modifier.fillMaxWidth()) {
        Text(action)
    }
}

private const val CODE_GROUP = 3
