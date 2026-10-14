package io.github.wwandene2.pomegrade.emulatorui

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.res.ColorStateList
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.KeyboardShortcutGroup
import android.view.Menu
import android.view.MenuInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.Window
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.PopupMenu
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

/**
 * The shortcut pill, the same over every console's game: a small pill in a corner of the screen (top right
 * by default; dragged, it moves to the nearest corner) that, touched, swings out the user's shortcuts around
 * it like the blades of a pocket knife. A shortcut is one of the shared in-game menu's entries
 * (`R.menu.emulator_menu`), so it does exactly what that entry does in the core's menu: [onSelect] gets the
 * entry's id. Two are always there: Settings (System, and Game when the core has per-game settings) and the
 * Punica logo, the menu's Exit, back to Punica's game list. The others are the user's choice (a long press, or
 * the Edit blade), among the entries the core shows ([available]); the choice is kept for every console.
 *
 * A screen's layout can show a button of its own for it (the DS Classic layout, between SELECT and START):
 * given by [useAnchor], the pill hides and the blades swing out upward from that button, which [toggle]s them.
 *
 * Whether the screen shows the pill, the drawer's edge swipe or both is the user's choice too
 * ([KEY_ACCESS], Punica's settings): with the pill alone, the core locks its drawer's edge swipe
 * ([edgeSwipeAllowed]), which also leaves the screen's edge to Android's Back gesture. The drawer stays
 * reachable from Back and from the controller, as each core opens it.
 *
 * At rest the pill is visible, faint or hidden ([KEY_OPACITY]; hidden, a touch on its corner still opens it),
 * and can hide itself after a few seconds without a touch on the screen ([KEY_AUTO_HIDE]); any touch shows it
 * again. A controller opens it with Select + Start pressed together, or its Home button; then the D-pad moves
 * between the blades (right / down to the next, left / up to the previous), A picks one and B closes (or goes
 * back from System / Game). It sees the keys before the core does by wrapping the window's callback, so no core
 * has to forward them; while it is open the core gets no key press from the controller, only the releases.
 */
@SuppressLint("ViewConstructor", "ClickableViewAccessibility")
class ShortcutPill private constructor(
    private val activity: Activity,
    private val available: () -> Set<Int>,
    private val onSelect: (Int) -> Unit,
) : FrameLayout(activity) {

    companion object {
        /** "pill" (default), "drawer" or "both"; in the app's default preferences, which the settings screen writes. */
        const val KEY_ACCESS = "punica_menu_access"
        private const val KEY_SHORTCUTS = "punica_pill_shortcuts"
        private const val KEY_CORNER = "punica_pill_corner"
        /** "visible", "faint" (default) or "hidden": the pill at rest. */
        const val KEY_OPACITY = "punica_pill_opacity"
        /** True: the pill hides after a few seconds without a touch on the screen. */
        const val KEY_AUTO_HIDE = "punica_pill_auto_hide"
        private const val MAX_SHORTCUTS = 6
        private val FIXED = listOf(R.id.emulator_menu_settings, R.id.emulator_menu_exit)

        private fun preferences(context: Context): SharedPreferences =
            context.getSharedPreferences(context.packageName + "_preferences", Context.MODE_PRIVATE)

        private fun access(context: Context) = preferences(context).getString(KEY_ACCESS, "pill") ?: "pill"

        /** False when the pill is the only way to the menu: the core then locks its drawer's edge swipe. */
        fun edgeSwipeAllowed(context: Context) = access(context) != "pill"

        /**
         * Puts the pill over [activity]'s screen, unless the user chose the drawer alone (then null).
         * [available]: the menu entries the core shows now; [onSelect]: runs one, as the core's menu does.
         */
        fun attach(activity: Activity, available: () -> Set<Int>, onSelect: (Int) -> Unit): ShortcutPill? {
            if (access(activity) == "drawer") return null
            val content = activity.findViewById<ViewGroup>(android.R.id.content) ?: return null
            return ShortcutPill(activity, available, onSelect).also {
                content.addView(it, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
            }
        }
    }

    private val themed = ContextThemeWrapper(activity, R.style.Theme_Pomegrade_EmulatorMenu)
    private val density = resources.displayMetrics.density
    private fun dp(value: Int) = (value * density).toInt()

    // the shared menu's entries, for the shortcuts' titles and icons
    private val entries: Menu = PopupMenu(activity, this).menu.also { MenuInflater(activity).inflate(R.menu.emulator_menu, it) }

    private val scrim = View(activity).apply {
        setBackgroundColor(0x59000000)
        alpha = 0f
        visibility = GONE
        setOnClickListener { close() }
    }
    private val pill = LinearLayout(activity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(dp(6), 0, dp(12), 0)
        minimumWidth = dp(48)
        background = GradientDrawable().apply { cornerRadius = dp(16).toFloat(); setColor(PILL_COLOR) }
        contentDescription = context.getString(R.string.emulator_pill)
        isFocusable = true
        addView(ImageView(activity).apply { setImageResource(R.drawable.emulator_menu_ic_punica) }, LinearLayout.LayoutParams(dp(24), dp(24)))
        val dots = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
        repeat(3) {
            dots.addView(View(activity).apply {
                background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(0xFF9EA3AC.toInt()) }
            }, LinearLayout.LayoutParams(dp(4), dp(4)).apply { marginStart = dp(3) })
        }
        addView(dots, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { marginStart = dp(4) })
    }
    private val blades = ArrayList<View>()
    private var bladesOpen = false
    // the Settings blade's System / Game choice is shown
    private var settingsShown = false
    private var corner = preferences(activity).getInt(KEY_CORNER, 0) // 0 top right, 1 top left, 2 bottom right, 3 bottom left
    private var insetLeft = 0
    private var insetTop = 0
    private var insetRight = 0
    private var insetBottom = 0
    private val pressHandler = Handler(Looper.getMainLooper())
    private val restAlpha = when (preferences(activity).getString(KEY_OPACITY, "faint")) {
        "visible" -> 1f
        "hidden" -> 0f
        else -> FAINT_ALPHA
    }
    private val autoHide = preferences(activity).getBoolean(KEY_AUTO_HIDE, false)
    private var autoHidden = false
    private val hideLater = Runnable { autoHidden = true; rest() }
    // the layout's Punica button when the screen shows one: the blades open from it, the pill is hidden
    private var anchor: View? = null
    // the controller's focused blade, -1 when none
    private var focused = -1
    private var selectDown = false
    private var startDown = false
    private val originalCallback: Window.Callback = activity.window.callback
    private val keyCallback = object : Window.Callback by originalCallback {
        override fun dispatchKeyEvent(event: KeyEvent): Boolean = onKey(event) || originalCallback.dispatchKeyEvent(event)
        // Java default methods, which delegation may not forward
        override fun onPointerCaptureChanged(hasCapture: Boolean) = originalCallback.onPointerCaptureChanged(hasCapture)
        override fun onProvideKeyboardShortcuts(data: MutableList<KeyboardShortcutGroup>?, menu: Menu?, deviceId: Int) =
            originalCallback.onProvideKeyboardShortcuts(data, menu, deviceId)
    }

    init {
        clipChildren = false
        addView(scrim, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        addView(pill, LayoutParams(LayoutParams.WRAP_CONTENT, dp(32)))
        placePill()
        ViewCompat.setOnApplyWindowInsetsListener(this) { _, insets ->
            // the notch and the system bars when shown: the pill stays clear of them
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            insetLeft = bars.left; insetTop = bars.top; insetRight = bars.right; insetBottom = bars.bottom
            placePill()
            insets
        }
        setUpPillTouch()
        activity.window.callback = keyCallback
        pill.alpha = restAlpha
        if (autoHide) pressHandler.postDelayed(hideLater, AUTO_HIDE_DELAY_MS)
    }

    /** The layout's Punica button the blades open from, the pill hidden; null shows the pill again. */
    fun useAnchor(anchor: View?) {
        if (this.anchor === anchor) return
        close()
        this.anchor = anchor
        pill.visibility = if (anchor == null) VISIBLE else GONE
    }

    /** Opens the blades, or closes them when open (the layout's Punica button). */
    fun toggle() {
        if (bladesOpen) close() else openBlades(main = true)
    }

    // the pill at rest: its chosen opacity, or none once hidden for play
    private fun rest() {
        if (bladesOpen) return
        pill.animate().alpha(if (autoHidden) 0f else restAlpha).setDuration(200).start()
    }

    // any touch on the screen (passed on to the game below) shows the hidden pill again
    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN && autoHide) {
            pressHandler.removeCallbacks(hideLater)
            if (autoHidden) { autoHidden = false; rest() }
            pressHandler.postDelayed(hideLater, AUTO_HIDE_DELAY_MS)
        }
        return super.dispatchTouchEvent(event)
    }

    // the controller: true when the key is the pill's, and the core must not get it
    private fun onKey(event: KeyEvent): Boolean {
        val code = event.keyCode
        val down = event.action == KeyEvent.ACTION_DOWN
        if (code == KeyEvent.KEYCODE_BUTTON_SELECT) selectDown = down
        if (code == KeyEvent.KEYCODE_BUTTON_START) startDown = down
        if (!bladesOpen) {
            if (!down || event.repeatCount > 0) return false
            val chord = (code == KeyEvent.KEYCODE_BUTTON_SELECT && startDown) || (code == KeyEvent.KEYCODE_BUTTON_START && selectDown)
            if (!chord && code != KeyEvent.KEYCODE_BUTTON_MODE) return false
            autoHidden = false
            openBlades(main = true)
            focus(0)
            return true
        }
        // open: the releases still reach the core, so no game key stays held
        if (!down) return false
        when (code) {
            KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_DPAD_DOWN -> focus(if (focused < 0) 0 else (focused + 1) % blades.size)
            KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_UP -> focus(if (focused <= 0) blades.size - 1 else focused - 1)
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER ->
                if (event.repeatCount == 0 && focused in blades.indices) {
                    blades[focused].findViewWithTag<View>(TAG_CIRCLE)?.performClick()
                    // Settings swapped the blades for System / Game: focus the first
                    if (bladesOpen && focused < 0) focus(0)
                }
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_BUTTON_MODE ->
                if (event.repeatCount == 0) { if (settingsShown) { openBlades(main = true); focus(0) } else close() }
        }
        return true
    }

    // the focused blade's circle gets a 3dp white ring; not enlarged, which would put it off the 48dp scale step
    private fun focus(index: Int) {
        blades.forEachIndexed { i, view ->
            val on = i == index
            val circle = view.findViewWithTag<View>(TAG_CIRCLE) ?: return@forEachIndexed
            circle.foreground = if (on) GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setStroke(dp(3), 0xFFFFFFFF.toInt())
            } else null
        }
        focused = index
    }

    private fun placePill() {
        val params = pill.layoutParams as LayoutParams
        val right = corner == 0 || corner == 2
        val bottom = corner >= 2
        params.gravity = (if (right) Gravity.END else Gravity.START) or (if (bottom) Gravity.BOTTOM else Gravity.TOP)
        params.setMargins(dp(16) + insetLeft, dp(12) + insetTop, dp(16) + insetRight, dp(12) + insetBottom)
        pill.layoutParams = params
        pill.translationX = 0f
        pill.translationY = 0f
    }

    // a tap opens or closes, a long press edits the shortcuts, a drag moves the pill to the nearest corner
    private fun setUpPillTouch() {
        val slop = ViewConfiguration.get(activity).scaledTouchSlop
        var downX = 0f
        var downY = 0f
        var dragging = false
        var longPressed = false
        val longPress = Runnable {
            longPressed = true
            pill.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
            close()
            editShortcuts()
        }
        pill.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = event.rawX; downY = event.rawY
                    dragging = false; longPressed = false
                    pill.animate().cancel()
                    pill.alpha = 1f
                    pressHandler.postDelayed(longPress, ViewConfiguration.getLongPressTimeout().toLong())
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = event.rawX - downX
                    val dy = event.rawY - downY
                    if (!dragging && !longPressed && hypot(dx, dy) > slop) {
                        dragging = true
                        pressHandler.removeCallbacks(longPress)
                        close()
                    }
                    if (dragging) {
                        pill.translationX = dx
                        pill.translationY = dy
                    }
                }
                MotionEvent.ACTION_UP -> {
                    pressHandler.removeCallbacks(longPress)
                    when {
                        dragging -> {
                            val cx = pill.x + pill.width / 2f
                            val cy = pill.y + pill.height / 2f
                            corner = (if (cx < width / 2f) 1 else 0) + (if (cy > height / 2f) 2 else 0)
                            preferences(activity).edit().putInt(KEY_CORNER, corner).apply()
                            placePill()
                        }
                        !longPressed -> if (bladesOpen) close() else openBlades(main = true)
                    }
                    rest()
                }
                MotionEvent.ACTION_CANCEL -> {
                    pressHandler.removeCallbacks(longPress)
                    if (dragging) placePill()
                    rest()
                }
            }
            true
        }
    }

    private class Blade(val id: Int, val label: CharSequence, val icon: Int, val style: Int)

    private fun mainBlades(): List<Blade> {
        val shown = available()
        val chosen = chosenShortcuts().filter { it in shown && it !in FIXED }
        val list = ArrayList<Blade>()
        if (R.id.emulator_menu_settings in shown) list += Blade(R.id.emulator_menu_settings, title(R.id.emulator_menu_settings), R.drawable.emulator_menu_ic_settings, STYLE_PLAIN)
        chosen.forEach { list += Blade(it, title(it), iconOf(it), STYLE_PLAIN) }
        if (R.id.emulator_menu_exit in shown) list += Blade(R.id.emulator_menu_exit, context.getString(R.string.emulator_pill_home), R.drawable.emulator_menu_ic_punica, STYLE_HOME)
        list += Blade(ID_EDIT, context.getString(R.string.emulator_pill_edit), R.drawable.emulator_menu_ic_tune, STYLE_EDIT)
        return list
    }

    private fun settingsBlades() = listOf(
        Blade(ID_BACK, context.getString(R.string.emulator_pill_back), R.drawable.emulator_menu_ic_restore, STYLE_PLAIN),
        Blade(R.id.emulator_menu_settings, context.getString(R.string.emulator_pill_system), R.drawable.emulator_menu_ic_settings, STYLE_SUB),
        Blade(R.id.emulator_menu_game_settings, context.getString(R.string.emulator_pill_game), R.drawable.emulator_menu_ic_game_settings, STYLE_SUB),
    )

    private fun openBlades(main: Boolean) {
        clearBlades()
        bladesOpen = true
        settingsShown = !main
        focused = -1
        pill.animate().cancel()
        pill.alpha = 1f
        pill.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK)
        scrim.visibility = VISIBLE
        scrim.animate().alpha(1f).setDuration(160).start()
        val items = if (main) mainBlades() else settingsBlades()
        val anchor = anchor
        val cx: Float
        val cy: Float
        if (anchor != null) {
            val here = IntArray(2).also { getLocationInWindow(it) }
            val there = IntArray(2).also { anchor.getLocationInWindow(it) }
            cx = there[0] - here[0] + anchor.width / 2f
            cy = there[1] - here[1] + anchor.height / 2f
        } else {
            cx = pill.x + pill.width / 2f
            cy = pill.y + pill.height / 2f
        }
        // toward the screen's middle: from the horizontal to near the vertical, wider in portrait
        val h = if (corner == 0 || corner == 2) -1 else 1
        val v = if (corner >= 2) -1 else 1
        val span = Math.toRadians(if (height > width) 80.0 else 50.0)
        val radius = dp(if (items.size > 4) 128 else 104)
        items.forEachIndexed { i, blade ->
            val view = bladeView(blade)
            // placed with its icon's centre on the pill's, then swung out by its translation
            addView(view, LayoutParams(dp(96), LayoutParams.WRAP_CONTENT).apply {
                gravity = Gravity.TOP or Gravity.START
                leftMargin = (cx - dp(48)).toInt()
                topMargin = (cy - dp(24)).toInt()
            })
            // from the button: a half circle above it, left to right; from the pill: toward the screen's middle
            val dx: Double
            val dy: Double
            if (anchor != null) {
                val angle = if (items.size == 1) PI / 2 else PI * (1 - i.toDouble() / (items.size - 1))
                dx = cos(angle) * radius
                dy = -sin(angle) * radius
            } else {
                val angle = if (items.size == 1) 0.0 else span * i / (items.size - 1)
                dx = h * cos(angle) * radius
                dy = v * sin(angle) * radius
            }
            view.alpha = 0f
            view.scaleX = 0.4f; view.scaleY = 0.4f
            view.animate()
                .translationX(dx.toFloat())
                .translationY(dy.toFloat())
                .alpha(1f).scaleX(1f).scaleY(1f)
                .setStartDelay(i * 30L).setDuration(240).start()
            blades += view
        }
        pill.bringToFront()
    }

    private fun bladeView(blade: Blade): View {
        val column = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
        }
        val circle = ImageView(activity).apply {
            setImageResource(blade.icon)
            scaleType = ImageView.ScaleType.CENTER_INSIDE
            setPadding(dp(12), dp(12), dp(12), dp(12))
            val fill = when (blade.style) {
                STYLE_HOME -> 0xFFFFFFFF.toInt()
                STYLE_SUB -> ContextCompat.getColor(activity, R.color.emulator_menu_primary_container)
                STYLE_EDIT -> 0x00000000
                else -> ContextCompat.getColor(activity, R.color.emulator_menu_surface)
            }
            background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(fill)
                setStroke(dp(1), if (blade.style == STYLE_EDIT) 0xFF9EA3AC.toInt() else 0x33FFFFFF)
            }
            if (blade.style != STYLE_HOME) {
                imageTintList = ColorStateList.valueOf(
                    ContextCompat.getColor(activity, if (blade.style == STYLE_SUB) R.color.emulator_menu_on_primary_container else R.color.emulator_menu_on_surface)
                )
            } else {
                setPadding(dp(8), dp(8), dp(8), dp(8))
            }
            contentDescription = blade.label
            tag = TAG_CIRCLE
            isFocusable = true
            setOnClickListener { onBlade(blade.id) }
        }
        column.addView(circle, LinearLayout.LayoutParams(dp(48), dp(48)))
        column.addView(TextView(activity).apply {
            text = blade.label
            textSize = 12f
            // Punica's title face, as every label of the app's design
            typeface = ResourcesCompat.getFont(activity, R.font.punica_bricolage_grotesque_bold)
            setTextColor(0xFFFFFFFF.toInt())
            setShadowLayer(4f, 0f, 0f, 0xFF000000.toInt())
            gravity = Gravity.CENTER_HORIZONTAL
            maxLines = 2
        }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(4) })
        return column
    }

    private fun onBlade(id: Int) {
        when (id) {
            ID_EDIT -> { close(); editShortcuts() }
            ID_BACK -> openBlades(main = true)
            R.id.emulator_menu_settings -> {
                // with per-game settings in the core: System or Game; otherwise straight to the settings
                if (!settingsShown && R.id.emulator_menu_game_settings in available()) openBlades(main = false)
                else { close(); onSelect(id) }
            }
            else -> { close(); onSelect(id) }
        }
    }

    /** Removes the pill (its screen's view is destroyed). */
    fun detach() {
        pressHandler.removeCallbacksAndMessages(null)
        // given back unless something wrapped it after the pill
        if (activity.window.callback === keyCallback) activity.window.callback = originalCallback
        (parent as? ViewGroup)?.removeView(this)
    }

    fun close() {
        if (!bladesOpen) return
        bladesOpen = false
        focused = -1
        clearBlades()
        scrim.animate().alpha(0f).setDuration(120).withEndAction { scrim.visibility = GONE }.start()
        rest()
    }

    private fun clearBlades() {
        blades.forEach { removeView(it) }
        blades.clear()
    }

    private fun editShortcuts() {
        val choices = available().filter { it !in FIXED && entries.findItem(it) != null }
        if (choices.isEmpty()) return
        val chosen = chosenShortcuts().toMutableList()
        val checked = BooleanArray(choices.size) { choices[it] in chosen }
        AlertDialog.Builder(themed)
            .setTitle(R.string.emulator_pill_edit_title)
            .setMultiChoiceItems(choices.map { title(it) }.toTypedArray(), checked) { dialog, which, isChecked ->
                val id = choices[which]
                if (isChecked && chosen.size >= MAX_SHORTCUTS) {
                    (dialog as AlertDialog).listView.setItemChecked(which, false)
                    Toast.makeText(activity, R.string.emulator_pill_too_many, Toast.LENGTH_SHORT).show()
                    return@setMultiChoiceItems
                }
                if (isChecked) chosen += id else chosen -= id
            }
            .setPositiveButton(android.R.string.ok) { _, _ -> saveShortcuts(chosen) }
            .setNegativeButton(android.R.string.cancel, null)
            .setNeutralButton(R.string.emulator_pill_default) { _, _ -> saveShortcuts(emptyList()) }
            .show()
    }

    // kept by entry name, not by id: ids change from one build to the next
    private fun chosenShortcuts(): List<Int> =
        (preferences(activity).getString(KEY_SHORTCUTS, "") ?: "").split(',').filter { it.isNotEmpty() }
            .map { resources.getIdentifier(it, "id", activity.packageName) }.filter { it != 0 }

    private fun saveShortcuts(ids: List<Int>) {
        val names = ids.mapNotNull { runCatching { resources.getResourceEntryName(it) }.getOrNull() }
        preferences(activity).edit().putString(KEY_SHORTCUTS, names.joinToString(",")).apply()
    }

    private fun title(id: Int): CharSequence = entries.findItem(id)?.title ?: ""
    private fun iconOf(id: Int): Int = ICONS[id] ?: R.drawable.emulator_menu_ic_tune

    private val ICONS = mapOf(
        R.id.emulator_menu_pause to R.drawable.emulator_menu_ic_pause,
        R.id.emulator_menu_save_state to R.drawable.emulator_menu_ic_save,
        R.id.emulator_menu_load_state to R.drawable.emulator_menu_ic_restore,
        R.id.emulator_menu_quick_save to R.drawable.emulator_menu_ic_save,
        R.id.emulator_menu_quick_load to R.drawable.emulator_menu_ic_restore,
        R.id.emulator_menu_rewind to R.drawable.emulator_menu_ic_rewind,
        R.id.emulator_menu_screenshot to R.drawable.emulator_menu_ic_screenshot,
        R.id.emulator_menu_swap_screens to R.drawable.emulator_menu_ic_splitscreen,
        R.id.emulator_menu_cheats to R.drawable.emulator_menu_ic_code,
        R.id.emulator_menu_quick_settings to R.drawable.emulator_menu_ic_tune,
        R.id.emulator_menu_game_settings to R.drawable.emulator_menu_ic_game_settings,
        R.id.emulator_menu_reset to R.drawable.emulator_menu_ic_reset,
        R.id.emulator_menu_controls to R.drawable.emulator_menu_ic_controller,
        R.id.emulator_menu_achievements to R.drawable.emulator_menu_ic_trophy,
        R.id.emulator_menu_amiibo to R.drawable.emulator_menu_ic_nfc,
        R.id.emulator_menu_change_disc to R.drawable.emulator_menu_ic_disc,
        R.id.emulator_menu_performance_overlay to R.drawable.emulator_menu_ic_tune,
    )
}

private const val PILL_COLOR = 0xA6111215.toInt()
private const val FAINT_ALPHA = 0.55f
private const val AUTO_HIDE_DELAY_MS = 4000L
private const val TAG_CIRCLE = "circle"
private const val STYLE_PLAIN = 0
private const val STYLE_HOME = 1
private const val STYLE_SUB = 2
private const val STYLE_EDIT = 3
private const val ID_EDIT = -1
private const val ID_BACK = -2
