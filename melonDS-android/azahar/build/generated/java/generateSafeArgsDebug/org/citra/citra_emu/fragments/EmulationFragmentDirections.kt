package org.citra.citra_emu.fragments

import androidx.`annotation`.CheckResult
import androidx.navigation.NavDirections
import kotlin.Long
import org.citra.citra_emu.EmulationNavigationDirections

public class EmulationFragmentDirections private constructor() {
  public companion object {
    @CheckResult
    public fun actionGlobalCheatsActivity(titleId: Long = -1L): NavDirections = EmulationNavigationDirections.actionGlobalCheatsActivity(titleId)
  }
}
