package org.citra.citra_emu.fragments

import androidx.`annotation`.CheckResult
import androidx.navigation.ActionOnlyNavDirections
import androidx.navigation.NavDirections
import kotlin.Long
import org.citra.citra_emu.HomeNavigationDirections
import org.citra.citra_emu.R
import org.citra.citra_emu.model.Game

public class HomeSettingsFragmentDirections private constructor() {
  public companion object {
    @CheckResult
    public fun actionHomeSettingsFragmentToAboutFragment(): NavDirections = ActionOnlyNavDirections(R.id.action_homeSettingsFragment_to_aboutFragment)

    @CheckResult
    public fun actionHomeSettingsFragmentToSystemFilesFragment(): NavDirections = ActionOnlyNavDirections(R.id.action_homeSettingsFragment_to_systemFilesFragment)

    @CheckResult
    public fun actionHomeSettingsFragmentToDriverManagerFragment(): NavDirections = ActionOnlyNavDirections(R.id.action_homeSettingsFragment_to_driverManagerFragment)

    @CheckResult
    public fun actionGlobalEmulationActivity(game: Game? = null): NavDirections = HomeNavigationDirections.actionGlobalEmulationActivity(game)

    @CheckResult
    public fun actionGlobalCheatsFragment(titleId: Long = -1L): NavDirections = HomeNavigationDirections.actionGlobalCheatsFragment(titleId)
  }
}
