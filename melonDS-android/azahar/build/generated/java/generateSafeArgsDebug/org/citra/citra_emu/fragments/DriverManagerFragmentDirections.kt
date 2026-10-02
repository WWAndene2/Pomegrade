package org.citra.citra_emu.fragments

import androidx.`annotation`.CheckResult
import androidx.navigation.NavDirections
import kotlin.Long
import org.citra.citra_emu.HomeNavigationDirections
import org.citra.citra_emu.model.Game

public class DriverManagerFragmentDirections private constructor() {
  public companion object {
    @CheckResult
    public fun actionGlobalEmulationActivity(game: Game? = null): NavDirections = HomeNavigationDirections.actionGlobalEmulationActivity(game)

    @CheckResult
    public fun actionGlobalCheatsFragment(titleId: Long = -1L): NavDirections = HomeNavigationDirections.actionGlobalCheatsFragment(titleId)
  }
}
