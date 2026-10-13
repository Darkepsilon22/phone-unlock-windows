package dev.phoneunlock

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.fragment.app.FragmentActivity
import dev.phoneunlock.pairing.PairingController
import dev.phoneunlock.pairing.PairingScreen

class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val controller = PairingController(this)
        setContent {
            MaterialTheme {
                Surface {
                    PairingScreen(controller, this)
                }
            }
        }
    }
}
