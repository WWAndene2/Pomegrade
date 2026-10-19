package me.magnum.melonds.ui.settings.fragments

import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.features.settings.ui.SettingsSubscreen
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.impl.ModInstaller
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.impl.PunicaLayout
import me.magnum.melonds.impl.SwitchUserDataImport
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag as GcWiiSettingsMenu
import org.yuzu.yuzu_emu.features.settings.model.Settings.MenuTag as SwitchSettingsMenu
import me.magnum.melonds.ui.common.rom.N3dsLauncher
import me.magnum.melonds.ui.common.rom.GcWiiLauncher
import me.magnum.melonds.ui.common.rom.SwitchLauncher
import me.magnum.melonds.ui.settings.preferences.ConsoleSelectorPreference
import me.magnum.melonds.ui.settings.PreferenceFragmentTitleProvider
import me.magnum.melonds.ui.settings.GcWiiToolDialogActivity
import me.magnum.melonds.ui.settings.SettingsActivity
import me.magnum.melonds.ui.settings.SwitchToolDialogActivity
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.utils.PermissionsHandler
import java.io.IOException
import java.io.InputStream
import java.text.DateFormat
import java.util.Date
import javax.inject.Inject

@AndroidEntryPoint
class MainPreferencesFragment : BasePreferenceFragment(), PreferenceFragmentTitleProvider {

    companion object {
        // keys of the 3DS entries: this prefix + the Azahar settings section they open
        private const val THREE_DS_PREFIX = "three_ds_settings_"
        private const val SWITCH_PREFIX = "switch_settings_"
        private const val SWITCH_TOOL_PREFIX = "switch_tool_"
        private const val SWITCH_DIALOG_PREFIX = "switch_dialog_"
        private const val GC_WII_PREFIX = "gc_wii_settings_"
        private const val GC_WII_TOOL_PREFIX = "gc_wii_tool_"
    }

    @Inject lateinit var pomegradeFolder: PomegradeFolder

    private val folderSetup get() = (requireActivity() as SettingsActivity).pomegradeFolderSetup

    // a 3DS mod's zip, picked with the system's file picker
    private val modZipLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { zip ->
        if (zip != null) readMod(zip)
    }

    // a backup exported by Eden, imported into the Switch folder
    private val switchBackupLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { zip ->
        if (zip != null) importSwitchData { SwitchUserDataImport.importBackup(it, zip) }
    }

    override fun getTitle() = getString(R.string.settings)

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.pref_main, rootKey)

        findPreference<Preference>("pomegrade_folder_info")?.setOnPreferenceClickListener {
            if (folderSetup.isSetUp()) {
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.pomegrade_folder_title)
                    .setMessage(getString(R.string.pomegrade_folder_location, pomegradeFolder.path()?.absolutePath.orEmpty()))
                    .setPositiveButton(R.string.ok, null)
                    .show()
            } else {
                folderSetup.start { updateFolderSummary() }
            }
            true
        }
        findPreference<Preference>("pomegrade_settings_save")?.setOnPreferenceClickListener {
            withFolder {
                val saved = withContext(Dispatchers.IO) { pomegradeFolder.saveSettings() }
                toast(if (saved) R.string.pomegrade_settings_saved else R.string.pomegrade_settings_failed)
            }
            true
        }
        findPreference<Preference>("pomegrade_check_system_files")?.setOnPreferenceClickListener {
            withFolder {
                val report = withContext(Dispatchers.IO) { runCatching { pomegradeFolder.checkSystemFiles() }.getOrNull() }
                if (report == null) {
                    toast(R.string.system_files_check_failed)
                } else {
                    Toast.makeText(requireContext(), getString(R.string.system_files_check_done, report.moved.size, report.extracted, report.left.size), Toast.LENGTH_LONG).show()
                }
            }
            true
        }
        findPreference<Preference>("pomegrade_settings_restore")?.setOnPreferenceClickListener {
            withFolder {
                if (!pomegradeFolder.hasSavedSettings()) {
                    toast(R.string.pomegrade_settings_none)
                } else {
                    val restored = withContext(Dispatchers.IO) { pomegradeFolder.restoreSettings() }
                    toast(if (restored) R.string.pomegrade_settings_restored else R.string.pomegrade_settings_failed)
                    // the screens show the restored values when opened again
                    requireActivity().recreate()
                }
            }
            true
        }

        // DS debug trace: written by path into the Pomegrade folder, the path kept with the setting
        findPreference<androidx.preference.SwitchPreference>("ds_debug_trace")?.setOnPreferenceChangeListener { _, value ->
            if (value != true) return@setOnPreferenceChangeListener true
            val traces = PomegradeFolder.consoleFolder(requireContext(), PunicaLayout.DS, "${PunicaLayout.CORE}/Traces")
            if (traces == null) {
                toast(R.string.ds_debug_trace_no_folder)
                return@setOnPreferenceChangeListener false
            }
            androidx.preference.PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                .putString("ds_debug_trace_folder", traces.absolutePath)
                .apply()
            Toast.makeText(requireContext(), getString(R.string.ds_debug_trace_on, traces.absolutePath), Toast.LENGTH_LONG).show()
            true
        }

        // one console's section at a time, picked by its logo; 3DS on 64-bit devices only
        findPreference<ConsoleSelectorPreference>("settings_console")?.let { selector ->
            selector.threeDsAvailable = N3dsLauncher.isSupported()
            selector.switchAvailable = SwitchLauncher.isSupported()
            selector.gcWiiAvailable = GcWiiLauncher.isSupported()
            selector.onConsoleSelected = { showConsoleSection(it) }
            showConsoleSection(selector.selected)
        }
        // the code trace is written next to the 3DS core's folder: its path is kept with the
        // setting, for Azahar's emulation screen
        findPreference<androidx.preference.SwitchPreference>(EmulationActivity.CODE_TRACE)?.setOnPreferenceChangeListener { _, value ->
            if (value != true) return@setOnPreferenceChangeListener true
            withThreeDsFolder {
                val traces = threeDsFolder()?.let { java.io.File(it, "${PunicaLayout.CORE}/Traces") }
                if (traces == null) {
                    toast(R.string.three_ds_code_trace_no_folder)
                    findPreference<androidx.preference.SwitchPreference>(EmulationActivity.CODE_TRACE)?.isChecked = false
                } else {
                    androidx.preference.PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                        .putString(EmulationActivity.CODE_TRACE_FOLDER, traces.absolutePath)
                        .apply()
                    Toast.makeText(requireContext(), getString(R.string.three_ds_code_trace_on, traces.absolutePath), Toast.LENGTH_LONG).show()
                }
            }
            true
        }
        findPreference<Preference>("three_ds_mod_install")?.setOnPreferenceClickListener {
            withThreeDsFolder { modZipLauncher.launch(arrayOf("application/zip", "application/x-zip-compressed")) }
            true
        }
        preferenceScreen.findPreference<androidx.preference.PreferenceCategory>("three_ds_settings")?.let { category ->
            for (i in 0 until category.preferenceCount) {
                val preference = category.getPreference(i)
                val key = preference.key ?: continue
                if (!key.startsWith(THREE_DS_PREFIX)) continue
                val section = key.removePrefix(THREE_DS_PREFIX)
                preference.setOnPreferenceClickListener {
                    openThreeDsSettings(section)
                    true
                }
            }
        }
        preferenceScreen.findPreference<androidx.preference.PreferenceCategory>("switch_settings")?.let { category ->
            for (i in 0 until category.preferenceCount) {
                val preference = category.getPreference(i)
                val key = preference.key ?: continue
                if (!key.startsWith(SWITCH_PREFIX)) continue
                val section = SwitchSettingsMenu.valueOf(key.removePrefix(SWITCH_PREFIX))
                preference.setOnPreferenceClickListener {
                    viewLifecycleOwner.lifecycleScope.launch {
                        SwitchLauncher.prepare(requireContext())
                        startActivity(SwitchLauncher.settingsIntent(requireContext(), section))
                    }
                    true
                }
            }
        }
        // Eden's home screen tools, opened directly
        preferenceScreen.findPreference<androidx.preference.PreferenceCategory>("switch_settings")?.let { category ->
            for (i in 0 until category.preferenceCount) {
                val preference = category.getPreference(i)
                val key = preference.key ?: continue
                if (key.startsWith(SWITCH_DIALOG_PREFIX)) {
                    val tool = SwitchToolDialogActivity.Tool.valueOf(key.removePrefix(SWITCH_DIALOG_PREFIX))
                    preference.setOnPreferenceClickListener {
                        viewLifecycleOwner.lifecycleScope.launch {
                            SwitchLauncher.prepare(requireContext())
                            startActivity(SwitchToolDialogActivity.intent(requireContext(), tool))
                        }
                        true
                    }
                    continue
                }
                if (!key.startsWith(SWITCH_TOOL_PREFIX)) continue
                val screen = SettingsSubscreen.valueOf(key.removePrefix(SWITCH_TOOL_PREFIX))
                // the Turnip/Freedreno settings only apply to Adreno GPUs, as on Eden's home screen
                if (screen == SettingsSubscreen.FREEDRENO_SETTINGS && !SwitchLauncher.isAdrenoGpu()) {
                    preference.isVisible = false
                    continue
                }
                preference.setOnPreferenceClickListener {
                    viewLifecycleOwner.lifecycleScope.launch {
                        SwitchLauncher.prepare(requireContext())
                        startActivity(SwitchLauncher.toolIntent(requireContext(), screen))
                    }
                    true
                }
            }
        }
        listOf("switch_share_log" to false, "switch_share_gpu_log" to true).forEach { (key, gpu) ->
            findPreference<Preference>(key)?.setOnPreferenceClickListener {
                val share = SwitchLauncher.shareLogIntent(requireContext(), gpu)
                if (share != null) {
                    startActivity(share)
                } else {
                    Toast.makeText(requireContext(), if (gpu) org.yuzu.yuzu_emu.R.string.share_gpu_log_missing else org.yuzu.yuzu_emu.R.string.share_log_missing, Toast.LENGTH_SHORT).show()
                }
                true
            }
        }
        findPreference<Preference>("switch_import_data")?.setOnPreferenceClickListener {
            withFolder {
                val oldData = withContext(Dispatchers.IO) { SwitchUserDataImport.oldDataEntries(requireContext()) }.isNotEmpty()
                val choices = listOfNotNull(
                    R.string.switch_import_old_data.takeIf { oldData },
                    R.string.switch_import_backup,
                )
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.switch_import_data)
                    .setItems(choices.map { getString(it) }.toTypedArray()) { _, which ->
                        when (choices[which]) {
                            R.string.switch_import_old_data -> importSwitchData { SwitchUserDataImport.importOldData(it) }
                            else -> switchBackupLauncher.launch(arrayOf("application/zip", "application/x-zip-compressed"))
                        }
                    }
                    .setNegativeButton(R.string.cancel, null)
                    .show()
            }
            true
        }
        preferenceScreen.findPreference<androidx.preference.PreferenceCategory>("gc_wii_settings")?.let { category ->
            for (i in 0 until category.preferenceCount) {
                val preference = category.getPreference(i)
                val key = preference.key ?: continue
                if (key.startsWith(GC_WII_TOOL_PREFIX)) {
                    val tool = GcWiiToolDialogActivity.Tool.valueOf(key.removePrefix(GC_WII_TOOL_PREFIX))
                    preference.setOnPreferenceClickListener {
                        startActivity(GcWiiToolDialogActivity.intent(requireContext(), tool))
                        true
                    }
                    continue
                }
                if (!key.startsWith(GC_WII_PREFIX)) continue
                val section = GcWiiSettingsMenu.valueOf(key.removePrefix(GC_WII_PREFIX))
                preference.setOnPreferenceClickListener {
                    GcWiiLauncher.openSettings(requireContext(), section)
                    true
                }
            }
        }
        updateFolderSummary()
    }

    private fun updateFolderSummary() {
        findPreference<Preference>("pomegrade_folder_info")?.summary =
            pomegradeFolder.path()?.takeIf { folderSetup.isSetUp() }?.absolutePath ?: getString(R.string.pomegrade_folder_not_set)
    }

    // the 3DS core reads its settings from its folder: set up first if needed
    private fun openThreeDsSettings(section: String) {
        withThreeDsFolder { org.citra.citra_emu.features.settings.ui.SettingsActivity.launch(requireContext(), section, "") }
    }

    private fun withThreeDsFolder(action: () -> Unit) {
        if (N3dsLauncher.isReady(requireContext())) {
            action()
        } else {
            folderSetup.start { ready ->
                updateFolderSummary()
                if (ready && N3dsLauncher.isReady(requireContext())) action()
            }
        }
    }

    // the 3DS core's folder as a path (Pomegrade/3DS, or where it was set up before), or null
    private fun threeDsFolder() = PomegradeFolder.documentIdOf(requireContext(), PermissionsHandler.citraDirectory)?.let { PomegradeFolder.pathOf(it) }

    // what the zip holds is shown first: each game it mods, recognised against the mods installed (new, the same zip
    // again, or an update of another mod, and the copies of a mod found for it), and whether it replaces a save
    private fun readMod(zip: Uri) {
        val resolver = requireContext().contentResolver
        val open = { resolver.openInputStream(zip) ?: throw IOException("can't open $zip") }
        val name = runCatching {
            resolver.query(zip, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { if (it.moveToFirst()) it.getString(0) else null }
        }.getOrNull()?.removeSuffix(".zip") ?: ""
        val folder = threeDsFolder()
        val progress = progressDialog(R.string.three_ds_mod_reading)
        viewLifecycleOwner.lifecycleScope.launch {
            val read = withContext(Dispatchers.IO) {
                runCatching {
                    val contents = ModInstaller().inspect(open)
                    contents to (folder?.let { ModInstaller().recognise(contents, it) } ?: emptyList())
                }.getOrNull()
            }
            progress.dismiss()
            val contents = read?.first
            when {
                contents == null -> toast(R.string.three_ds_mod_failed)
                !contents.isMod -> toast(R.string.three_ds_mod_not_a_mod)
                else -> {
                    val dates = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)
                    val games = read.second.joinToString("\n") { r ->
                        val line = when {
                            r.previous == null -> getString(R.string.three_ds_mod_new, r.programId)
                            r.sameZip -> getString(R.string.three_ds_mod_same, r.programId, r.previous.name, dates.format(Date(r.previous.installedAt)))
                            else -> getString(R.string.three_ds_mod_update, r.programId, r.previous.name, dates.format(Date(r.previous.installedAt)))
                        }
                        if (r.copies.isEmpty()) line else line + getString(R.string.three_ds_mod_copies, r.copies.joinToString())
                    }.ifEmpty { getString(R.string.three_ds_mod_none) }
                    var message = getString(R.string.three_ds_mod_confirm, games)
                    if (contents.saveFiles > 0) message += getString(R.string.three_ds_mod_confirm_save, contents.saveFiles)
                    AlertDialog.Builder(requireContext())
                        .setTitle(R.string.three_ds_mod_confirm_title)
                        .setMessage(message)
                        .setPositiveButton(R.string.three_ds_mod_install_action) { _, _ -> installMod(open, name) }
                        .setNegativeButton(R.string.cancel, null)
                        .show()
                }
            }
        }
    }

    private fun installMod(open: () -> InputStream, name: String) {
        val folder = threeDsFolder()
        if (folder == null) {
            toast(R.string.three_ds_mod_failed)
            return
        }
        val progress = progressDialog(R.string.three_ds_mod_installing)
        viewLifecycleOwner.lifecycleScope.launch {
            // a zip the picker's app no longer gives (SecurityException) is a failure, not a crash
            val result = withContext(Dispatchers.IO) { runCatching { ModInstaller().install(open, folder, name) }.getOrElse { ModInstaller.Result.Failed } }
            progress.dismiss()
            when (result) {
                is ModInstaller.Result.Installed -> {
                    // the 3DS core picks its mods folder when it starts: it reads its folders again (now, or before the
                    // next game when one runs), so the mod just installed is the one it loads, not a copy elsewhere
                    PomegradeFolder.onUserFolderChanged(requireContext().applicationContext)
                    Toast.makeText(requireContext(), getString(R.string.three_ds_mod_installed, result.files), Toast.LENGTH_LONG).show()
                }
                ModInstaller.Result.NotAMod -> toast(R.string.three_ds_mod_not_a_mod)
                ModInstaller.Result.Failed -> toast(R.string.three_ds_mod_failed)
            }
        }
    }

    private fun progressDialog(message: Int) = AlertDialog.Builder(requireContext())
        .setTitle(R.string.three_ds_mod_install)
        .setMessage(message)
        .setCancelable(false)
        .show()

    private fun withFolder(action: suspend () -> Unit) {
        val run = { viewLifecycleOwner.lifecycleScope.launch { action() } }
        if (folderSetup.isSetUp()) {
            run()
        } else {
            folderSetup.start { ready ->
                updateFolderSummary()
                if (ready) run()
            }
        }
    }

    /** Runs a Switch data import off the main thread, then tells its result. */
    private fun importSwitchData(import: (android.content.Context) -> SwitchUserDataImport.Result) {
        val context = requireContext().applicationContext
        viewLifecycleOwner.lifecycleScope.launch {
            toast(R.string.switch_import_running)
            val message = when (val result = withContext(Dispatchers.IO) { import(context) }) {
                is SwitchUserDataImport.Result.Done -> result.backup
                    ?.let { getString(R.string.switch_import_done_backup, result.files, it.absolutePath) }
                    ?: getString(R.string.switch_import_done, result.files)
                SwitchUserDataImport.Result.NoFolder -> getString(R.string.switch_import_no_folder)
                SwitchUserDataImport.Result.NothingToImport -> getString(R.string.switch_import_nothing)
                SwitchUserDataImport.Result.NotEdenBackup -> getString(R.string.switch_import_not_backup)
                is SwitchUserDataImport.Result.Failed -> getString(R.string.switch_import_failed, result.error.localizedMessage ?: result.error.javaClass.simpleName)
            }
            AlertDialog.Builder(requireContext())
                .setTitle(R.string.switch_import_data)
                .setMessage(message)
                .setPositiveButton(R.string.ok, null)
                .show()
        }
    }

    private fun toast(text: Int) = Toast.makeText(requireContext(), text, Toast.LENGTH_SHORT).show()

    private fun showConsoleSection(console: String) {
        findPreference<Preference>("gba_settings")?.isVisible = console == ConsoleSelectorPreference.GBA
        findPreference<Preference>("ds_settings")?.isVisible = console == ConsoleSelectorPreference.DS
        findPreference<Preference>("three_ds_settings")?.isVisible = console == ConsoleSelectorPreference.THREE_DS && N3dsLauncher.isSupported()
        findPreference<Preference>("switch_settings")?.isVisible = console == ConsoleSelectorPreference.SWITCH && SwitchLauncher.isSupported()
        findPreference<Preference>("gc_wii_settings")?.isVisible = console == ConsoleSelectorPreference.GC_WII && GcWiiLauncher.isSupported()
    }
}
