package io.github.wwandene2.pomegrade.emulatorui

import android.content.ContentValues
import android.content.Context
import android.graphics.Bitmap
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.provider.MediaStore
import android.widget.Toast
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * The in-game menu's screenshot, the same for every console: a PNG in the phone's gallery, under
 * Pictures/Punica/<console> (MediaStore: no storage permission needed, and it stays when the app is
 * uninstalled). The GameCube / Wii core keeps its own screenshots (GC-Wii/ScreenShots), as Dolphin does.
 */
object ScreenshotGallery {

    /**
     * Saves [bitmap] on a thread of its own (a PNG of a phone's screen takes a while) and tells the user;
     * [console] is the folder's name ("DS", "3DS"...). Returns at once; any thread.
     */
    fun save(context: Context, bitmap: Bitmap?, console: String, title: String) {
        val appContext = context.applicationContext
        Thread {
            val saved = bitmap != null && runCatching { write(appContext, bitmap, console, title) }.getOrDefault(false)
            val message = if (saved) R.string.emulator_screenshot_saved else R.string.emulator_screenshot_failed
            Handler(Looper.getMainLooper()).post {
                Toast.makeText(appContext, message, Toast.LENGTH_SHORT).show()
            }
        }.start()
    }

    private fun write(context: Context, bitmap: Bitmap, console: String, title: String): Boolean {
        // a file name without the characters a file system refuses
        val name = title.replace(Regex("[\\\\/:*?\"<>|]"), "_").trim().ifEmpty { console } +
            " " + SimpleDateFormat("yyyy-MM-dd HH.mm.ss", Locale.US).format(Date()) + ".png"
        val values = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, name)
            put(MediaStore.Images.Media.MIME_TYPE, "image/png")
            put(MediaStore.Images.Media.RELATIVE_PATH, "${Environment.DIRECTORY_PICTURES}/Punica/$console")
            put(MediaStore.Images.Media.IS_PENDING, 1)
        }
        val resolver = context.contentResolver
        val uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values) ?: return false
        val written = resolver.openOutputStream(uri)?.use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) } ?: false
        if (!written) {
            resolver.delete(uri, null, null)
            return false
        }
        resolver.update(uri, ContentValues().apply { put(MediaStore.Images.Media.IS_PENDING, 0) }, null, null)
        return true
    }
}
