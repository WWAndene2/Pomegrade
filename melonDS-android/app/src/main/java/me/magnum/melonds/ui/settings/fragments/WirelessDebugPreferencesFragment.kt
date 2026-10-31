package me.magnum.melonds.ui.settings.fragments

import android.Manifest
import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.lifecycleScope
import androidx.preference.ListPreference
import androidx.preference.Preference
import kotlinx.coroutines.launch
import me.magnum.melonds.R
import me.magnum.melonds.impl.diagnostics.DiagnosticCommand
import me.magnum.melonds.impl.diagnostics.WirelessDebugDiagnostics
import me.magnum.melonds.impl.diagnostics.WirelessDebugPairingNotification
import me.magnum.melonds.ui.settings.PreferenceFragmentTitleProvider

/**
 * Diagnostics over wireless debugging (owner's decision, 29 October): off until paired, for this session only, and
 * limited to the fixed [DiagnosticCommand]s.
 */
class WirelessDebugPreferencesFragment : BasePreferenceFragment(), PreferenceFragmentTitleProvider {

    private val notificationPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) startPairing() else toast(getString(R.string.wireless_debug_notifications_needed))
    }

    override fun getTitle() = getString(R.string.wireless_debug)

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.pref_wireless_debug, rootKey)

        findPreference<Preference>("wireless_debug_pair")?.setOnPreferenceClickListener {
            when {
                !WirelessDebugDiagnostics.isSupported -> toast(getString(R.string.wireless_debug_unsupported))
                WirelessDebugPairingNotification.canPost(requireContext()) -> startPairing()
                else -> notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
            true
        }
        findPreference<Preference>("wireless_debug_full")?.setOnPreferenceClickListener {
            runCommand(DiagnosticCommand.FULL_ANALYSIS, traceDelay())
            true
        }
        findPreference<Preference>("wireless_debug_trace")?.setOnPreferenceClickListener {
            runCommand(DiagnosticCommand.SYSTEM_TRACE, traceDelay())
            true
        }
        findPreference<Preference>("wireless_debug_gpu_state")?.setOnPreferenceClickListener {
            runCommand(DiagnosticCommand.GPU_STATE)
            true
        }
        findPreference<Preference>("wireless_debug_driver_log")?.setOnPreferenceClickListener {
            runCommand(DiagnosticCommand.DRIVER_LOG)
            true
        }
        findPreference<Preference>("wireless_debug_disconnect")?.setOnPreferenceClickListener {
            lifecycleScope.launch {
                WirelessDebugDiagnostics.disconnect()
                WirelessDebugPairingNotification.cancel(requireContext())
                updateState()
            }
            true
        }
    }

    override fun onResume() {
        super.onResume()
        // the pairing finishes in the notification, while this screen is behind Android's settings
        updateState()
    }

    private fun traceDelay(): Int =
        findPreference<ListPreference>("wireless_debug_trace_delay")?.value?.toIntOrNull() ?: 60

    private fun startPairing() {
        WirelessDebugDiagnostics.startPairing(requireContext())
        WirelessDebugPairingNotification.show(requireContext())
        toast(getString(R.string.wireless_debug_pairing_started))
        // Developer options: Wireless debugging is in it; its own screen has no public intent
        runCatching { startActivity(Intent(Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS)) }
    }

    private fun runCommand(command: DiagnosticCommand, delaySeconds: Int = 0) {
        lifecycleScope.launch {
            when (val result = WirelessDebugDiagnostics.run(requireContext(), command, delaySeconds)) {
                is WirelessDebugDiagnostics.Result.Written ->
                    toast(getString(R.string.wireless_debug_written, result.file.absolutePath))
                is WirelessDebugDiagnostics.Result.Started -> toast(
                    getString(
                        if (command == DiagnosticCommand.FULL_ANALYSIS) R.string.wireless_debug_full_started else R.string.wireless_debug_trace_started,
                        result.delaySeconds, DiagnosticCommand.TRACE_SECONDS, result.folder.absolutePath,
                    )
                )
                is WirelessDebugDiagnostics.Result.Failed ->
                    toast(getString(R.string.wireless_debug_failed, result.reason))
            }
            updateState()
        }
    }

    // the commands only while connected; pairing only while not
    private fun updateState() {
        val connected = WirelessDebugDiagnostics.isConnected
        findPreference<Preference>("wireless_debug_status")?.summary = getString(
            if (connected) R.string.wireless_debug_status_connected else R.string.wireless_debug_status_off
        )
        findPreference<Preference>("wireless_debug_pair")?.isEnabled = !connected
        listOf("wireless_debug_trace_delay", "wireless_debug_full", "wireless_debug_trace", "wireless_debug_gpu_state", "wireless_debug_driver_log", "wireless_debug_disconnect")
            .forEach { findPreference<Preference>(it)?.isEnabled = connected }
    }

    private fun toast(text: String) {
        Toast.makeText(requireContext(), text, Toast.LENGTH_LONG).show()
    }
}
