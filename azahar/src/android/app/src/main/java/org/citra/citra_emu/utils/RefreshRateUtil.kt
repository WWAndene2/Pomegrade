// Copyright 2025-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils
import android.app.Activity
import android.os.Build
import kotlin.math.abs
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.features.settings.model.IntSetting

object RefreshRateUtil {
    // Since Android 15, the OS automatically runs apps categorized as games with a
    // 60hz refresh rate by default, regardless of the refresh rate set by the user.
    //
    // This function sets the refresh rate to either the maximum allowed refresh rate or
    // 60hz depending on the value of the `sixtyHz` parameter.
    //
    // Note: This isn't always the maximum refresh rate that the display is *capable of*,
    // but is instead the refresh rate chosen by the user in the Android system settings.
    // For example, if the user selected 120hz in the settings, but the display is capable
    // of 144hz, 120hz will be treated as the maximum within this function.
    fun enforceRefreshRate(activity: Activity, sixtyHz: Boolean = false) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return
        }

        val display = activity.display
        val window = activity.window

        display?.let {
            // Get all supported modes and find the one with the highest refresh rate
            val supportedModes = it.supportedModes
            val maxRefreshRate = supportedModes.maxByOrNull { mode -> mode.refreshRate }

            if (maxRefreshRate == null) {
                return
            }

            var newModeId: Int?
            if (sixtyHz) {
                newModeId = supportedModes.firstOrNull { mode -> mode.refreshRate == 60f }?.modeId
            } else {
                // Set the preferred display mode to the one with the highest refresh rate
                newModeId = maxRefreshRate.modeId
            }

            if (newModeId == null) {
                return
            }

            window.attributes.preferredDisplayModeId = newModeId
        }
    }

    /**
     * Pomegrade: the screen rate for the frame rate mode (IntSetting.FRAME_RATE_MODE, video_core/frame_generation.h) during
     * emulation: 60 Hz for 30, 60 and 60 smooth (as before), the mode at 120 Hz for 120, at 240 Hz (else 120) for 240 and
     * adaptive, at exactly that rate (120 images on a 144 Hz screen would be shown unevenly). Then the rate the screen is
     * at goes to the emulator, which never generates more images than the screen shows.
     */
    fun applyFrameRateMode(activity: Activity, hot: Boolean) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val display = activity.display
            if (display != null) {
                val current = display.mode
                val modes = display.supportedModes.filter {
                    it.physicalWidth == current.physicalWidth && it.physicalHeight == current.physicalHeight
                }
                fun modeAt(rate: Float) =
                    modes.filter { abs(it.refreshRate - rate) < 1f }.minByOrNull { abs(it.refreshRate - rate) }
                val mode = when (IntSetting.FRAME_RATE_MODE.int) {
                    3 -> modeAt(120f)
                    4, 5 -> modeAt(240f) ?: modeAt(120f)
                    else -> modeAt(60f)
                }
                if (mode != null) {
                    // the attributes are a copy: they apply once set back
                    activity.window.attributes = activity.window.attributes.also { it.preferredDisplayModeId = mode.modeId }
                }
            }
        }
        reportDisplay(activity, hot)
    }

    /** The screen's rate and the phone's heat to the emulator (adaptive mode: 60 while hot) */
    fun reportDisplay(activity: Activity, hot: Boolean) {
        val rate = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            activity.display?.refreshRate ?: 0f
        } else {
            @Suppress("DEPRECATION")
            activity.windowManager.defaultDisplay.refreshRate
        }
        NativeLibrary.setDisplayRefresh(rate, hot)
    }
}
