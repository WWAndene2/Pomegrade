package org.citra.citra_emu.features.cheats.ui

import android.os.Bundle
import androidx.lifecycle.SavedStateHandle
import androidx.navigation.NavArgs
import java.lang.IllegalArgumentException
import kotlin.Long
import kotlin.jvm.JvmStatic

public data class CheatsActivityArgs(
  public val titleId: Long = -1L,
) : NavArgs {
  public fun toBundle(): Bundle {
    val result = Bundle()
    result.putLong("titleId", this.titleId)
    return result
  }

  public fun toSavedStateHandle(): SavedStateHandle {
    val result = SavedStateHandle()
    result.set("titleId", this.titleId)
    return result
  }

  public companion object {
    @JvmStatic
    public fun fromBundle(bundle: Bundle): CheatsActivityArgs {
      bundle.setClassLoader(CheatsActivityArgs::class.java.classLoader)
      val __titleId : Long
      if (bundle.containsKey("titleId")) {
        __titleId = bundle.getLong("titleId")
      } else {
        __titleId = -1L
      }
      return CheatsActivityArgs(__titleId)
    }

    @JvmStatic
    public fun fromSavedStateHandle(savedStateHandle: SavedStateHandle): CheatsActivityArgs {
      val __titleId : Long?
      if (savedStateHandle.contains("titleId")) {
        __titleId = savedStateHandle["titleId"]
        if (__titleId == null) {
          throw IllegalArgumentException("Argument \"titleId\" of type long does not support null values")
        }
      } else {
        __titleId = -1L
      }
      return CheatsActivityArgs(__titleId)
    }
  }
}
