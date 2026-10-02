package org.citra.citra_emu.activities

import android.os.Bundle
import android.os.Parcelable
import androidx.lifecycle.SavedStateHandle
import androidx.navigation.NavArgs
import java.io.Serializable
import java.lang.UnsupportedOperationException
import kotlin.Suppress
import kotlin.jvm.JvmStatic
import org.citra.citra_emu.model.Game

public data class EmulationActivityArgs(
  public val game: Game? = null,
) : NavArgs {
  @Suppress("CAST_NEVER_SUCCEEDS")
  public fun toBundle(): Bundle {
    val result = Bundle()
    if (Parcelable::class.java.isAssignableFrom(Game::class.java)) {
      result.putParcelable("game", this.game as Parcelable?)
    } else if (Serializable::class.java.isAssignableFrom(Game::class.java)) {
      result.putSerializable("game", this.game as Serializable?)
    }
    return result
  }

  @Suppress("CAST_NEVER_SUCCEEDS")
  public fun toSavedStateHandle(): SavedStateHandle {
    val result = SavedStateHandle()
    if (Parcelable::class.java.isAssignableFrom(Game::class.java)) {
      result.set("game", this.game as Parcelable?)
    } else if (Serializable::class.java.isAssignableFrom(Game::class.java)) {
      result.set("game", this.game as Serializable?)
    }
    return result
  }

  public companion object {
    @JvmStatic
    @Suppress("DEPRECATION")
    public fun fromBundle(bundle: Bundle): EmulationActivityArgs {
      bundle.setClassLoader(EmulationActivityArgs::class.java.classLoader)
      val __game : Game?
      if (bundle.containsKey("game")) {
        if (Parcelable::class.java.isAssignableFrom(Game::class.java) || Serializable::class.java.isAssignableFrom(Game::class.java)) {
          __game = bundle.get("game") as Game?
        } else {
          throw UnsupportedOperationException(Game::class.java.name + " must implement Parcelable or Serializable or must be an Enum.")
        }
      } else {
        __game = null
      }
      return EmulationActivityArgs(__game)
    }

    @JvmStatic
    public fun fromSavedStateHandle(savedStateHandle: SavedStateHandle): EmulationActivityArgs {
      val __game : Game?
      if (savedStateHandle.contains("game")) {
        if (Parcelable::class.java.isAssignableFrom(Game::class.java) || Serializable::class.java.isAssignableFrom(Game::class.java)) {
          __game = savedStateHandle.get<Game?>("game")
        } else {
          throw UnsupportedOperationException(Game::class.java.name + " must implement Parcelable or Serializable or must be an Enum.")
        }
      } else {
        __game = null
      }
      return EmulationActivityArgs(__game)
    }
  }
}
