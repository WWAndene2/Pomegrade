package me.magnum.melonds.ui.gamedata

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.extensions.parcelable
import me.magnum.melonds.impl.GameData
import me.magnum.melonds.impl.ModInstaller
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.impl.PunicaLayout
import me.magnum.melonds.impl.TexturePackInstaller
import me.magnum.melonds.parcelables.RomParcelable
import me.magnum.melonds.ui.common.rom.TexturePackResultDialog
import me.magnum.melonds.ui.gamedata.ui.GameDataScreen
import me.magnum.melonds.ui.gamedata.ui.GameDataState
import me.magnum.melonds.ui.theme.MelonTheme

/**
 * A game's mods and saves (owner's request, 29 October), opened from its sheet: the save (copies, restore, export,
 * import, delete) and the mods and texture packs (install, on / off, delete), each change of the save or of a mod
 * made after a copy of the save ([GameData]). Needs the Punica folder in its layout; without it, says so and closes.
 */
class GameDataActivity : AppCompatActivity() {

    companion object {
        private const val KEY_ROM = "rom"
        private val ZIP_TYPES = arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")

        fun intent(context: Context, rom: Rom): Intent = Intent(context, GameDataActivity::class.java).putExtra(KEY_ROM, RomParcelable(rom))
    }

    private lateinit var rom: Rom
    private var data: GameData? = null
    private var state by mutableStateOf(GameDataState())

    // what the zip picked is for
    private var pendingInstall: GameData.Kind? = null
    private val installPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        val kind = pendingInstall ?: return@registerForActivityResult
        pendingInstall = null
        if (uri != null) install(kind, uri)
    }
    private val importPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) confirmImport(uri) }
    private val exportCreator = registerForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri -> if (uri != null) export(uri) }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        rom = intent.parcelable<RomParcelable>(KEY_ROM)?.rom ?: run { finish(); return }
        setContent {
            MelonTheme {
                GameDataScreen(
                    rom = rom,
                    state = state,
                    onBack = { finish() },
                    onBackup = { withData { it.backup(getString(R.string.game_data_reason_manual)); Unit } },
                    onRestore = { backup -> confirm(R.string.game_data_restore_confirm) { withData { it.restore(backup, getString(R.string.game_data_reason_before_restore)) } } },
                    onDeleteBackup = { backup -> confirm(R.string.game_data_delete_backup_confirm) { withData { it.deleteBackup(backup) } } },
                    onExport = { exportCreator.launch("${state.title}.zip") },
                    onImport = { importPicker.launch(ZIP_TYPES) },
                    onDeleteSave = { confirm(R.string.game_data_delete_save_confirm) { withData { it.delete(getString(R.string.game_data_reason_before_delete)) } } },
                    onInstall = { kind -> pendingInstall = kind; installPicker.launch(ZIP_TYPES) },
                    onToggleMod = { mod -> withData { it.setActive(mod, !mod.active, getString(R.string.game_data_reason_before_mod)) } },
                    onDeleteMod = { mod -> confirm(R.string.game_data_delete_mod_confirm) { withData { it.deleteMod(mod, getString(R.string.game_data_reason_before_mod)) } } },
                )
            }
        }
        lifecycleScope.launch {
            val root = withContext(Dispatchers.IO) { PomegradeFolder.layoutRoot(this@GameDataActivity) }
            if (root == null) {
                Toast.makeText(this@GameDataActivity, R.string.game_data_no_folder, Toast.LENGTH_LONG).show()
                finish()
                return@launch
            }
            val id = withContext(Dispatchers.IO) { gameId() }
            data = GameData(root, rom.platform, rom.fileName, id)
            refresh()
        }
    }

    // the ID the game's save and mods are named after: the cartridge's or disc's (TexturePackInstaller.gameId), the
    // Switch title ID in the file's name ([0100…]); null when it can't be read (the page then offers what needs none)
    private fun gameId(): String? = when (rom.platform) {
        RomPlatform.SWITCH -> Regex("\\[([0-9A-Fa-f]{16})]").find(rom.fileName)?.groupValues?.get(1)?.uppercase()
        RomPlatform.GBA -> null
        else -> TexturePackInstaller.gameId(this, rom)?.uppercase()
    }

    private fun refresh() {
        val data = data ?: return
        lifecycleScope.launch {
            state = withContext(Dispatchers.IO) {
                GameDataState(
                    loaded = true,
                    title = rom.config.customName ?: rom.name.ifEmpty { rom.fileName },
                    hasSave = data.saves().isNotEmpty(),
                    lastSaved = data.lastSaved(),
                    backups = data.backups(),
                    mods = data.mods(),
                    takesMods = data.takesMods,
                    takesTextures = data.takesTextures,
                )
            }
        }
    }

    // [action] on the game's data off the main thread, then the page shown again; a failure is said
    private fun withData(action: (GameData) -> Unit) {
        val data = data ?: return
        lifecycleScope.launch {
            val failed = withContext(Dispatchers.IO) { runCatching { action(data) }.isFailure }
            if (failed) Toast.makeText(this@GameDataActivity, R.string.game_data_failed, Toast.LENGTH_LONG).show()
            refresh()
        }
    }

    private fun confirm(message: Int, onYes: () -> Unit) {
        AlertDialog.Builder(this)
            .setMessage(message)
            .setPositiveButton(R.string.ok) { _, _ -> onYes() }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun confirmImport(uri: Uri) = confirm(R.string.game_data_import_confirm) {
        val data = data ?: return@confirm
        lifecycleScope.launch {
            val written = withContext(Dispatchers.IO) {
                runCatching { data.import({ contentResolver.openInputStream(uri)!! }, getString(R.string.game_data_reason_before_import)) }.getOrDefault(-1)
            }
            Toast.makeText(this@GameDataActivity, if (written > 0) R.string.game_data_imported else R.string.game_data_import_none, Toast.LENGTH_LONG).show()
            refresh()
        }
    }

    private fun export(uri: Uri) {
        val data = data ?: return
        lifecycleScope.launch {
            val files = withContext(Dispatchers.IO) {
                runCatching { contentResolver.openOutputStream(uri)!!.use { data.export(it) } }.getOrDefault(-1)
            }
            Toast.makeText(this@GameDataActivity, if (files > 0) R.string.game_data_exported else R.string.game_data_failed, Toast.LENGTH_LONG).show()
        }
    }

    // a mod or texture pack from a zip: the one in use set aside first (kept, not deleted), the save copied
    private fun install(kind: GameData.Kind, uri: Uri) {
        val data = data ?: return
        val name = displayName(uri)?.substringBeforeLast('.') ?: getString(R.string.game_data_mod_unnamed)
        val reason = getString(R.string.game_data_reason_before_mod)
        lifecycleScope.launch {
            if (kind == GameData.Kind.TEXTURES) {
                val result = withContext(Dispatchers.IO) {
                    data.setAsideBeforeInstall(kind, reason)
                    TexturePackInstaller.install(this@GameDataActivity, rom, uri).also { data.nameInstalled(kind, name) }
                }
                TexturePackResultDialog.show(this@GameDataActivity, result)
            } else {
                val ok = withContext(Dispatchers.IO) {
                    runCatching {
                        if (rom.platform == RomPlatform.SWITCH) {
                            data.installSwitchMod({ contentResolver.openInputStream(uri)!! }, name, reason) > 0
                        } else {
                            data.setAsideBeforeInstall(kind, reason)
                            val threeDs = PomegradeFolder.layoutRoot(this@GameDataActivity)!!.resolve(PunicaLayout.N3DS)
                            val result = ModInstaller().install({ contentResolver.openInputStream(uri)!! }, threeDs, name)
                            (result is ModInstaller.Result.Installed).also { if (it) data.nameInstalled(kind, name) }
                        }
                    }.getOrDefault(false)
                }
                Toast.makeText(this@GameDataActivity, if (ok) R.string.game_data_mod_installed else R.string.game_data_mod_not_installed, Toast.LENGTH_LONG).show()
            }
            refresh()
        }
    }

    private fun displayName(uri: Uri): String? = runCatching {
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { if (it.moveToFirst()) it.getString(0) else null }
    }.getOrNull()
}
