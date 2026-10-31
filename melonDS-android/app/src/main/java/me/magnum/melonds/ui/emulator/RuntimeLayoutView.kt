package me.magnum.melonds.ui.emulator

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.ImageView
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.core.content.ContextCompat
import dagger.hilt.android.AndroidEntryPoint
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import me.magnum.melonds.R
import me.magnum.melonds.common.vibration.TouchVibrator
import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.input.SoftInputBehaviour
import me.magnum.melonds.domain.model.layout.LayoutComponent
import me.magnum.melonds.ui.common.LayoutView
import me.magnum.melonds.ui.emulator.input.ButtonsInputHandler
import me.magnum.melonds.ui.emulator.input.ClassicButtonDrawable
import me.magnum.melonds.ui.emulator.input.ClassicPressListener
import me.magnum.melonds.ui.emulator.input.FeedbackInputHandler
import me.magnum.melonds.ui.emulator.input.DpadInputHandler
import me.magnum.melonds.ui.emulator.input.FrontendInputHandler
import me.magnum.melonds.ui.emulator.input.IInputListener
import me.magnum.melonds.ui.emulator.input.JoystickDrawable
import me.magnum.melonds.ui.emulator.input.JoystickInputHandler
import me.magnum.melonds.ui.emulator.input.SingleButtonInputHandler
import me.magnum.melonds.ui.emulator.input.TouchscreenInputHandler
import me.magnum.melonds.ui.emulator.input.view.ToggleableImageView
import me.magnum.melonds.ui.emulator.model.ConnectedControllersState
import me.magnum.melonds.ui.emulator.model.RuntimeInputLayoutConfiguration
import me.magnum.melonds.ui.layouteditor.model.LayoutTarget
import javax.inject.Inject

@AndroidEntryPoint
class RuntimeLayoutView(context: Context, attrs: AttributeSet? = null) : LayoutView(context, attrs) {

    init {
        // Pomegrade: the Classic layout's key caps draw their thickness a little past their bounds
        clipChildren = false
    }

    @Inject
    lateinit var touchVibrator: TouchVibrator

    private var currentRuntimeLayout: RuntimeInputLayoutConfiguration? = null
    private var frontendInputHandler: IInputListener? = null
    private var systemInputHandler: IInputListener? = null
    private var isSoftInputVisible = true
    private var areScreensSwapped = false
    private var connectedControllersState: ConnectedControllersState = ConnectedControllersState.NoControllers

    /** Pomegrade: told the layout's Punica button each time the components' visibility changes, null when none is shown. */
    var onPunicaMenuButtonChanged: ((View?) -> Unit)? = null

    fun setFrontendInputHandler(frontendInputHandler: FrontendInputHandler) {
        this.frontendInputHandler = frontendInputHandler
        updateInputs()
    }

    fun setSystemInputHandler(systemInputHandler: IInputListener) {
        this.systemInputHandler = systemInputHandler
        updateInputs()
    }

    fun setConnectedControllersState(state: ConnectedControllersState) {
        connectedControllersState = state
        updateVisibility()
    }

    fun toggleSoftInputVisibility() {
        isSoftInputVisible = !isSoftInputVisible
        setLayoutComponentToggleState(LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT, isSoftInputVisible)
        updateVisibility()
    }

    fun swapScreens() {
        areScreensSwapped = !areScreensSwapped
        updateScreenInputs()
    }

    fun areScreensSwapped(): Boolean {
        return areScreensSwapped
    }

    fun setLayoutComponentToggleState(layoutComponent: LayoutComponent, isEnabled: Boolean) {
        val toggleableImageView = getLayoutComponentView(layoutComponent)?.view as? ToggleableImageView ?: return
        toggleableImageView.setToggleState(isEnabled)
    }

    fun instantiateLayout(runtimeLayout: RuntimeInputLayoutConfiguration, layoutTarget: LayoutTarget) {
        currentRuntimeLayout = runtimeLayout
        instantiateLayout(runtimeLayout.layout, layoutTarget)
        updateInputs()
        updateVisibility()
        setLayoutComponentToggleState(LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT, isSoftInputVisible)
    }

    private fun updateInputs() {
        val currentRuntimeLayout = currentRuntimeLayout
        if (currentRuntimeLayout == null) {
            isGone = true
            return
        }

        isVisible = true
        val inputAlpha = currentRuntimeLayout.softInputOpacity / 100f

        val enableHapticFeedback = currentRuntimeLayout.isHapticFeedbackEnabled
        // Pomegrade: the Classic layout's buttons drawn as its template (sunk while pressed, a hard click as haptic
        // feedback), else upstream's images
        val classicDrawables = HashMap<LayoutComponent, ClassicButtonDrawable>()
        listOf(
            LayoutComponent.BUTTONS to (ClassicButtonDrawable.Kind.BUTTONS to R.drawable.buttons),
            LayoutComponent.BUTTON_L to (ClassicButtonDrawable.Kind.L to R.drawable.button_l),
            LayoutComponent.BUTTON_R to (ClassicButtonDrawable.Kind.R to R.drawable.button_r),
            LayoutComponent.BUTTON_START to (ClassicButtonDrawable.Kind.START to R.drawable.button_start),
            LayoutComponent.BUTTON_SELECT to (ClassicButtonDrawable.Kind.SELECT to R.drawable.button_select),
            LayoutComponent.BUTTON_PUNICA_MENU to (ClassicButtonDrawable.Kind.PUNICA to EmulatorMenuR.drawable.emulator_menu_ic_punica),
            LayoutComponent.DPAD to (ClassicButtonDrawable.Kind.DPAD to R.drawable.keypad),
        ).forEach { (component, images) ->
            // the joystick takes the D-pad's place when analogue movement is on (below)
            if (component == LayoutComponent.DPAD && currentRuntimeLayout.isAnalogueMovementEnabled) return@forEach
            val image = getLayoutComponentView(component)?.view as? ImageView ?: return@forEach
            val classic = classicImage(currentRuntimeLayout, images.first)
            if (classic != null) classicDrawables[component] = classic
            image.setImageDrawable(classic ?: getDrawableCompat(images.second))
        }
        val isClassic = currentRuntimeLayout.classicButtonStyle != null
        fun <T : FeedbackInputHandler> T.classic(): T = apply { clickHaptic = isClassic }
        fun pressing(listener: IInputListener) = if (isClassic) ClassicPressListener(listener, classicDrawables) else listener

        systemInputHandler?.let { system ->
            val listener = pressing(system)
            getLayoutComponentView(LayoutComponent.DPAD)?.view?.let { dpad ->
                if (currentRuntimeLayout.isAnalogueMovementEnabled) {
                    // Pomegrade: the D-pad's place holds a joystick
                    val joystick = JoystickDrawable()
                    (dpad as? ImageView)?.setImageDrawable(joystick)
                    dpad.setOnTouchListener(JoystickInputHandler(listener, joystick, enableHapticFeedback, touchVibrator))
                } else {
                    dpad.setOnTouchListener(DpadInputHandler(listener, enableHapticFeedback, touchVibrator).classic())
                }
            }
            getLayoutComponentView(LayoutComponent.BUTTONS)?.view?.setOnTouchListener(ButtonsInputHandler(listener, enableHapticFeedback, touchVibrator).classic())
            getLayoutComponentView(LayoutComponent.BUTTON_L)?.view?.setOnTouchListener(SingleButtonInputHandler(listener, Input.L, enableHapticFeedback, touchVibrator).classic())
            getLayoutComponentView(LayoutComponent.BUTTON_R)?.view?.setOnTouchListener(SingleButtonInputHandler(listener, Input.R, enableHapticFeedback, touchVibrator).classic())
            getLayoutComponentView(LayoutComponent.BUTTON_SELECT)?.view?.setOnTouchListener(SingleButtonInputHandler(listener, Input.SELECT, enableHapticFeedback, touchVibrator).classic())
            getLayoutComponentView(LayoutComponent.BUTTON_START)?.view?.setOnTouchListener(SingleButtonInputHandler(listener, Input.START, enableHapticFeedback, touchVibrator).classic())
            getLayoutComponentView(LayoutComponent.BUTTON_HINGE)?.view?.setOnTouchListener(SingleButtonInputHandler(listener, Input.HINGE, enableHapticFeedback, touchVibrator))
        }
        frontendInputHandler?.let {
            getLayoutComponentView(LayoutComponent.BUTTON_RESET)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.RESET, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_PAUSE)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.PAUSE, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.FAST_FORWARD, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_MICROPHONE_TOGGLE)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.MICROPHONE, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.TOGGLE_SOFT_INPUT, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_SWAP_SCREENS)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.SWAP_SCREENS, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_QUICK_SAVE)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.QUICK_SAVE, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_QUICK_LOAD)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.QUICK_LOAD, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_REWIND)?.view?.setOnTouchListener(SingleButtonInputHandler(it, Input.REWIND, enableHapticFeedback, touchVibrator))
            getLayoutComponentView(LayoutComponent.BUTTON_PUNICA_MENU)?.view?.setOnTouchListener(SingleButtonInputHandler(pressing(it), Input.PUNICA_MENU, enableHapticFeedback, touchVibrator).classic())
        }

        getLayoutComponentViews().forEach {
            if (!it.component.isScreen()) {
                val classic = classicDrawables[it.component]
                if (classic != null) {
                    // Pomegrade: the opacity on the drawn cap, not the view, whose layer would clip the cap's sides
                    it.view.alpha = 1f
                    classic.alpha = (inputAlpha * 255).toInt()
                } else {
                    it.view.apply {
                        alpha = inputAlpha
                    }
                }
            }
        }

        updateScreenInputs()
    }

    private fun updateScreenInputs() {
        val (touchScreenComponent, nonTouchScreenComponent) = if (areScreensSwapped) {
            LayoutComponent.TOP_SCREEN to LayoutComponent.BOTTOM_SCREEN
        } else {
            LayoutComponent.BOTTOM_SCREEN to LayoutComponent.TOP_SCREEN
        }
        systemInputHandler?.let {
            getLayoutComponentView(touchScreenComponent)?.view?.setOnTouchListener(TouchscreenInputHandler(it))
        }
        getLayoutComponentView(nonTouchScreenComponent)?.view?.setOnTouchListener(null)
    }

    private fun updateVisibility() {
        val currentConnectedControllersState = connectedControllersState
        var hiddenComponents = when(currentRuntimeLayout?.softInputBehaviour) {
            SoftInputBehaviour.ALWAYS_VISIBLE -> emptyList()
            SoftInputBehaviour.HIDE_SYSTEM_BUTTONS_WHEN_CONTROLLERS_CONNECTED, null -> {
                when(currentConnectedControllersState) {
                    ConnectedControllersState.NoControllers -> emptyList()
                    is ConnectedControllersState.ControllersConnected -> listOf(
                        LayoutComponent.BUTTONS,
                        LayoutComponent.DPAD,
                        LayoutComponent.BUTTON_L,
                        LayoutComponent.BUTTON_R,
                        LayoutComponent.BUTTON_START,
                        LayoutComponent.BUTTON_SELECT
                    )
                }
            }
            SoftInputBehaviour.HIDE_ALL_BUTTONS_ASSIGNED_TO_CONNECTED_CONTROLLERS -> when(currentConnectedControllersState) {
                ConnectedControllersState.NoControllers -> emptyList()
                is ConnectedControllersState.ControllersConnected -> {
                    LayoutComponent.entries.filter {
                        // The component can be hidden if all matching inputs are assigned to connected controllers
                        it.matchingInputs.all {
                            currentConnectedControllersState.assignedInputs.contains(it)
                        }
                    }
                }
            }
            SoftInputBehaviour.ALWAYS_INVISIBLE -> LayoutComponent.entries.toList()
        }

        if (!isSoftInputVisible) {
            // Hide everything except the soft input toggle button if it was not already hidden by the soft input behaviour logic
            hiddenComponents = if (hiddenComponents.contains(LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT)) {
                LayoutComponent.entries.toList()
            } else {
                LayoutComponent.entries.toList().filter { it != LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT}
            }
        }

        val visibleComponents = LayoutComponent.entries.filter { !hiddenComponents.contains(it) }

        hiddenComponents.forEach { component ->
            if (!component.isScreen()) {
                getLayoutComponentView(component)?.view?.isVisible = false
            }
        }
        visibleComponents.forEach { component ->
            if (!component.isScreen()) {
                getLayoutComponentView(component)?.view?.isVisible = true
            }
        }
        onPunicaMenuButtonChanged?.invoke(punicaMenuButton())
    }

    /** Pomegrade: the layout's Punica button when it is shown, else null. */
    fun punicaMenuButton(): View? = getLayoutComponentView(LayoutComponent.BUTTON_PUNICA_MENU)?.view?.takeIf { it.isVisible }

    private fun classicImage(layout: RuntimeInputLayoutConfiguration, kind: ClassicButtonDrawable.Kind): ClassicButtonDrawable? =
        layout.classicButtonStyle?.let { ClassicButtonDrawable(context, kind, it) }

    private fun getDrawableCompat(resource: Int) = ContextCompat.getDrawable(context, resource)
}