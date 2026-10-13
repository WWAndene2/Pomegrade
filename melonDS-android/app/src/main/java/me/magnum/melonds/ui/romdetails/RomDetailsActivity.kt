package me.magnum.melonds.ui.romdetails

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.net.toUri
import androidx.lifecycle.lifecycleScope
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.ConsoleType
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.TexturePackInstaller
import me.magnum.melonds.ui.common.rom.EmulatorLaunchValidatorDelegate
import me.magnum.melonds.ui.emulator.EmulatorActivity
import me.magnum.melonds.ui.romdetails.ui.RomDetailsScreen
import me.magnum.melonds.ui.theme.MelonTheme

@AndroidEntryPoint
class RomDetailsActivity : AppCompatActivity() {

    companion object {
        const val KEY_ROM = "rom"
    }

    private val romDetailsViewModel by viewModels<RomDetailsViewModel>()
    private val romRetroAchievementsViewModel by viewModels<RomDetailsRetroAchievementsViewModel>()

    // Pomegrade: the texture pack picked for the game shown
    private val texturePackPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { pack ->
        if (pack != null) installTexturePack(romDetailsViewModel.rom.value, pack)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val emulatorLauncherValidatorDelegate = EmulatorLaunchValidatorDelegate(this, object : EmulatorLaunchValidatorDelegate.Callback {
            override fun onRomValidated(rom: Rom) {
                val intent = EmulatorActivity.getRomEmulatorActivityIntent(this@RomDetailsActivity, rom)
                startActivity(intent)
            }

            override fun onFirmwareValidated(consoleType: ConsoleType) {
                // Do nothing (can't launch firmware fro this screen)
            }

            override fun onValidationAborted() {
                // Do nothing
            }
        })

        setContent {
            val rom by romDetailsViewModel.rom.collectAsState()
            val romConfig by romDetailsViewModel.romConfigUiState.collectAsState()

            val retroAchievementsUiState by romRetroAchievementsViewModel.uiState.collectAsState()

            LaunchedEffect(null) {
                romRetroAchievementsViewModel.viewAchievementEvent.collect {
                    launchViewAchievementIntent(it)
                }
            }

            MelonTheme {
                RomDetailsScreen(
                    rom = rom,
                    romConfigUiState = romConfig,
                    retroAchievementsUiState = retroAchievementsUiState,
                    onNavigateBack = { onNavigateUp() },
                    onLaunchRom = {
                        emulatorLauncherValidatorDelegate.validateRom(it)
                    },
                    onRomConfigUpdate = {
                        romDetailsViewModel.onRomConfigUpdateEvent(it)
                    },
                    onInstallTexturePack = {
                        texturePackPicker.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream"))
                    },
                    onRetroAchievementsLogin = { username, password ->
                        romRetroAchievementsViewModel.login(username, password)
                    },
                    onRetroAchievementsRetryLoad = {
                        romRetroAchievementsViewModel.retryLoadAchievements()
                    },
                    onViewAchievement = {
                        romRetroAchievementsViewModel.viewAchievement(it)
                    }
                )
            }
        }
    }

    private fun installTexturePack(rom: Rom, pack: Uri) {
        lifecycleScope.launch {
            val result = withContext(Dispatchers.IO) { TexturePackInstaller.install(this@RomDetailsActivity, rom, pack) }
            val message = when (result) {
                is TexturePackInstaller.Result.Installed -> {
                    val installed = getString(R.string.texture_pack_installed, result.files, result.folder.path)
                    if (result.enableHint) installed + "\n\n" + getString(R.string.texture_pack_enable_hint) else installed
                }
                TexturePackInstaller.Result.NoGameId -> getString(R.string.texture_pack_no_game_id)
                TexturePackInstaller.Result.NoFolder -> getString(R.string.texture_pack_no_folder)
                TexturePackInstaller.Result.Empty -> getString(R.string.texture_pack_empty)
                is TexturePackInstaller.Result.Failed -> getString(R.string.texture_pack_failed, result.error.localizedMessage ?: result.error.javaClass.simpleName)
            }
            AlertDialog.Builder(this@RomDetailsActivity)
                .setTitle(R.string.texture_pack_install)
                .setMessage(message)
                .setPositiveButton(R.string.ok, null)
                .show()
        }
    }

    private fun launchViewAchievementIntent(achievementUrl: String) {
        val intent = Intent(Intent.ACTION_VIEW).apply {
            data = achievementUrl.toUri()
        }
        startActivity(intent)
    }
}