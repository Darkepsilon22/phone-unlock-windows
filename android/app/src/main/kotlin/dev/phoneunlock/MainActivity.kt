package dev.phoneunlock

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.fragment.app.FragmentActivity
import dev.phoneunlock.diagnostic.DiagnosticController
import dev.phoneunlock.diagnostic.DiagnosticScreen

class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val controller = DiagnosticController(this)
        setContent {
            MaterialTheme {
                Surface {
                    DiagnosticScreen(controller)
                }
            }
        }
        controller.refresh()
    }
}
