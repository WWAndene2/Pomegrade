package me.magnum.melonds.ui.emulator

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.hardware.display.DisplayManager
import android.hardware.input.InputManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.view.Display
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.Window
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.isGone
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import androidx.drawerlayout.widget.DrawerLayout
import androidx.lifecycle.DEFAULT_ARGS_KEY
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.lifecycle.viewmodel.MutableCreationExtras
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import dagger.hilt.android.AndroidEntryPoint
import io.github.wwandene2.pomegrade.emulatorui.EmulatorMenu
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import io.github.wwandene2.pomegrade.emulatorui.SaveStateSlotUi
import io.github.wwandene2.pomegrade.emulatorui.SaveStatesDialog
import kotlin.math.abs
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.filterIsInstance
import kotlinx.coroutines.launch
import me.magnum.melonds.MelonEmulator
import me.magnum.melonds.R
import me.magnum.melonds.common.PermissionHandler
import me.magnum.melonds.databinding.ActivityEmulatorBinding
import me.magnum.melonds.domain.model.ConsoleType
import me.magnum.melonds.domain.model.ControllerConfiguration
import me.magnum.melonds.domain.model.FpsCounterPosition
import me.magnum.melonds.domain.model.Rect
import me.magnum.melonds.domain.model.SaveStateSlot
import me.magnum.melonds.domain.model.layout.Insets
import me.magnum.melonds.domain.model.layout.LayoutComponent
import me.magnum.melonds.domain.model.layout.ScreenFold
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.ui.Orientation
import me.magnum.melonds.extensions.insetsControllerCompat
import me.magnum.melonds.extensions.setLayoutOrientation
import me.magnum.melonds.impl.RomIconProvider
import me.magnum.melonds.impl.emulator.LifecycleOwnerProvider
import me.magnum.melonds.impl.layout.DeviceLayoutDisplayMapper
import me.magnum.melonds.impl.layout.SecondaryDisplaySelector
import me.magnum.melonds.impl.system.AppForegroundStateObserver
import me.magnum.melonds.parcelables.RomInfoParcelable
import me.magnum.melonds.parcelables.RomParcelable
import me.magnum.melonds.ui.cheats.CheatsActivity
import me.magnum.melonds.ui.common.rom.EmulatorLaunchValidatorDelegate
import me.magnum.melonds.ui.emulator.component.EmulatorOverlayTracker
import me.magnum.melonds.ui.emulator.firmware.FirmwarePauseMenuOption
import me.magnum.melonds.ui.emulator.input.ConnectedControllerManager
import me.magnum.melonds.ui.emulator.input.EmulatorRumbleManager
import me.magnum.melonds.ui.emulator.input.FrontendInputHandler
import me.magnum.melonds.ui.emulator.input.INativeInputListener
import me.magnum.melonds.ui.emulator.input.InputProcessor
import me.magnum.melonds.ui.emulator.input.MelonTouchHandler
import me.magnum.melonds.ui.emulator.model.EmulatorOverlay
import me.magnum.melonds.ui.emulator.model.EmulatorState
import me.magnum.melonds.ui.emulator.model.EmulatorUiEvent
import me.magnum.melonds.ui.emulator.model.LaunchArgs
import me.magnum.melonds.ui.emulator.model.PauseMenu
import me.magnum.melonds.ui.emulator.model.RAEventUi
import me.magnum.melonds.ui.emulator.model.RumbleEvent
import me.magnum.melonds.ui.emulator.model.RuntimeInputLayoutConfiguration
import me.magnum.melonds.ui.emulator.model.ToastEvent
import me.magnum.melonds.ui.emulator.render.ChoreographerFrameRenderer
import me.magnum.melonds.ui.emulator.render.ChoreographerFrameRendererFactory
import me.magnum.melonds.ui.emulator.render.ExternalPresentation
import me.magnum.melonds.ui.emulator.render.FrameRenderCoordinator
import me.magnum.melonds.ui.emulator.rewind.EdgeSpacingDecorator
import me.magnum.melonds.ui.emulator.rewind.RewindSaveStateAdapter
import me.magnum.melonds.ui.emulator.rom.RomPauseMenuOption
import me.magnum.melonds.ui.emulator.rewind.model.RewindWindow
import me.magnum.melonds.ui.emulator.ui.AchievementListDialog
import me.magnum.melonds.ui.emulator.ui.AchievementUpdatesUi
import me.magnum.melonds.ui.emulator.ui.PendingSubmissionsDialog
import me.magnum.melonds.ui.layouteditor.model.LayoutTarget
import me.magnum.melonds.ui.settings.SettingsActivity
import me.magnum.melonds.ui.theme.MelonTheme
import javax.inject.Inject

@AndroidEntryPoint
class EmulatorActivity : AppCompatActivity() {
    companion object {
        const val KEY_ROM = "rom"
        const val KEY_PATH = "PATH"
        const val KEY_URI = "uri"
        const val KEY_BOOT_FIRMWARE_CONSOLE = "boot_firmware_console"
        const val KEY_BOOT_FIRMWARE_ONLY = "boot_firmware_only"

        fun getRomEmulatorActivityIntent(context: Context, rom: Rom): Intent {
            return Intent(context, EmulatorActivity::class.java).apply {
                putExtra(KEY_ROM, RomParcelable(rom))
            }
        }

        fun getFirmwareEmulatorActivityIntent(context: Context, consoleType: ConsoleType): Intent {
            return Intent(context, EmulatorActivity::class.java).apply {
                putExtra(KEY_BOOT_FIRMWARE_ONLY, true)
                putExtra(KEY_BOOT_FIRMWARE_CONSOLE, consoleType.ordinal)
            }
        }
    }

    private lateinit var binding: ActivityEmulatorBinding
    private val viewModel: EmulatorViewModel by viewModels(
        extrasProducer = {
            val extras = MutableCreationExtras(defaultViewModelCreationExtras)
            // Inject intent data into view-model creation extras to make it accessible through the SavedStateHandle
            intent.data?.let { dataUri ->
                val existingExtras = extras[DEFAULT_ARGS_KEY]?.let { Bundle(it) } ?: Bundle()
                existingExtras.putString(KEY_URI, dataUri.toString())
                extras[DEFAULT_ARGS_KEY] = existingExtras
            }
            extras
        }
    )

    @Inject
    lateinit var secondaryDisplaySelector: SecondaryDisplaySelector

    @Inject
    lateinit var deviceLayoutDisplayMapper: DeviceLayoutDisplayMapper

    @Inject
    lateinit var romIconProvider: RomIconProvider

    @Inject
    lateinit var permissionHandler: PermissionHandler

    @Inject
    lateinit var lifecycleOwnerProvider: LifecycleOwnerProvider

    @Inject
    lateinit var appForegroundStateObserver: AppForegroundStateObserver

    private var presentation: ExternalPresentation? = null

    private lateinit var handler: Handler
    private val displayListener = object : DisplayManager.DisplayListener {

        override fun onDisplayAdded(displayId: Int) {
            runOnUiThread {
                updateDisplays()
            }
        }

        override fun onDisplayRemoved(displayId: Int) {
            runOnUiThread {
                updateDisplays()
            }
        }

        override fun onDisplayChanged(displayId: Int) {
            updateDisplays()
        }
    }

    private val connectedControllerManager = ConnectedControllerManager()
    private lateinit var emulatorLaunchValidatorDelegate: EmulatorLaunchValidatorDelegate
    private lateinit var emulatorRumbleManager: EmulatorRumbleManager
    private lateinit var frameRenderCoordinator: FrameRenderCoordinator
    private lateinit var choreographerFrameRenderer: ChoreographerFrameRenderer
    private lateinit var mainScreenRenderer: DSRenderer
    private lateinit var melonTouchHandler: MelonTouchHandler
    private lateinit var nativeInputListener: INativeInputListener
    private val frontendInputHandler = object : FrontendInputHandler() {
        var fastForwardEnabled = false
            private set
        var microphoneEnabled = true
            private set

        override fun onSoftInputTogglePressed() {
            binding.viewLayoutControls.toggleSoftInputVisibility()
            presentation?.layoutView?.toggleSoftInputVisibility()
        }

        override fun onPausePressed() {
            viewModel.pauseEmulator(true)
        }

        override fun onFastForwardPressed() {
            fastForwardEnabled = !fastForwardEnabled
            binding.viewLayoutControls.setLayoutComponentToggleState(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE, fastForwardEnabled)
            presentation?.layoutView?.setLayoutComponentToggleState(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE, fastForwardEnabled)
            MelonEmulator.setFastForwardEnabled(fastForwardEnabled)
        }

        override fun onMicrophonePressed() {
            microphoneEnabled = !microphoneEnabled
            binding.viewLayoutControls.setLayoutComponentToggleState(LayoutComponent.BUTTON_MICROPHONE_TOGGLE, microphoneEnabled)
            presentation?.layoutView?.setLayoutComponentToggleState(LayoutComponent.BUTTON_MICROPHONE_TOGGLE, microphoneEnabled)
            MelonEmulator.setMicrophoneEnabled(microphoneEnabled)
        }

        override fun onResetPressed() {
            viewModel.resetEmulator()
        }

        override fun onSwapScreens() {
            swapScreen()
        }

        override fun onQuickSave() {
            viewModel.doQuickSave()
        }

        override fun onQuickLoad() {
            viewModel.doQuickLoad()
        }

        override fun onRewind() {
            viewModel.onOpenRewind()
        }
    }
    private val settingsLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        viewModel.onSettingsChanged()
        setupSustainedPerformanceMode()
        setupDisplayRefreshRate()
        setupFpsCounter()
        viewModel.resumeEmulator()
    }
    private val cheatsLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        viewModel.onCheatsChanged()
        viewModel.resumeEmulator()
    }
    private val permissionRequestLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        lifecycleScope.launch {
            it.keys.forEach { permission ->
                permissionHandler.notifyPermissionStatusUpdated(permission)
            }
        }
    }
    private val backPressedCallback = object : OnBackPressedCallback(false) {
        override fun handleOnBackPressed() {
            handleBackPressed()
        }
    }

    private val rewindSaveStateAdapter = RewindSaveStateAdapter {
        viewModel.rewindToState(it)
        closeRewindWindow()
    }
    // the pause menu's entries while it is open, and whether one was picked (else closing it resumes)
    private var pauseMenuOptions: List<PauseMenuOption> = emptyList()
    private var pauseMenuOptionPicked = false
    private val showAchievementList = mutableStateOf(false)
    private val showPendingSubmissionsDialog = mutableStateOf(false)

    private val activeOverlays = EmulatorOverlayTracker(
        onOverlaysCleared = {
            disableScreenTimeOut()
            presentation?.setPauseOverlayVisibility(false)
        },
        onOverlaysPresent = {
            enableScreenTimeOut()
            presentation?.setPauseOverlayVisibility(true)
        }
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        handler = Handler(mainLooper)
        lifecycleOwnerProvider.setCurrentLifecycleOwner(this)
        binding = ActivityEmulatorBinding.inflate(layoutInflater)
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE)
        setContentView(binding.root)
        setupFullscreen()
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, windowInsets ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            binding.listRewind.setPadding(insets.left, 0, insets.right, insets.bottom)
            binding.textFps.updateLayoutParams<ConstraintLayout.LayoutParams> {
                setMargins(insets.left, insets.top, insets.right, insets.bottom)
            }

            val uiInsets = Insets(insets.left, insets.top, insets.right, insets.bottom)
            viewModel.setUiInsets(uiInsets)

            WindowInsetsCompat.CONSUMED
        }

        onBackPressedDispatcher.addCallback(backPressedCallback)
        setupInGameMenu()

        emulatorLaunchValidatorDelegate = EmulatorLaunchValidatorDelegate(this, object : EmulatorLaunchValidatorDelegate.Callback {
            override fun onRomValidated(rom: Rom) {
                viewModel.onRomLaunchValidated(rom)
            }

            override fun onFirmwareValidated(consoleType: ConsoleType) {
                viewModel.onFirmwareLaunchValidated(consoleType)
            }

            override fun onValidationAborted() {
                finish()
            }
        })
        emulatorRumbleManager = EmulatorRumbleManager(this, lifecycleScope, connectedControllerManager)
        frameRenderCoordinator = FrameRenderCoordinator()
        choreographerFrameRenderer = ChoreographerFrameRendererFactory.createFrameRenderer(frameRenderCoordinator)
        melonTouchHandler = MelonTouchHandler()
        mainScreenRenderer = DSRenderer(this)
        binding.surfaceMain.apply {
            setRenderer(mainScreenRenderer)
        }

        binding.textFps.visibility = View.INVISIBLE
        binding.viewLayoutControls.setLayoutComponentViewBuilderFactory(RuntimeLayoutComponentViewBuilderFactory())
        binding.layoutRewind.setOnClickListener {
            closeRewindWindow()
        }
        binding.listRewind.apply {
            val listLayoutManager = LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, true)
            layoutManager = listLayoutManager
            addItemDecoration(EdgeSpacingDecorator())
            adapter = rewindSaveStateAdapter
        }
        binding.viewLayoutControls.apply {
            setFrontendInputHandler(frontendInputHandler)
            setSystemInputHandler(melonTouchHandler)
        }

        val layoutChangeListener = View.OnLayoutChangeListener { _, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom ->
            val oldWith = oldRight - oldLeft
            val oldHeight = oldBottom - oldTop

            val newWidth = right - left
            val newHeight = bottom - top

            if (newWidth != oldWith || newHeight != oldHeight) {
                updateRendererScreenAreas()
                viewModel.setUiSize(newWidth, newHeight)
            }
        }
        binding.viewLayoutControls.addOnLayoutChangeListener(layoutChangeListener)

        updateOrientation(resources.configuration)
        disableScreenTimeOut()

        binding.layoutAchievement.setContent {
            MelonTheme {
                val achievementsViewModel = viewModels<EmulatorRetroAchievementsViewModel>().value

                LaunchedEffect(Unit) {
                    viewModel.achievementsEvent.filterIsInstance<RAEventUi.Reset>().collect {
                        achievementsViewModel.onSessionReset()
                    }
                }

                AchievementUpdatesUi(viewModel)

                if (showAchievementList.value) {
                    AchievementListDialog(
                        viewModel = achievementsViewModel,
                        onDismiss = {
                            activeOverlays.removeActiveOverlay(EmulatorOverlay.ACHIEVEMENTS_DIALOG)
                            viewModel.resumeEmulator()
                            showAchievementList.value = false
                        }
                    )
                }

                if (showPendingSubmissionsDialog.value) {
                    PendingSubmissionsDialog(
                        pendingSubmissionsSummaryFlow = viewModel.pendingSubmissionsSummary,
                        onExit = { viewModel.exitEmulator(force = true) },
                        onCancel = {
                            activeOverlays.removeActiveOverlay(EmulatorOverlay.PENDING_SUBMISSION_CONFIRM_EXIT)
                            viewModel.resumeEmulator()
                            showPendingSubmissionsDialog.value = false
                        }
                    )
                }
            }
        }

        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                permissionHandler.observePermissionRequests().collect {
                    permissionRequestLauncher.launch(arrayOf(it))
                }
            }
        }

        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.runtimeLayout.collectLatest {
                    setupSoftInput(it)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                combine(viewModel.controllerConfiguration, viewModel.isAnalogueMovementEnabled, ::Pair).collect { (configuration, analogueMovement) ->
                    setupInputHandling(configuration, analogueMovement)
                    connectedControllerManager.setCurrentControllerConfiguration(configuration)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                connectedControllerManager.controllersState.collect {
                    binding.viewLayoutControls.setConnectedControllersState(it)
                    presentation?.layoutView?.setConnectedControllersState(it)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.mainScreenBackground.collectLatest {
                    mainScreenRenderer.setBackground(it)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.secondaryScreenBackground.collectLatest {
                    presentation?.updateBackground(it)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.runtimeRendererConfiguration.collectLatest {
                    mainScreenRenderer.updateRendererConfiguration(it)
                    presentation?.updateRendererConfiguration(it)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.currentFps.collectLatest {
                    if (it == null) {
                        binding.textFps.text = null
                    } else {
                        binding.textFps.text = getString(R.string.info_fps, it)
                    }
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.toastEvent.collectLatest {
                    val (message, duration) = when (it) {
                        ToastEvent.GbaLoadFailed -> R.string.error_load_gba_rom to Toast.LENGTH_SHORT
                        ToastEvent.QuickSaveSuccessful -> R.string.saved to Toast.LENGTH_SHORT
                        ToastEvent.QuickLoadSuccessful -> R.string.loaded to Toast.LENGTH_SHORT
                        ToastEvent.RewindNotEnabled -> R.string.rewind_not_enabled to Toast.LENGTH_SHORT
                        ToastEvent.RewindNotAvailableWhileRAHardcoreModeEnabled -> R.string.rewind_unavailable_ra_hardcore_enabled to Toast.LENGTH_LONG
                        ToastEvent.StateLoadFailed -> R.string.failed_load_state to Toast.LENGTH_SHORT
                        ToastEvent.StateSaveFailed -> R.string.failed_save_state to Toast.LENGTH_SHORT
                        ToastEvent.StateStateDoesNotExist -> R.string.cant_load_empty_slot to Toast.LENGTH_SHORT
                        ToastEvent.CannotUseSaveStatesWhenRAHardcoreIsEnabled -> R.string.save_states_unavailable_ra_hardcore_enabled to Toast.LENGTH_LONG
                        ToastEvent.CannotLoadStateWhenRunningFirmware,
                        ToastEvent.CannotSaveStateWhenRunningFirmware -> R.string.save_states_not_supported to Toast.LENGTH_LONG
                        ToastEvent.CannotSwitchRetroAchievementsMode -> R.string.retro_achievements_relaunch_to_apply_settings to Toast.LENGTH_LONG
                        ToastEvent.GbaModeNotSupported -> R.string.emulator_stop_gba_mode_unsupported to Toast.LENGTH_SHORT
                        ToastEvent.InternalError -> R.string.emulator_stop_internal_error to Toast.LENGTH_LONG
                    }

                    Toast.makeText(this@EmulatorActivity, message, duration).show()
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.CREATED) {
                viewModel.uiEvent.collectLatest {
                    when (it) {
                        EmulatorUiEvent.CloseEmulator -> {
                            choreographerFrameRenderer.stopRendering()
                            presentation?.dismiss()
                            finish()
                        }
                        is EmulatorUiEvent.OpenScreen.CheatsScreen -> {
                            val intent = Intent(this@EmulatorActivity, CheatsActivity::class.java)
                            intent.putExtra(CheatsActivity.KEY_ROM_INFO, RomInfoParcelable.fromRomInfo(it.romInfo))
                            cheatsLauncher.launch(intent)
                        }
                        EmulatorUiEvent.OpenScreen.SettingsScreen -> {
                            val settingsIntent = Intent(this@EmulatorActivity, SettingsActivity::class.java)
                            settingsLauncher.launch(settingsIntent)
                        }
                        is EmulatorUiEvent.ShowPauseMenu -> showPauseMenu(it.pauseMenu)
                        is EmulatorUiEvent.ShowRewindWindow -> showRewindWindow(it.rewindWindow)
                        is EmulatorUiEvent.ShowRomSaveStates -> {
                            showSaveStateSlotsDialog(it.saveStates, saving = it.reason == EmulatorUiEvent.ShowRomSaveStates.Reason.SAVING) { slot ->
                                if (it.reason == EmulatorUiEvent.ShowRomSaveStates.Reason.SAVING) {
                                    viewModel.saveStateToSlot(slot)
                                } else {
                                    viewModel.loadStateFromSlot(slot)
                                }
                            }
                        }
                        EmulatorUiEvent.ShowAchievementList -> {
                            activeOverlays.addActiveOverlay(EmulatorOverlay.ACHIEVEMENTS_DIALOG)
                            showAchievementList.value = true
                        }
                        EmulatorUiEvent.ShowPendingSubmissionsDialog -> {
                            activeOverlays.addActiveOverlay(EmulatorOverlay.PENDING_SUBMISSION_CONFIRM_EXIT)
                            showPendingSubmissionsDialog.value = true
                        }
                    }
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.rumbleEvent.collect {
                    when (it) {
                        is RumbleEvent.RumbleStart -> emulatorRumbleManager.startRumbling()
                        RumbleEvent.RumbleStop -> emulatorRumbleManager.stopRumbling()
                    }
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.CREATED) {
                viewModel.emulatorState.collectLatest {
                    when (it) {
                        is EmulatorState.Uninitialized -> {
                            binding.viewLayoutControls.isInvisible = true
                            binding.textFps.isGone = true
                            binding.textLoading.isGone = true
                        }
                        is EmulatorState.ValidatingFirmware -> {
                            showLoadingState()
                            emulatorLaunchValidatorDelegate.validateFirmware(it.consoleType)
                        }
                        is EmulatorState.ValidatingRom -> {
                            showLoadingState()
                            emulatorLaunchValidatorDelegate.validateRom(it.rom)
                        }
                        EmulatorState.LoadingFirmware,
                        EmulatorState.LoadingRom -> showLoadingState()
                        is EmulatorState.RunningRom,
                        is EmulatorState.RunningFirmware -> {
                            updateInGameMenuHeader(it)
                            setupSustainedPerformanceMode()
                            setupDisplayRefreshRate()
                            setupFpsCounter()
                            binding.textLoading.isGone = true
                            binding.viewLayoutControls.isVisible = true
                            backPressedCallback.isEnabled = true
                        }
                        is EmulatorState.RomLoadError -> {
                            binding.viewLayoutControls.isInvisible = true
                            binding.textFps.isGone = true
                            binding.textLoading.isGone = true
                            showRomLoadErrorDialog()
                        }
                        is EmulatorState.FirmwareLoadError -> {
                            binding.viewLayoutControls.isInvisible = true
                            binding.textFps.isGone = true
                            binding.textLoading.isGone = true
                            showFirmwareLoadErrorDialog(it)
                        }
                        is EmulatorState.RomNotFoundError -> {
                            binding.viewLayoutControls.isInvisible = true
                            binding.textFps.isGone = true
                            binding.textLoading.isGone = true
                            showRomNotFoundDialog(it.romPath)
                        }
                    }
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                WindowInfoTracker.getOrCreate(this@EmulatorActivity).windowLayoutInfo(this@EmulatorActivity).collect {
                    val folds = it.displayFeatures.mapNotNull {
                        if (it is FoldingFeature) {
                            ScreenFold(
                                orientation = if (it.orientation == FoldingFeature.Orientation.HORIZONTAL) Orientation.LANDSCAPE else Orientation.PORTRAIT,
                                type = if (it.isSeparating) ScreenFold.FoldType.SEAMLESS else ScreenFold.FoldType.GAP,
                                foldBounds = Rect(it.bounds.left, it.bounds.top, it.bounds.width(), it.bounds.height())
                            )
                        } else {
                            null
                        }
                    }
                    viewModel.setScreenFolds(folds)
                }
            }
        }
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.CREATED) {
                appForegroundStateObserver.onAppMovedToBackgroundEvent.collect {
                    presentation?.dismiss()
                    presentation = null
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        updateDisplays()
        getSystemService<DisplayManager>()?.registerDisplayListener(displayListener, null)
        getSystemService<InputManager>()?.registerInputDeviceListener(connectedControllerManager, null)
        connectedControllerManager.startTrackingControllers()
        frameRenderCoordinator.addSurface(binding.surfaceMain)
    }

    private fun updateDisplays() {
        val currentDisplay = ContextCompat.getDisplayOrDefault(this)
        val secondaryDisplay = secondaryDisplaySelector.getSecondaryDisplay(this)

        val displays = deviceLayoutDisplayMapper.mapDisplaysToLayoutDisplays(currentDisplay, secondaryDisplay)
        viewModel.setConnectedDisplays(displays)

        showExternalDisplay(secondaryDisplay)
    }

    private fun showExternalDisplay(secondaryDisplay: Display?) {
        if (presentation?.display?.displayId == secondaryDisplay?.displayId) {
            return
        }

        presentation?.dismiss()
        presentation = null

        if (secondaryDisplay != null) {
            presentation = ExternalPresentation(
                context = this,
                display = secondaryDisplay,
                frameRenderCoordinator = frameRenderCoordinator,
            ).apply {
                layoutView.apply {
                    setLayoutComponentViewBuilderFactory(RuntimeLayoutComponentViewBuilderFactory())
                    setFrontendInputHandler(frontendInputHandler)
                    setSystemInputHandler(melonTouchHandler)
                    viewModel.runtimeLayout.value?.let {
                        updateLayout(it)
                    }

                    setLayoutComponentToggleState(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE, frontendInputHandler.fastForwardEnabled)
                    setLayoutComponentToggleState(LayoutComponent.BUTTON_MICROPHONE_TOGGLE, frontendInputHandler.microphoneEnabled)
                    setConnectedControllersState(connectedControllerManager.controllersState.value)
                }

                updateRendererConfiguration(viewModel.runtimeRendererConfiguration.value)
                updateBackground(viewModel.secondaryScreenBackground.value)
                if (binding.viewLayoutControls.areScreensSwapped()) {
                    swapScreens()
                }
                if (activeOverlays.hasActiveOverlays()) {
                    setPauseOverlayVisibility(true)
                }

                show()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)

        val launchArgs = LaunchArgs.fromIntent(intent)
        // Invalid arguments. Ignore completely
        if (launchArgs == null)
            return

        if (viewModel.emulatorState.value.isRunning()) {
            viewModel.pauseEmulator(false)

            activeOverlays.addActiveOverlay(EmulatorOverlay.SWITCH_NEW_ROM_DIALOG)
            AlertDialog.Builder(this)
                    .setTitle(getString(R.string.title_emulator_running))
                    .setMessage(getString(R.string.message_stop_emulation))
                    .setPositiveButton(R.string.ok) { _, _ ->
                        setIntent(intent)
                        viewModel.relaunchWithNewArgs(launchArgs)
                    }
                    .setNegativeButton(R.string.no) { dialog, _ ->
                        dialog.cancel()
                    }
                    .setOnDismissListener {
                        activeOverlays.removeActiveOverlay(EmulatorOverlay.SWITCH_NEW_ROM_DIALOG)
                    }
                    .setOnCancelListener {
                        viewModel.resumeEmulator()
                    }
                    .show()
        }
    }

    override fun onResume() {
        super.onResume()
        choreographerFrameRenderer.startRendering()

        if (!activeOverlays.hasActiveOverlays()) {
            disableScreenTimeOut()
            viewModel.resumeEmulator()
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        setupFullscreen()
    }

    private fun setupFullscreen() {
        window.insetsControllerCompat?.let {
            it.hide(WindowInsetsCompat.Type.navigationBars())
            it.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun setupSustainedPerformanceMode() {
        window.setSustainedPerformanceMode(viewModel.isSustainedPerformanceModeEnabled())
    }

    /**
     * Frame generation (Pomegrade) makes two images per DS frame: the display
     * is asked for its mode closest to 120 Hz (same resolution) so each one gets
     * a refresh. Otherwise the system chooses, as before.
     */
    private fun setupDisplayRefreshRate() {
        // Activity.getDisplay() is API 30; the minimum is 29
        val currentDisplay = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            display ?: return
        } else {
            @Suppress("DEPRECATION")
            windowManager.defaultDisplay
        }
        val current = currentDisplay.mode
        val modeId = if (viewModel.isFrameGenerationEnabled()) {
            currentDisplay.supportedModes
                .filter { it.physicalWidth == current.physicalWidth && it.physicalHeight == current.physicalHeight && it.refreshRate >= 119f }
                .minByOrNull { abs(it.refreshRate - 120f) }
                ?.modeId ?: 0
        } else {
            0
        }
        window.attributes = window.attributes.also { it.preferredDisplayModeId = modeId }
    }

    private fun setupFpsCounter() {
        val fpsCounterPosition = viewModel.getFpsCounterPosition()
        if (fpsCounterPosition == FpsCounterPosition.HIDDEN) {
            binding.textFps.isGone = true
        } else {
            binding.textFps.isVisible = true
            val newParams = binding.textFps.layoutParams as ConstraintLayout.LayoutParams
            when (fpsCounterPosition) {
                FpsCounterPosition.TOP_LEFT -> {
                    newParams.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.leftToLeft = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.TOP_CENTER -> {
                    newParams.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.leftToLeft = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.rightToRight = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.TOP_RIGHT -> {
                    newParams.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.rightToRight = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.BOTTOM_LEFT -> {
                    newParams.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.leftToLeft = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.BOTTOM_CENTER -> {
                    newParams.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.leftToLeft = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.rightToRight = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.BOTTOM_RIGHT -> {
                    newParams.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID
                    newParams.rightToRight = ConstraintLayout.LayoutParams.PARENT_ID
                }
                FpsCounterPosition.HIDDEN -> { /* Do nothing here */ }
            }
            binding.textFps.layoutParams = newParams
        }
    }

    private fun setupSoftInput(layoutConfiguration: RuntimeInputLayoutConfiguration?) {
        if (layoutConfiguration != null) {
            setLayoutOrientation(layoutConfiguration.layoutOrientation)
            with(binding.viewLayoutControls) {
                instantiateLayout(layoutConfiguration, LayoutTarget.MAIN_SCREEN)
                setLayoutComponentToggleState(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE, frontendInputHandler.fastForwardEnabled)
                setLayoutComponentToggleState(LayoutComponent.BUTTON_MICROPHONE_TOGGLE, frontendInputHandler.microphoneEnabled)
            }
            handler.post {
                updateRendererScreenAreas()
            }

            presentation?.apply {
                updateLayout(layoutConfiguration)
                layoutView.setLayoutComponentToggleState(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE, frontendInputHandler.fastForwardEnabled)
                layoutView.setLayoutComponentToggleState(LayoutComponent.BUTTON_MICROPHONE_TOGGLE, frontendInputHandler.microphoneEnabled)
            }
        } else {
            binding.viewLayoutControls.destroyLayout()
            presentation?.layoutView?.destroyLayout()
        }
    }

    private fun swapScreen() {
        binding.viewLayoutControls.swapScreens()
        presentation?.swapScreens()

        updateRendererScreenAreas()
    }

    private fun updateRendererScreenAreas() {
        val (topScreen, bottomScreen) = if (binding.viewLayoutControls.areScreensSwapped()) {
            LayoutComponent.BOTTOM_SCREEN to LayoutComponent.TOP_SCREEN
        } else {
            LayoutComponent.TOP_SCREEN to LayoutComponent.BOTTOM_SCREEN
        }
        val topView = binding.viewLayoutControls.getLayoutComponentView(topScreen)
        val bottomView = binding.viewLayoutControls.getLayoutComponentView(bottomScreen)
        mainScreenRenderer.updateScreenAreas(
            topView?.getRect(),
            bottomView?.getRect(),
            topView?.baseAlpha ?: 1f,
            bottomView?.baseAlpha ?: 1f,
            topView?.onTop ?: false,
            bottomView?.onTop ?: false,
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val touchScreenArea = bottomView?.getRect()?.let {
                val rect = android.graphics.Rect(it.x, it.y, it.right, it.bottom)
                listOf(rect)
            }
            window?.systemGestureExclusionRects = touchScreenArea.orEmpty()
        }
    }

    private fun setupInputHandling(controllerConfiguration: ControllerConfiguration, analogueMovement: Boolean) {
        nativeInputListener = InputProcessor(controllerConfiguration, melonTouchHandler, frontendInputHandler, analogueMovement)
    }

    private fun handleBackPressed() {
        if (binding.drawerLayout.isOpen) {
            binding.drawerLayout.close()
        } else if (isRewindWindowOpen()) {
            closeRewindWindow()
        } else {
            viewModel.pauseEmulator(true)
        }
    }

    // Pomegrade: the in-game menu shared with the 3DS screen (module :emulator-ui), a drawer opened
    // by pausing. Only opened from code: an edge swipe would fight with the touch screen
    private fun setupInGameMenu() {
        binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)
        binding.drawerLayout.addDrawerListener(object : DrawerLayout.SimpleDrawerListener() {
            override fun onDrawerClosed(drawerView: View) {
                binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)
                activeOverlays.removeActiveOverlay(EmulatorOverlay.PAUSE_MENU)
                if (!pauseMenuOptionPicked) {
                    viewModel.resumeEmulator()
                }
            }
        })
        binding.inGameMenu.setNavigationItemSelectedListener { item ->
            pauseMenuOptionPicked = true
            binding.drawerLayout.close()
            if (item.itemId == EmulatorMenuR.id.emulator_menu_swap_screens) {
                swapScreen()
                viewModel.resumeEmulator()
            } else {
                pauseMenuOptions.firstOrNull { inGameMenuItemId(it) == item.itemId }?.let {
                    viewModel.onPauseMenuOptionSelected(it)
                }
            }
            true
        }
    }

    private fun updateInGameMenuHeader(state: EmulatorState) {
        val (title, consoleType) = when (state) {
            is EmulatorState.RunningRom -> state.rom.name to ConsoleType.DS
            is EmulatorState.RunningFirmware -> getString(R.string.action_boot_firmware) to state.console
            else -> return
        }
        val console = getString(
            if (consoleType == ConsoleType.DSi) EmulatorMenuR.string.emulator_menu_console_dsi else EmulatorMenuR.string.emulator_menu_console_ds
        )
        val iconView = EmulatorMenu.setHeader(binding.inGameMenu, title, console) ?: return
        iconView.isVisible = false
        if (state is EmulatorState.RunningRom) {
            lifecycleScope.launch {
                romIconProvider.getRomIcon(state.rom)?.let {
                    iconView.setImageBitmap(it)
                    iconView.isVisible = true
                }
            }
        }
    }

    private fun inGameMenuItemId(option: PauseMenuOption): Int? {
        return when (option) {
            RomPauseMenuOption.SETTINGS, FirmwarePauseMenuOption.SETTINGS -> EmulatorMenuR.id.emulator_menu_settings
            RomPauseMenuOption.SAVE_STATE -> EmulatorMenuR.id.emulator_menu_save_state
            RomPauseMenuOption.LOAD_STATE -> EmulatorMenuR.id.emulator_menu_load_state
            RomPauseMenuOption.REWIND -> EmulatorMenuR.id.emulator_menu_rewind
            RomPauseMenuOption.CHEATS -> EmulatorMenuR.id.emulator_menu_cheats
            RomPauseMenuOption.VIEW_ACHIEVEMENTS -> EmulatorMenuR.id.emulator_menu_achievements
            RomPauseMenuOption.RESET, FirmwarePauseMenuOption.RESET -> EmulatorMenuR.id.emulator_menu_reset
            RomPauseMenuOption.EXIT, FirmwarePauseMenuOption.EXIT -> EmulatorMenuR.id.emulator_menu_exit
            else -> null
        }
    }

    private fun showPauseMenu(pauseMenu: PauseMenu) {
        pauseMenuOptions = pauseMenu.options
        pauseMenuOptionPicked = false
        val shownItems = pauseMenu.options.mapNotNull { inGameMenuItemId(it) }.toSet() + EmulatorMenuR.id.emulator_menu_swap_screens
        EmulatorMenu.showOnly(binding.inGameMenu.menu, shownItems)

        activeOverlays.addActiveOverlay(EmulatorOverlay.PAUSE_MENU)
        binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_UNLOCKED)
        binding.drawerLayout.open()
    }

    private fun disableScreenTimeOut() {
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private fun enableScreenTimeOut() {
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (!activeOverlays.hasActiveOverlays() && nativeInputListener.onKeyEvent(event))
            return true

        return super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (!activeOverlays.hasActiveOverlays() && nativeInputListener.onMotionEvent(event))
            return true

        return super.dispatchGenericMotionEvent(event)
    }

    private fun isRewindWindowOpen(): Boolean {
        return binding.layoutRoot.currentState == R.id.rewind_visible
    }

    // Pomegrade: the save state dialog shared with the 3DS screen (module :emulator-ui)
    private fun showSaveStateSlotsDialog(slots: List<SaveStateSlot>, saving: Boolean, onSlotPicked: (SaveStateSlot) -> Unit) {
        fun toUi(slots: List<SaveStateSlot>) = slots.map {
            SaveStateSlotUi(
                slot = it.slot,
                isQuickSlot = it.slot == SaveStateSlot.QUICK_SAVE_SLOT,
                savedAt = it.lastUsedDate.takeIf { _ -> it.exists },
                screenshot = it.screenshot,
            )
        }
        var currentSlots = slots

        activeOverlays.addActiveOverlay(EmulatorOverlay.SAVE_STATES_DIALOG)
        SaveStatesDialog.show(
            context = this,
            saving = saving,
            slots = toUi(slots),
            onSlotPicked = { picked ->
                currentSlots.firstOrNull { it.slot == picked.slot }?.let(onSlotPicked)
            },
            onSlotDeleted = { deleted ->
                currentSlots.firstOrNull { it.slot == deleted.slot }?.let { slot ->
                    viewModel.deleteSaveStateSlot(slot)?.let { newSlots ->
                        currentSlots = newSlots
                        toUi(newSlots)
                    }
                }
            },
            onCancel = { viewModel.resumeEmulator() },
            onDismiss = { activeOverlays.removeActiveOverlay(EmulatorOverlay.SAVE_STATES_DIALOG) },
        )
    }

    private fun showRomLoadErrorDialog() {
        activeOverlays.addActiveOverlay(EmulatorOverlay.ROM_LOAD_ERROR_DIALOG)
        AlertDialog.Builder(this)
            .setCancelable(false)
            .setTitle(R.string.error_load_rom)
            .setMessage(R.string.error_load_rom_message)
            .setPositiveButton(R.string.ok) { dialog, _ ->
                dialog.dismiss()
                finish()
            }
            .show()
    }

    private fun showRomNotFoundDialog(romPath: String) {
        activeOverlays.addActiveOverlay(EmulatorOverlay.ROM_NOT_FOUND_DIALOG)
        AlertDialog.Builder(this)
            .setTitle(R.string.error_rom_not_found)
            .setMessage(getString(R.string.error_rom_not_found_info, romPath))
            .setPositiveButton(R.string.ok) { _, _ ->
                finish()
            }
            .setOnDismissListener {
                finish()
            }
            .show()
    }

    private fun showFirmwareLoadErrorDialog(error: EmulatorState.FirmwareLoadError) {
        activeOverlays.addActiveOverlay(EmulatorOverlay.FIRMWARE_LOAD_ERROR_DIALOG)
        AlertDialog.Builder(this)
            .setCancelable(false)
            .setTitle(R.string.error_load_firmware)
            .setMessage(resources.getString(R.string.error_load_firmware_message, error.reason.toString()))
            .setPositiveButton(R.string.ok) { dialog, _ ->
                dialog.dismiss()
                finish()
            }
            .show()
    }

    private fun showRewindWindow(rewindWindow: RewindWindow) {
        activeOverlays.addActiveOverlay(EmulatorOverlay.REWIND_WINDOW)
        binding.layoutRoot.transitionToState(R.id.rewind_visible)
        rewindSaveStateAdapter.setRewindWindow(rewindWindow)
    }

    private fun closeRewindWindow() {
        activeOverlays.removeActiveOverlay(EmulatorOverlay.REWIND_WINDOW)
        binding.layoutRoot.transitionToState(R.id.rewind_hidden)
        viewModel.resumeEmulator()
    }

    private fun showLoadingState() {
        binding.viewLayoutControls.isInvisible = true
        binding.textFps.isGone = true
        binding.textLoading.isVisible = true
    }

    private fun updateOrientation(configuration: Configuration) {
        val orientation = if (configuration.orientation == Configuration.ORIENTATION_PORTRAIT) {
            Orientation.PORTRAIT
        } else {
            Orientation.LANDSCAPE
        }
        viewModel.setSystemOrientation(orientation)
    }

    override fun onPause() {
        super.onPause()
        enableScreenTimeOut()
        choreographerFrameRenderer.stopRendering()
        viewModel.pauseEmulator(false)
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        updateOrientation(newConfig)
        // There is an issue in which, after moving the app to a different display, the app reports that it is still running on the previous display. Adding a frame of delay
        // seems to fix the problem.
        handler.post {
            updateDisplays()
        }
    }

    override fun onStop() {
        super.onStop()
        getSystemService<DisplayManager>()?.unregisterDisplayListener(displayListener)
        getSystemService<InputManager>()?.unregisterInputDeviceListener(connectedControllerManager)
        connectedControllerManager.stopTrackingControllers()
        frameRenderCoordinator.removeSurface(binding.surfaceMain)
    }

    override fun onDestroy() {
        super.onDestroy()
        frameRenderCoordinator.stop()
        presentation?.dismiss()
    }
}