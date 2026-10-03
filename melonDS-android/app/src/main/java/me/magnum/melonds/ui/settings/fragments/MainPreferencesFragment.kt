package me.magnum.melonds.ui.settings.fragments

import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.ui.common.rom.N3dsLauncher
import me.magnum.melonds.ui.settings.PreferenceFragmentTitleProvider
import me.magnum.melonds.ui.settings.SettingsActivity
import javax.inject.Inject

@AndroidEntryPoint
class MainPreferencesFragment : BasePreferenceFragment(), PreferenceFragmentTitleProvider {

    companion object {
        // keys of the 3DS entries: this prefix + the Azahar settings section they open
        private const val THREE_DS_PREFIX = "three_ds_settings_"
    }

    @Inject lateinit var pomegradeFolder: PomegradeFolder

    private val folderSetup get() = (requireActivity() as SettingsActivity).pomegradeFolderSetup

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
        preferenceScreen.findPreference<androidx.preference.PreferenceCategory>("three_ds_settings")?.let { category ->
            for (i in 0 until category.preferenceCount) {
                val preference = category.getPreference(i)
                val section = preference.key?.removePrefix(THREE_DS_PREFIX) ?: continue
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
        val open = { org.citra.citra_emu.features.settings.ui.SettingsActivity.launch(requireContext(), section, "") }
        if (N3dsLauncher.isReady(requireContext())) {
            open()
        } else {
            folderSetup.start { ready ->
                updateFolderSummary()
                if (ready && N3dsLauncher.isReady(requireContext())) open()
            }
        }
    }

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
