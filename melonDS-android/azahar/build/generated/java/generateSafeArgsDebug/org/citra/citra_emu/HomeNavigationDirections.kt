package org.citra.citra_emu

import android.os.Bundle
import android.os.Parcelable
import androidx.`annotation`.CheckResult
import androidx.navigation.NavDirections
import java.io.Serializable
import kotlin.Int
import kotlin.Long
import kotlin.Suppress
import org.citra.citra_emu.model.Game

public class HomeNavigationDirections private constructor() {
  private data class ActionGlobalEmulationActivity(
    public val game: Game? = null,
  ) : NavDirections {
    public override val actionId: Int = R.id.action_global_emulationActivity

    public override val arguments: Bundle
      @Suppress("CAST_NEVER_SUCCEEDS")
      get() {
        val result = Bundle()
        if (Parcelable::class.java.isAssignableFrom(Game::class.java)) {
          result.putParcelable("game", this.game as Parcelable?)
        } else if (Serializable::class.java.isAssignableFrom(Game::class.java)) {
          result.putSerializable("game", this.game as Serializable?)
        }
        return result
      }
  }

  private data class ActionGlobalCheatsFragment(
    public val titleId: Long = -1L,
  ) : NavDirections {
    public override val actionId: Int = R.id.action_global_cheatsFragment

    public override val arguments: Bundle
      get() {
        val result = Bundle()
        result.putLong("titleId", this.titleId)
        return result
      }
  }

  public companion object {
    @CheckResult
    public fun actionGlobalEmulationActivity(game: Game? = null): NavDirections = ActionGlobalEmulationActivity(game)

    @CheckResult
    public fun actionGlobalCheatsFragment(titleId: Long = -1L): NavDirections = ActionGlobalCheatsFragment(titleId)
  }
}
