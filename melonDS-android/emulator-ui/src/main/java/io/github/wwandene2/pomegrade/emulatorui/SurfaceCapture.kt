package io.github.wwandene2.pomegrade.emulatorui

import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import android.view.PixelCopy
import android.view.SurfaceView

/**
 * The image a core shows on its [SurfaceView], copied by the system (PixelCopy), for the cores that keep no
 * copy of their frame themselves (3DS, Switch). [onResult] runs on the main thread, with null when the
 * surface isn't showing anything (not created yet, or released).
 */
object SurfaceCapture {

    fun capture(view: SurfaceView, onResult: (Bitmap?) -> Unit) {
        val surface = view.holder.surface
        if (surface == null || !surface.isValid || view.width == 0 || view.height == 0) {
            onResult(null)
            return
        }
        val bitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        PixelCopy.request(view, bitmap, { result ->
            onResult(bitmap.takeIf { result == PixelCopy.SUCCESS })
        }, Handler(Looper.getMainLooper()))
    }
}
