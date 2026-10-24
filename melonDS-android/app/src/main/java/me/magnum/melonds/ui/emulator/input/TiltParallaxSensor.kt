package me.magnum.melonds.ui.emulator.input

import android.app.Activity
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.Surface
import me.magnum.melonds.MelonEmulator
import kotlin.math.exp

/**
 * Pomegrade: the tilt parallax (Tilt depth setting). The phone's gyroscope turns its tilt into a shift of the DS 3D scene by depth
 * (see MelonEmulator.setTiltParallax), so the screen looks like a window onto a model: turn the phone's right edge away and the
 * far scene slides right, as it would behind glass.
 *
 * The rest position follows the phone slowly (a few seconds), so the effect needs no recentring and the gyroscope's drift
 * doesn't build up: holding the phone still in any posture brings the scene back in place. Only the game's drawing changes,
 * never its input. Without a gyroscope it does nothing (the setting's summary says it needs one).
 *
 * @param strength 1 to 100, the setting's value
 */
class TiltParallaxSensor(private val activity: Activity, strength: Int) : SensorEventListener {

    private companion object {
        /** The tilt, in radians, that gives the full shift (about 20 degrees) */
        const val FULL_TILT = 0.35f
        /** The farthest points' shift at full strength and full tilt, in clip units (1 = half the screen) */
        const val MAX_SHIFT = 0.12f
        /** How fast the rest position follows the phone, in seconds */
        const val RECENTRE_SECONDS = 3f
    }

    private val sensorManager = activity.getSystemService(SensorManager::class.java)
    private val gyroscope = sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
    private val shiftPerRadian = strength.coerceIn(1, 100) / 100f * MAX_SHIFT / FULL_TILT

    // tilt from the rest position, in the screen's axes (x to the right, y up), in radians
    private var tiltAboutX = 0f
    private var tiltAboutY = 0f
    private var lastTimestamp = 0L

    fun start() {
        val sensor = gyroscope ?: return
        tiltAboutX = 0f
        tiltAboutY = 0f
        lastTimestamp = 0L
        sensorManager?.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME)
    }

    fun stop() {
        sensorManager?.unregisterListener(this)
        MelonEmulator.setTiltParallax(0f, 0f)
    }

    override fun onSensorChanged(event: SensorEvent) {
        val timestamp = event.timestamp
        if (lastTimestamp == 0L) {
            lastTimestamp = timestamp
            return
        }
        val seconds = ((timestamp - lastTimestamp) / 1_000_000_000f).coerceIn(0f, 0.1f)
        lastTimestamp = timestamp

        // the gyroscope's axes are the device's natural ones: turned into the screen's, as the activity is rotated
        val deviceX = event.values[0]
        val deviceY = event.values[1]
        @Suppress("DEPRECATION")
        val rotation = activity.windowManager.defaultDisplay.rotation
        val (rateX, rateY) = when (rotation) {
            Surface.ROTATION_90 -> -deviceY to deviceX
            Surface.ROTATION_180 -> -deviceX to -deviceY
            Surface.ROTATION_270 -> deviceY to -deviceX
            else -> deviceX to deviceY
        }

        val keep = exp(-seconds / RECENTRE_SECONDS)
        tiltAboutX = ((tiltAboutX + rateX * seconds) * keep).coerceIn(-FULL_TILT, FULL_TILT)
        tiltAboutY = ((tiltAboutY + rateY * seconds) * keep).coerceIn(-FULL_TILT, FULL_TILT)

        // right edge away (positive about y): the eye is right of the screen's normal, the far scene slides right.
        // Top edge away (negative about x): the eye is above it, the far scene slides up.
        MelonEmulator.setTiltParallax(tiltAboutY * shiftPerRadian, -tiltAboutX * shiftPerRadian)
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
}
