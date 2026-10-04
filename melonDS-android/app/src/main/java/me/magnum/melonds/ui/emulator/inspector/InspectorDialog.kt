package me.magnum.melonds.ui.emulator.inspector

import android.content.Context
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import me.magnum.melonds.MelonEmulator
import me.magnum.melonds.R
import me.magnum.melonds.impl.PomegradeFolder
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Pomegrade: the in-game inspector dialog (DS_ENGINE_REMAKE.md 5.12 step 1). Turns the core's inspector on or off, picks what the 3D is
 * coloured by, and saves its report to Pomegrade/Inspector, where it can be shared. The mode lasts until the game is closed.
 */
object InspectorDialog {

    /** Off, recording only, or recording and colouring the 3D by polygon ID, call site or display list. */
    enum class Mode(val labelRes: Int, val view: Int) {
        OFF(R.string.inspector_mode_off, 0),
        RECORD(R.string.inspector_mode_record, 0),
        POLYGON_ID(R.string.inspector_mode_polygon_id, 1),
        CALL_SITE(R.string.inspector_mode_call_site, 2),
        DISPLAY_LIST(R.string.inspector_mode_display_list, 3);

        val enabled: Boolean get() = this != OFF
    }

    private const val FOLDER = "Inspector"
    private const val MATERIALS_FOLDER = "Materials"

    /**
     * @param onModeChanged the mode picked, already applied to the emulator
     * @param onDismiss the dialog went away (the game can resume)
     */
    fun show(context: Context, current: Mode, gameName: String, onModeChanged: (Mode) -> Unit, onDismiss: () -> Unit): AlertDialog {
        var mode = current
        val labels = Mode.entries.map { context.getString(it.labelRes) }.toTypedArray()
        return AlertDialog.Builder(context)
            .setTitle(R.string.inspector_title)
            .setSingleChoiceItems(labels, current.ordinal) { _, which ->
                if (mode == Mode.OFF && Mode.entries[which].enabled) loadMaterialManifest(context)
                mode = Mode.entries[which]
                MelonEmulator.setInspector(mode.enabled, mode.view)
                onModeChanged(mode)
            }
            .setPositiveButton(R.string.inspector_save_report) { _, _ ->
                if (!mode.enabled) {
                    Toast.makeText(context, R.string.inspector_save_needs_recording, Toast.LENGTH_LONG).show()
                } else {
                    saveReport(context, gameName)
                }
            }
            .setNegativeButton(R.string.inspector_close, null)
            .setOnDismissListener { onDismiss() }
            .show()
    }

    /**
     * The game's material manifest, Pomegrade/Materials/<game code>.txt, read when recording starts: textures named by hash there
     * take its class over the classifier's. Missing: none.
     */
    private fun loadMaterialManifest(context: Context) {
        val gameCode = MelonEmulator.getGameCode().takeIf { it.isNotBlank() } ?: return
        val file = PomegradeFolder.subFolder(context, MATERIALS_FOLDER)?.let { File(it, gameCode.replace(Regex("[^A-Za-z0-9]"), "_") + ".txt") }
        val text = file?.takeIf { it.isFile }?.let { runCatching { it.readText() }.getOrNull() } ?: ""
        MelonEmulator.setMaterialManifest(text)
    }

    private fun saveReport(context: Context, gameName: String) {
        val report = MelonEmulator.getInspectorReport()
        // the Pomegrade folder when it is set up, else the app's own folder
        val folder = PomegradeFolder.subFolder(context, FOLDER) ?: context.getExternalFilesDir(FOLDER)
        val safeName = gameName.replace(Regex("[^A-Za-z0-9 ._-]"), "_").take(64).ifBlank { "game" }
        val stamp = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(Date())
        val file = folder?.let { File(it, "$safeName $stamp.txt") }
        val saved = file != null && runCatching {
            file.parentFile?.mkdirs()
            file.writeText(report)
        }.isSuccess
        val message = if (saved) context.getString(R.string.inspector_saved, file!!.absolutePath) else context.getString(R.string.inspector_save_failed)
        Toast.makeText(context, message, Toast.LENGTH_LONG).show()
    }
}
