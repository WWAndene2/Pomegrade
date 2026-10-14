package io.github.wwandene2.pomegrade.emulatorui

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager

/**
 * Pomegrade: the Classic layouts' (DS, 3DS) haptic feedback: a very hard and short "click" (owner's request),
 * Android's predefined heavy click, when a control goes down and again when it comes back up ("click click", as a
 * real key). Each core calls it only when its own haptic feedback setting is on.
 */
object ClassicHaptics {
    fun click(context: Context) {
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            (context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager)?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        } ?: return
        if (!vibrator.hasVibrator()) return
        vibrator.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_HEAVY_CLICK))
    }
}
