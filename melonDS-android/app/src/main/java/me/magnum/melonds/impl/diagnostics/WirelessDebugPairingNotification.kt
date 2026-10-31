package me.magnum.melonds.impl.diagnostics

import android.Manifest
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.app.RemoteInput
import androidx.core.content.ContextCompat
import me.magnum.melonds.R

/**
 * The notification the pairing code is typed into, as Shizuku does: Android's code dialog closes when another app
 * opens, so the code is answered from the notification shade over it ([WirelessDebugPairingReceiver]).
 */
object WirelessDebugPairingNotification {

    const val KEY_CODE = "wireless_debug_pairing_code"
    private const val CHANNEL_ID = "wireless_debug_pairing"
    private const val NOTIFICATION_ID = 0x5044 // "PD"

    fun canPost(context: Context): Boolean =
        ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    /** Asks for the code; [show] again with a [message] for the result. */
    fun show(context: Context, message: CharSequence? = null) {
        if (!canPost(context)) return
        val manager = NotificationManagerCompat.from(context)
        manager.createNotificationChannel(
            NotificationChannelCompat.Builder(CHANNEL_ID, NotificationManagerCompat.IMPORTANCE_HIGH)
                .setName(context.getString(R.string.wireless_debug_notification_channel))
                .build()
        )
        val builder = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_settings)
            .setContentTitle(context.getString(R.string.wireless_debug_pairing_title))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setOnlyAlertOnce(true)
        if (message == null) {
            val input = RemoteInput.Builder(KEY_CODE)
                .setLabel(context.getString(R.string.wireless_debug_pairing_code))
                .build()
            val reply = PendingIntent.getBroadcast(
                context,
                0,
                Intent(context, WirelessDebugPairingReceiver::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE,
            )
            val action = NotificationCompat.Action.Builder(R.drawable.ic_settings, context.getString(R.string.wireless_debug_pairing_code), reply)
                .addRemoteInput(input)
                .build()
            builder.setContentText(context.getString(R.string.wireless_debug_pairing_text))
                .setStyle(NotificationCompat.BigTextStyle().bigText(context.getString(R.string.wireless_debug_pairing_text)))
                .addAction(action)
                .setOngoing(true)
        } else {
            builder.setContentText(message).setAutoCancel(true)
        }
        manager.notify(NOTIFICATION_ID, builder.build())
    }

    fun cancel(context: Context) {
        NotificationManagerCompat.from(context).cancel(NOTIFICATION_ID)
    }
}
