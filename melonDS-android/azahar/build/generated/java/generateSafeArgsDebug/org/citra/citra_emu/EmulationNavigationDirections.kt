package org.citra.citra_emu

import android.os.Bundle
import androidx.`annotation`.CheckResult
import androidx.navigation.NavDirections
import kotlin.Int
import kotlin.Long

public class EmulationNavigationDirections private constructor() {
  private data class ActionGlobalCheatsActivity(
    public val titleId: Long = -1L,
  ) : NavDirections {
    public override val actionId: Int = R.id.action_global_cheatsActivity

    public override val arguments: Bundle
      get() {
        val result = Bundle()
        result.putLong("titleId", this.titleId)
        return result
      }
  }

  public companion object {
    @CheckResult
    public fun actionGlobalCheatsActivity(titleId: Long = -1L): NavDirections = ActionGlobalCheatsActivity(titleId)
  }
}
