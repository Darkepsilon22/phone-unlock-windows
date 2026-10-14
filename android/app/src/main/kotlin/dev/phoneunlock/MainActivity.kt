package dev.phoneunlock

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.fragment.app.FragmentActivity
import dev.phoneunlock.pairing.PairingController
import dev.phoneunlock.pairing.PairingScreen
import dev.phoneunlock.unlock.UnlockController

class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val pairing = PairingController(this)
        val unlock = UnlockController(this)
        setContent {
            MaterialTheme {
                Surface {
                    PairingScreen(pairing, unlock, this)
                }
            }
        }
    }
}
