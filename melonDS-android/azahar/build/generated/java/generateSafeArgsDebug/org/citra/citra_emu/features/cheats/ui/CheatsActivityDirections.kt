package org.citra.citra_emu.features.cheats.ui

import androidx.`annotation`.CheckResult
import androidx.navigation.NavDirections
import kotlin.Long
import org.citra.citra_emu.EmulationNavigationDirections

public class CheatsActivityDirections private constructor() {
  public companion object {
    @CheckResult
    public fun actionGlobalCheatsActivity(titleId: Long = -1L): NavDirections = EmulationNavigationDirections.actionGlobalCheatsActivity(titleId)
  }
}
