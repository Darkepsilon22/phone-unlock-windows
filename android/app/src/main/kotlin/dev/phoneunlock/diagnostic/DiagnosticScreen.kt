package dev.phoneunlock.diagnostic

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun DiagnosticScreen(controller: DiagnosticController) {
    val state by controller.state.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text("Diagnostic PhoneUnlock", style = MaterialTheme.typography.headlineSmall)
        StatusCard(state)
        Actions(state, controller)
        state.lastResult?.let { ResultCard(it) }
    }
}

@Composable
private fun StatusCard(state: DiagnosticState) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Appareil : ${state.device}")
            Text("Biométrie : ${state.biometric.label}")
            Text("Clé d'autorisation : ${state.keyProtection?.label ?: "absente"}")
        }
    }
}

@Composable
private fun Actions(state: DiagnosticState, controller: DiagnosticController) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(onClick = controller::createKey, enabled = state.canCreateKey, modifier = Modifier.fillMaxWidth()) {
            Text("Créer la clé d'autorisation")
        }
        Button(onClick = controller::signWithBiometric, enabled = state.canUseKey, modifier = Modifier.fillMaxWidth()) {
            Text("Signer avec l'empreinte")
        }
        OutlinedButton(onClick = controller::signWithoutBiometric, enabled = state.canUseKey, modifier = Modifier.fillMaxWidth()) {
            Text("Vérifier le refus sans empreinte")
        }
        OutlinedButton(onClick = controller::deleteKey, enabled = state.canUseKey, modifier = Modifier.fillMaxWidth()) {
            Text("Supprimer la clé")
        }
    }
}

@Composable
private fun ResultCard(result: String) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Text(result, modifier = Modifier.padding(16.dp))
    }
}
