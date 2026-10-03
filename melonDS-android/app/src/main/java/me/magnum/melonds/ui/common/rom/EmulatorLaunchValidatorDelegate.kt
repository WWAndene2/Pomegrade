package me.magnum.melonds.ui.common.rom

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.launch
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.ConfigurationDirResult
import me.magnum.melonds.domain.model.ConsoleType
import me.magnum.melonds.domain.model.emulator.validation.FirmwareLaunchPreconditionCheckResult
import me.magnum.melonds.domain.model.emulator.validation.RomLaunchPreconditionCheckResult
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.common.rom.model.LaunchValidationResult
import me.magnum.melonds.ui.dsiwaremanager.DSiWareManagerActivity
import me.magnum.melonds.ui.settings.SettingsActivity

class EmulatorLaunchValidatorDelegate(
    private val context: ComponentActivity,
    private val callback: Callback,
) {

    private val viewModel by context.viewModels<EmulatorLaunchValidationViewModel>()

    private val firmwareSettingsLauncher = context.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        viewModel.onReturnFromFirmwareSettings()
    }
    private val dsiWareManagerLauncher = context.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        viewModel.onReturnFromDsiWareManagerSetup()
    }
    // the 3DS game waiting for its file access or folder to be set up (lost if the activity is
    // recreated meanwhile: what was set up stays, the game is started with one more tap)
    private var pendingN3dsRom: Rom? = null
    private val n3dsAllFilesAccessLauncher = context.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        resumeN3dsSetup(N3dsLauncher.hasFileAccess(context))
    }
    private val n3dsStoragePermissionLauncher = context.registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        resumeN3dsSetup(granted)
    }
    private val n3dsFolderLauncher: ActivityResultLauncher<Uri?> = context.registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { folder ->
        val rom = pendingN3dsRom
        pendingN3dsRom = null
        if (folder == null) {
            callback.onValidationAborted()
            return@registerForActivityResult
        }
        if (N3dsLauncher.setUpFolder(context, folder)) {
            if (rom != null) N3dsLauncher.launch(context, rom)
        } else {
            AlertDialog.Builder(context)
                .setTitle(R.string.three_ds_folder_title)
                .setMessage(R.string.three_ds_folder_invalid)
                .setPositiveButton(R.string.three_ds_folder_choose) { _, _ ->
                    pendingN3dsRom = rom
                    n3dsFolderLauncher.launch(null)
                }
                .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
                .setOnCancelListener { callback.onValidationAborted() }
                .show()
        }
    }

    init {
        context.lifecycleScope.launch {
            context.lifecycle.repeatOnLifecycle(Lifecycle.State.CREATED) {
                viewModel.romValidationResult.collect {
                    when (it) {
                        is LaunchValidationResult.Firmware -> {
                            when (it.result) {
                                is FirmwareLaunchPreconditionCheckResult.Success -> callback.onFirmwareValidated(it.result.consoleType)
                                is FirmwareLaunchPreconditionCheckResult.BiosConfigurationIncorrect -> {
                                    showIncorrectConfigurationDirectoryDialogForFirmware(it.result.configurationDirectoryResult)
                                }
                            }
                        }
                        is LaunchValidationResult.Rom -> {
                            when (it.result) {
                                is RomLaunchPreconditionCheckResult.Success -> callback.onRomValidated(it.result.rom)
                                is RomLaunchPreconditionCheckResult.DSiWareTitleValidationFailed -> showDsiWareTitleLoadIssueDialog(it.result.reason)
                                is RomLaunchPreconditionCheckResult.BiosConfigurationIncorrect -> {
                                    showIncorrectConfigurationDirectoryDialogForRom(it.result.configurationDirectoryResult)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fun validateRom(rom: Rom) {
        // 3DS games don't use the DS BIOS/firmware checks, they go straight to the 3DS core,
        // once its folder is set up
        if (rom.platform == RomPlatform.N3DS) {
            when {
                !N3dsLauncher.isSupported() -> N3dsLauncher.launch(context, rom) // explains why not
                !N3dsLauncher.hasFileAccess(context) -> showN3dsFileAccessDialog(rom)
                !N3dsLauncher.isFolderReady(context) -> showN3dsFolderSetupDialog(rom)
                else -> N3dsLauncher.launch(context, rom)
            }
            return
        }
        viewModel.validateRomForLaunch(rom)
    }

    fun validateFirmware(consoleType: ConsoleType) {
        viewModel.validateFirmwareForLaunch(consoleType)
    }

    private fun showN3dsFileAccessDialog(rom: Rom) {
        AlertDialog.Builder(context)
            .setTitle(R.string.three_ds_file_access_title)
            .setMessage(R.string.three_ds_file_access_message)
            .setPositiveButton(R.string.three_ds_file_access_allow) { _, _ ->
                pendingN3dsRom = rom
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    try {
                        n3dsAllFilesAccessLauncher.launch(N3dsLauncher.allFilesAccessSettings(context))
                    } catch (e: ActivityNotFoundException) {
                        n3dsAllFilesAccessLauncher.launch(N3dsLauncher.allFilesAccessSettingsList())
                    }
                } else {
                    n3dsStoragePermissionLauncher.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                }
            }
            .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
            .setOnCancelListener { callback.onValidationAborted() }
            .show()
    }

    private fun resumeN3dsSetup(granted: Boolean) {
        val rom = pendingN3dsRom
        pendingN3dsRom = null
        when {
            rom == null -> callback.onValidationAborted()
            granted -> validateRom(rom) // on to the folder, then the game
            else -> {
                Toast.makeText(context, R.string.three_ds_file_access_denied, Toast.LENGTH_LONG).show()
                callback.onValidationAborted()
            }
        }
    }

    private fun showN3dsFolderSetupDialog(rom: Rom) {
        AlertDialog.Builder(context)
            .setTitle(R.string.three_ds_folder_title)
            .setMessage(R.string.three_ds_folder_message)
            .setPositiveButton(R.string.three_ds_folder_choose) { _, _ ->
                pendingN3dsRom = rom
                n3dsFolderLauncher.launch(null)
            }
            .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
            .setOnCancelListener { callback.onValidationAborted() }
            .show()
    }

    private fun showIncorrectConfigurationDirectoryDialogForFirmware(configurationDirResult: ConfigurationDirResult) {
        AlertDialog.Builder(context)
            .setTitle(R.string.firmware_launch_failed)
            .setMessage(R.string.firmware_launch_bad_setup)
            .setPositiveButton(R.string.settings) { _, _ ->
                val intent = Intent(context, SettingsActivity::class.java).apply {
                    putExtra(SettingsActivity.KEY_ENTRY_POINT, SettingsActivity.CUSTOM_FIRMWARE_ENTRY_POINT)
                }
                firmwareSettingsLauncher.launch(intent)
            }
            .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
            .setOnCancelListener { callback.onValidationAborted() }
            .show()
    }

    private fun showIncorrectConfigurationDirectoryDialogForRom(configurationDirResult: ConfigurationDirResult) {
        AlertDialog.Builder(context)
            .setTitle(R.string.rom_launch_failed)
            .setMessage(R.string.rom_launch_custom_bios_firmware_bad_setup)
            .setPositiveButton(R.string.settings) { _, _ ->
                val intent = Intent(context, SettingsActivity::class.java).apply {
                    putExtra(SettingsActivity.KEY_ENTRY_POINT, SettingsActivity.CUSTOM_FIRMWARE_ENTRY_POINT)
                }
                firmwareSettingsLauncher.launch(intent)
            }
            .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
            .setOnCancelListener { callback.onValidationAborted() }
            .show()
    }

    private fun showDsiWareTitleLoadIssueDialog(reason: RomLaunchPreconditionCheckResult.DSiWareTitleValidationFailed.Reason) {
        val message = when (reason) {
            RomLaunchPreconditionCheckResult.DSiWareTitleValidationFailed.Reason.NandError -> context.getString(R.string.failed_launch_dsiware_title_check_failed)
            RomLaunchPreconditionCheckResult.DSiWareTitleValidationFailed.Reason.RomParseError -> context.getString(R.string.failed_launch_dsiware_title_rom_failed)
            RomLaunchPreconditionCheckResult.DSiWareTitleValidationFailed.Reason.TitleNotInstalled -> context.getString(R.string.failed_launch_dsiware_title_not_installed)
        }

        AlertDialog.Builder(context)
            .setTitle(R.string.failed_launch_dsiware_title)
            .setMessage(message)
            .setPositiveButton(R.string.dsiware_manager) { _, _ ->
                val intent = Intent(context, DSiWareManagerActivity::class.java)
                dsiWareManagerLauncher.launch(intent)
            }
            .setNegativeButton(R.string.cancel) { _, _ -> callback.onValidationAborted() }
            .setOnCancelListener {
                callback.onValidationAborted()
            }
            .show()
    }

    private fun showInvalidDirectoryAccessDialog() {
        AlertDialog.Builder(context)
            .setTitle(R.string.error_invalid_directory)
            .setMessage(R.string.error_invalid_directory_description)
            .setPositiveButton(R.string.ok, null)
            .setOnDismissListener { callback.onValidationAborted() }
            .show()
    }

    interface Callback {
        fun onRomValidated(rom: Rom)
        fun onFirmwareValidated(consoleType: ConsoleType)
        fun onValidationAborted()
    }
}