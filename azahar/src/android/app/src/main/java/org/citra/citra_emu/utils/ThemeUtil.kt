// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Color
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.preference.PreferenceManager
import kotlin.math.roundToInt
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.ui.main.ThemeProvider

object ThemeUtil {
    const val SYSTEM_BAR_ALPHA = 0.9f

    private val preferences: SharedPreferences get() =
        PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)

    // Pomegrade: Punica is always dark and coral (owner's decision, 13 October), so Azahar's own
    // colour, Material You, theme mode and black backgrounds are not applied, whatever was stored
    // before (its theme section is not reachable from Punica's settings)
    fun setTheme(activity: AppCompatActivity) {
        setThemeMode(activity)
        activity.setTheme(R.style.Theme_Citra_Red)
    }

    fun setThemeMode(activity: AppCompatActivity) {
        val themeMode = AppCompatDelegate.MODE_NIGHT_YES // Pomegrade: always dark
        activity.delegate.localNightMode = themeMode
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        when (themeMode) {
            AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM -> when (isNightMode(activity)) {
                false -> setLightModeSystemBars(windowController)
                true -> setDarkModeSystemBars(windowController)
            }

            AppCompatDelegate.MODE_NIGHT_NO -> setLightModeSystemBars(windowController)

            AppCompatDelegate.MODE_NIGHT_YES -> setDarkModeSystemBars(windowController)
        }
    }

    private fun isNightMode(activity: AppCompatActivity): Boolean =
        when (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> false
            Configuration.UI_MODE_NIGHT_YES -> true
            else -> false
        }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true
        windowController.isAppearanceLightNavigationBars = true
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as ThemeProvider).themeId
        setTheme(activity)
        if (currentTheme != (activity as ThemeProvider).themeId) {
            activity.recreate()
        }
    }

    @ColorInt
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int = Color.argb(
        (alphaFactor * Color.alpha(color)).roundToInt(),
        Color.red(color),
        Color.green(color),
        Color.blue(color)
    )

    // Listener that detects if the theme keys are being changed from the setting menu and recreates the activity
    private var listener: SharedPreferences.OnSharedPreferenceChangeListener? = null

    fun themeChangeListener(activity: AppCompatActivity) {
        listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
            val relevantKeys =
                listOf(
                    Settings.PREF_STATIC_THEME_COLOR,
                    Settings.PREF_MATERIAL_YOU,
                    Settings.PREF_BLACK_BACKGROUNDS
                )
            if (key in relevantKeys) {
                activity.recreate()
            }
        }
        preferences.registerOnSharedPreferenceChangeListener(listener)
    }
}
