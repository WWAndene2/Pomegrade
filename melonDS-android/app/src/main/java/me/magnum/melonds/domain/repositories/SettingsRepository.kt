package me.magnum.melonds.domain.repositories

import android.net.Uri
import me.magnum.melonds.domain.model.rom.RomPlatform
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.StateFlow
import me.magnum.melonds.domain.model.*
import me.magnum.melonds.domain.model.camera.DSiCameraSourceType
import me.magnum.melonds.domain.model.input.SoftInputBehaviour
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.ui.Theme
import java.util.UUID

interface SettingsRepository {
    suspend fun getEmulatorConfiguration(): EmulatorConfiguration

    fun getTheme(): Theme
    fun getFastForwardSpeedMultiplier(): Float
    fun isRewindEnabled(): Boolean
    fun isSustainedPerformanceModeEnabled(): Boolean
    // Pomegrade: intermediate frames, shown at 120 Hz (OpenGL renderer)
    fun isFrameGenerationEnabled(): Boolean

    fun getRomSearchDirectories(): Array<Uri>
    fun clearRomSearchDirectories()
    fun getRomIconFiltering(): RomIconFiltering
    fun getRomCacheMaxSize(): SizeUnit

    fun getDefaultConsoleType(): ConsoleType
    fun getFirmwareConfiguration(): FirmwareConfiguration
    fun useCustomBios(): Boolean
    fun getDsBiosDirectory(): Uri?
    fun getDsiBiosDirectory(): Uri?
    fun showBootScreen(): Boolean
    fun isJitEnabled(): Boolean

    fun getVideoRenderer(): Flow<VideoRenderer>
    fun getVideoInternalResolutionScaling(): Flow<Int>
    fun getVideoFiltering(): Flow<VideoFiltering>
    fun isThreadedRenderingEnabled(): Flow<Boolean>
    fun getFpsCounterPosition(): FpsCounterPosition
    fun isPerformanceDetailsEnabled(): Boolean
    fun getDSiCameraSource(): DSiCameraSourceType
    fun getDSiCameraStaticImage(): Uri?

    fun isSoundEnabled(): Boolean
    fun getAudioLatency(): AudioLatency
    fun getMicSource(): MicSource

    fun getRomSortingMode(): SortingMode
    fun getRomSortingOrder(): SortingOrder
    fun saveNextToRomFile(): Boolean
    fun getSaveFileDirectory(): Uri?
    fun getSaveFileDirectory(rom: Rom): Uri
    fun getSaveStateLocation(rom: Rom): SaveStateLocation
    fun getSaveStateDirectory(rom: Rom): Uri?

    fun getControllerConfiguration(): ControllerConfiguration
    fun observeControllerConfiguration(): StateFlow<ControllerConfiguration>
    fun getSelectedLayoutId(): UUID
    fun getSoftInputBehaviour(): Flow<SoftInputBehaviour>
    fun isTouchHapticFeedbackEnabled(): Flow<Boolean>
    fun getTouchHapticFeedbackStrength(): Int
    fun getSoftInputOpacity(): Flow<Int>

    /** Pomegrade: the touch D-pad becomes a joystick, gamepad sticks are read as analogue */
    fun isAnalogueMovementEnabled(): Flow<Boolean>

    fun isRetroAchievementsRichPresenceEnabled(): Boolean
    fun isRetroAchievementsHardcoreEnabled(): Boolean
    fun areRetroAchievementsActiveChallengeIndicatorsEnabled(): Boolean
    fun areRetroAchievementsProgressIndicatorsEnabled(): Boolean
    fun areRetroAchievementsLeaderboardIndicatorsEnabled(): Boolean

    fun areCheatsEnabled(): Boolean

    fun observeTheme(): Flow<Theme>
    fun observeRomIconFiltering(): Flow<RomIconFiltering>
    fun observeRomSearchDirectories(): Flow<Array<Uri>>
    fun observeSelectedLayoutId(): Flow<UUID>
    fun observeDSiCameraSource(): Flow<DSiCameraSourceType>
    fun observeDSiCameraStaticImage(): Flow<Uri?>

    fun setDsBiosDirectory(directoryUri: Uri)
    fun setDsiBiosDirectory(directoryUri: Uri)
    fun addRomSearchDirectory(directoryUri: Uri)

    // Pomegrade folder: the folder Pomegrade keeps games and their files in (see PomegradeFolder)
    fun getPomegradeFolder(): Uri?
    fun setPomegradeFolder(folderUri: Uri)
    // adds a ROM folder, keeping the others (addRomSearchDirectory replaces them)
    fun includeRomSearchDirectory(directoryUri: Uri)
    // DS saves and save states next to each ROM (in its game folder)
    fun keepGameFilesNextToRom()
    // the game list's console filter: null = every console
    fun getRomPlatformFilter(): RomPlatform?
    fun setRomPlatformFilter(platform: RomPlatform?)
    fun setControllerConfiguration(controllerConfiguration: ControllerConfiguration)
    fun setRomSortingMode(sortingMode: SortingMode)
    fun setRomSortingOrder(sortingOrder: SortingOrder)
    fun setSelectedLayoutId(layoutId: UUID)

    fun observeRenderConfiguration(): Flow<RendererConfiguration>
}
