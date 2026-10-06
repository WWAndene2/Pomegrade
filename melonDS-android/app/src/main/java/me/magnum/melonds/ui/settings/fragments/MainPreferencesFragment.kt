package me.magnum.melonds.ui.settings.fragments

import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.impl.ModInstaller
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.ui.common.rom.N3dsLauncher
import me.magnum.melonds.ui.settings.PreferenceFragmentTitleProvider
import me.magnum.melonds.ui.settings.SettingsActivity
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.utils.PermissionsHandler
import java.io.IOException
import java.io.InputStream
import javax.inject.Inject

@AndroidEntryPoint
class MainPreferencesFragment : BasePreferenceFragment(), PreferenceFragmentTitleProvider {

    companion object {
        // keys of the 3DS entries: this prefix + the Azahar settings section they open
        private const val THREE_DS_PREFIX = "three_ds_settings_"
    }

    @Inject lateinit var pomegradeFolder: PomegradeFolder

    private val folderSetup get() = (requireActivity() as SettingsActivity).pomegradeFolderSetup

    // a 3DS mod's zip, picked with the system's file picker
    private val modZipLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { zip ->
        if (zip != null) readMod(zip)
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

        // 3DS: Azahar's own settings, the section of each entry; 64-bit devices only
        findPreference<Preference>("three_ds_settings")?.isVisible = N3dsLauncher.isSupported()
        // the code trace is written next to the 3DS core's folder: its path is kept with the
        // setting, for Azahar's emulation screen
        findPreference<androidx.preference.SwitchPreference>(EmulationActivity.CODE_TRACE)?.setOnPreferenceChangeListener { _, value ->
            if (value != true) return@setOnPreferenceChangeListener true
            withThreeDsFolder {
                val traces = threeDsFolder()?.let { java.io.File(it, "Traces") }
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

    // what the zip holds is shown first: the games it mods, and whether it replaces a save
    private fun readMod(zip: Uri) {
        val resolver = requireContext().contentResolver
        val open = { resolver.openInputStream(zip) ?: throw IOException("can't open $zip") }
        val progress = progressDialog(R.string.three_ds_mod_reading)
        viewLifecycleOwner.lifecycleScope.launch {
            val contents = withContext(Dispatchers.IO) { runCatching { ModInstaller().inspect(open) }.getOrNull() }
            progress.dismiss()
            when {
                contents == null -> toast(R.string.three_ds_mod_failed)
                !contents.isMod -> toast(R.string.three_ds_mod_not_a_mod)
                else -> {
                    val games = contents.programIds.sorted().joinToString().ifEmpty { getString(R.string.three_ds_mod_none) }
                    var message = getString(R.string.three_ds_mod_confirm, games)
                    if (contents.saveFiles > 0) message += getString(R.string.three_ds_mod_confirm_save, contents.saveFiles)
                    AlertDialog.Builder(requireContext())
                        .setTitle(R.string.three_ds_mod_confirm_title)
                        .setMessage(message)
                        .setPositiveButton(R.string.three_ds_mod_install_action) { _, _ -> installMod(open) }
                        .setNegativeButton(R.string.cancel, null)
                        .show()
                }
            }
        }
    }

    private fun installMod(open: () -> InputStream) {
        val folder = threeDsFolder()
        if (folder == null) {
            toast(R.string.three_ds_mod_failed)
            return
        }
        val progress = progressDialog(R.string.three_ds_mod_installing)
        viewLifecycleOwner.lifecycleScope.launch {
            // a zip the picker's app no longer gives (SecurityException) is a failure, not a crash
            val result = withContext(Dispatchers.IO) { runCatching { ModInstaller().install(open, folder) }.getOrElse { ModInstaller.Result.Failed } }
            progress.dismiss()
            when (result) {
                is ModInstaller.Result.Installed ->
                    Toast.makeText(requireContext(), getString(R.string.three_ds_mod_installed, result.files), Toast.LENGTH_LONG).show()
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

    private fun toast(text: Int) = Toast.makeText(requireContext(), text, Toast.LENGTH_SHORT).show()
}
