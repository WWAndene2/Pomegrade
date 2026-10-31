package me.magnum.melonds.impl.diagnostics

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.core.app.RemoteInput
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import me.magnum.melonds.R

/** The pairing code typed in [WirelessDebugPairingNotification]: pairs, connects, and shows the result there. */
class WirelessDebugPairingReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        val code = RemoteInput.getResultsFromIntent(intent)?.getCharSequence(WirelessDebugPairingNotification.KEY_CODE)?.toString()
        if (code.isNullOrBlank()) return
        val appContext = context.applicationContext
        val pending = goAsync()
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val connected = WirelessDebugDiagnostics.pairAndConnect(appContext, code)
                WirelessDebugPairingNotification.show(
                    appContext,
                    appContext.getString(if (connected) R.string.wireless_debug_paired else R.string.wireless_debug_pairing_failed),
                )
            } finally {
                pending.finish()
            }
        }
    }
}
