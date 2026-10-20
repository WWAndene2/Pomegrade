// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Color
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlin.math.roundToInt
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.ui.main.ThemeProvider
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings

object ThemeHelper {
    const val SYSTEM_BAR_ALPHA = 0.9f

    // Listener that detects if the theme keys are being changed from the setting menu and recreates the activity
    private var listener: SharedPreferences.OnSharedPreferenceChangeListener? = null
    private val preferences = PreferenceManager.getDefaultSharedPreferences(
        YuzuApplication.appContext
    )

    // Pomegrade: Punica is always dark and coral (owner's decision, 13 October), so Eden's own theme,
    // colour, theme mode and black backgrounds are not applied, whatever was stored before (their
    // settings are hidden, SettingsFragmentPresenter.addThemeSettings)
    fun setTheme(activity: AppCompatActivity) {
        setThemeMode(activity)
        activity.setTheme(R.style.Theme_Eden_Main)
    }

    @ColorInt
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(),
            Color.red(color),
            Color.green(color),
            Color.blue(color)
        )
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as ThemeProvider).themeId
        setTheme(activity)
        if (currentTheme != (activity as ThemeProvider).themeId) {
            activity.recreate()
        }
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

    private fun isNightMode(activity: AppCompatActivity): Boolean {
        return when (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> false
            Configuration.UI_MODE_NIGHT_YES -> true
            else -> false
        }
    }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true
        windowController.isAppearanceLightNavigationBars = true
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    fun ThemeChangeListener(activity: AppCompatActivity) {
        listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
            val relevantKeys = listOf(
                Settings.PREF_STATIC_THEME_COLOR,
                Settings.PREF_THEME_MODE,
                Settings.PREF_BLACK_BACKGROUNDS
            )
            if (key in relevantKeys) {
                activity.recreate()
            }
        }
        preferences.registerOnSharedPreferenceChangeListener(listener)
    }
}

enum class Theme(val int: Int) {
    Default(0),
    MaterialYou(1);

    companion object {
        fun from(int: Int): Theme = entries.firstOrNull { it.int == int } ?: Default
    }
}
