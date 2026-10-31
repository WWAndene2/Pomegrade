package io.github.wwandene2.pomegrade.emulatorui

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.res.ColorStateList
import android.graphics.Rect
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
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

/**
 * The shortcut pill, the same over every console's game: a small pill on an edge of the screen (top right by default;
 * dragged, it stays where it is let go, against the nearest side) that, touched, opens a panel of the user's
 * shortcuts beside it: tiles three to a row, each an icon over its name, never overlapping (owner's choice, 31
 * October: the fan of blades before it overlapped its labels from five shortcuts). A shortcut is one of the shared
 * in-game menu's entries (`R.menu.emulator_menu`), so it does exactly what that entry does in the core's menu:
 * [onSelect] gets the entry's id. Two are always there: Settings and the Punica logo, the menu's Exit, back to
 * Punica's game list. Settings opens a second page of the panel with its direct ways in: the core's Video, Audio
 * and Controls pages ([openPage], when the core has them), its System settings and This game's (when the core has
 * per-game settings). The other shortcuts are the user's choice and order (a long press, or the Edit tile), up to
 * [MAX_SHORTCUTS] among the entries the core shows ([available]); the choice is kept for every console.
 *
 * A screen's layout can show a button of its own for it (the DS Classic layout, between SELECT and START):
 * given by [useAnchor], the pill hides and the panel opens above that button, which [toggle]s it.
 *
 * Whether the screen shows the pill, the drawer's edge swipe or both is the user's choice too
 * ([KEY_ACCESS], Punica's settings): with the pill alone, the core locks its drawer's edge swipe
 * ([edgeSwipeAllowed]), which also leaves the screen's edge to Android's Back gesture. The drawer stays
 * reachable from Back and from the controller, as each core opens it.
 *
 * At rest the pill is visible, faint or hidden ([KEY_OPACITY]; hidden, a touch on its place still opens it), its size
 * the user's ([KEY_SIZE]), and it can hide itself after a few seconds without a touch on the screen
 * ([KEY_AUTO_HIDE]); any touch shows it again. A controller opens it with Select + Start pressed together, or its
 * Home button; then the D-pad moves between the tiles (left / right along a row, up / down between rows), A picks one
 * and B closes (or goes back from the Settings page). It sees the keys before the core does by wrapping the window's
 * callback, so no core has to forward them; while it is open the core gets no key press from the controller, only
 * the releases.
 */
@SuppressLint("ViewConstructor", "ClickableViewAccessibility")
class ShortcutPill private constructor(
    private val activity: Activity,
    private val available: () -> Set<Int>,
    private val onSelect: (Int) -> Unit,
    private val openPage: ((SettingsPage) -> Unit)?,
) : FrameLayout(activity) {

    /** The settings pages the panel's Settings page opens directly, each core's own (see [attach]). */
    enum class SettingsPage { VIDEO, AUDIO, CONTROLS }

    companion object {
        /** "pill" (default), "drawer" or "both"; in the app's default preferences, which the settings screen writes. */
        const val KEY_ACCESS = "punica_menu_access"
        private const val KEY_SHORTCUTS = "punica_pill_shortcuts"
        // before 31 October the pill sat in a corner (0 top right, 1 top left, 2 bottom right, 3 bottom left)
        private const val KEY_CORNER = "punica_pill_corner"
        // its side (0 right, 1 left) and its height on the screen (0 top to 1 bottom)
        private const val KEY_SIDE = "punica_pill_side"
        private const val KEY_HEIGHT = "punica_pill_height"
        /** "visible", "faint" (default) or "hidden": the pill at rest. */
        const val KEY_OPACITY = "punica_pill_opacity"
        /** True: the pill hides after a few seconds without a touch on the screen. */
        const val KEY_AUTO_HIDE = "punica_pill_auto_hide"
        /** "small", "normal" (default) or "large": the pill's size. */
        const val KEY_SIZE = "punica_pill_size"
        private const val MAX_SHORTCUTS = 9
        private const val COLUMNS = 3
        private val FIXED = listOf(R.id.emulator_menu_settings, R.id.emulator_menu_exit, R.id.emulator_menu_game_settings)

        private fun preferences(context: Context): SharedPreferences =
            context.getSharedPreferences(context.packageName + "_preferences", Context.MODE_PRIVATE)

        private fun access(context: Context) = preferences(context).getString(KEY_ACCESS, "pill") ?: "pill"

        /** False when the pill is the only way to the menu: the core then locks its drawer's edge swipe. */
        fun edgeSwipeAllowed(context: Context) = access(context) != "pill"

        /**
         * Puts the pill over [activity]'s screen, unless the user chose the drawer alone (then null).
         * [available]: the menu entries the core shows now; [onSelect]: runs one, as the core's menu does;
         * [openPage]: opens one of the core's settings pages (null: the core has none, its Settings page shows System
         * and This game only).
         */
        fun attach(
            activity: Activity,
            available: () -> Set<Int>,
            onSelect: (Int) -> Unit,
            openPage: ((SettingsPage) -> Unit)? = null,
        ): ShortcutPill? {
            if (access(activity) == "drawer") return null
            val content = activity.findViewById<ViewGroup>(android.R.id.content) ?: return null
            return ShortcutPill(activity, available, onSelect, openPage).also {
                content.addView(it, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
            }
        }
    }

    private val themed = ContextThemeWrapper(activity, R.style.Theme_Pomegrade_EmulatorMenu)
    private val density = resources.displayMetrics.density
    private fun dp(value: Int) = (value * density).toInt()

    // the shared menu's entries, for the shortcuts' titles and icons
    private val entries: Menu = PopupMenu(activity, this).menu.also { MenuInflater(activity).inflate(R.menu.emulator_menu, it) }

    // the pill's height: 24, 32 (default) or 48 dp
    private val pillHeight = dp(
        when (preferences(activity).getString(KEY_SIZE, "normal")) {
            "small" -> 24
            "large" -> 48
            else -> 32
        }
    )
    private val scrim = View(activity).apply {
        setBackgroundColor(0x59000000)
        alpha = 0f
        visibility = GONE
        setOnClickListener { close() }
    }
    private val pill = LinearLayout(activity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(pillHeight * 3 / 16, 0, pillHeight * 3 / 8, 0)
        minimumWidth = pillHeight * 3 / 2
        background = GradientDrawable().apply { cornerRadius = pillHeight / 2f; setColor(PILL_COLOR) }
        contentDescription = context.getString(R.string.emulator_pill)
        isFocusable = true
        addView(ImageView(activity).apply { setImageResource(R.drawable.emulator_menu_ic_punica) },
            LinearLayout.LayoutParams(pillHeight * 3 / 4, pillHeight * 3 / 4))
        val dots = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
        repeat(3) {
            dots.addView(View(activity).apply {
                background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(0xFF9EA3AC.toInt()) }
            }, LinearLayout.LayoutParams(pillHeight / 8, pillHeight / 8).apply { marginStart = pillHeight * 3 / 32 })
        }
        addView(dots, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { marginStart = pillHeight / 8 })
    }
    // the open panel and its tiles' circles (the controller's targets), in reading order
    private var panel: View? = null
    private val tiles = ArrayList<View>()
    private var panelOpen = false
    // the panel shows the Settings page
    private var settingsShown = false
    private var side = 0
    private var heightFraction = 0f
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
    // the layout's Punica button when the screen shows one: the panel opens above it, the pill is hidden
    private var anchor: View? = null
    private var anchorArea: Rect? = null
    // the button's area read when the panel opens (its last position, not the one when the controls were built)
    private var anchorAreaOf: (() -> Rect?)? = null
    // the controller's focused tile, -1 when none
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
        val prefs = preferences(activity)
        if (prefs.contains(KEY_SIDE)) {
            side = prefs.getInt(KEY_SIDE, 0)
            heightFraction = prefs.getFloat(KEY_HEIGHT, 0f)
        } else {
            // the corner it had before
            val corner = prefs.getInt(KEY_CORNER, 0)
            side = if (corner == 1 || corner == 3) 1 else 0
            heightFraction = if (corner >= 2) 1f else 0f
        }
        addView(scrim, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        addView(pill, LayoutParams(LayoutParams.WRAP_CONTENT, pillHeight))
        ViewCompat.setOnApplyWindowInsetsListener(this) { _, insets ->
            // the notch and the system bars when shown: the pill stays clear of them
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            insetLeft = bars.left; insetTop = bars.top; insetRight = bars.right; insetBottom = bars.bottom
            placePill()
            insets
        }
        addOnLayoutChangeListener { _, _, top, _, bottom, _, oldTop, _, oldBottom -> if (bottom - top != oldBottom - oldTop) placePill() }
        setUpPillTouch()
        activity.window.callback = keyCallback
        pill.alpha = restAlpha
        if (autoHide) pressHandler.postDelayed(hideLater, AUTO_HIDE_DELAY_MS)
    }

    /**
     * The layout's Punica button the panel opens from, the pill hidden; null shows the pill again. [bubbleSize]: kept
     * for the layouts that give their buttons' size (the panel's tiles keep theirs). [area]: the button's part of
     * [anchor] when the view draws more than the button (a core's overlay drawing every control), else null.
     */
    @Suppress("UNUSED_PARAMETER")
    fun useAnchor(anchor: View?, bubbleSize: Int = 0, area: Rect? = null, areaOf: (() -> Rect?)? = null) {
        anchorArea = area
        anchorAreaOf = areaOf
        if (this.anchor === anchor) return
        close()
        this.anchor = anchor
        pill.visibility = if (anchor == null) VISIBLE else GONE
    }

    /** Opens the panel, or closes it when open (the layout's Punica button). */
    fun toggle() {
        if (panelOpen) close() else openPanel(settings = false)
    }

    // the pill at rest: its chosen opacity, or none once hidden for play
    private fun rest() {
        if (panelOpen) return
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
        if (!panelOpen) {
            if (!down || event.repeatCount > 0) return false
            val chord = (code == KeyEvent.KEYCODE_BUTTON_SELECT && startDown) || (code == KeyEvent.KEYCODE_BUTTON_START && selectDown)
            if (!chord && code != KeyEvent.KEYCODE_BUTTON_MODE) return false
            autoHidden = false
            openPanel(settings = false)
            focus(0)
            return true
        }
        // open: the releases still reach the core, so no game key stays held
        if (!down) return false
        val last = tiles.size - 1
        when (code) {
            KeyEvent.KEYCODE_DPAD_RIGHT -> focus(if (focused < 0) 0 else min(focused + 1, last))
            KeyEvent.KEYCODE_DPAD_LEFT -> focus(if (focused < 0) 0 else max(focused - 1, 0))
            KeyEvent.KEYCODE_DPAD_DOWN -> focus(if (focused < 0) 0 else min(focused + COLUMNS, last))
            KeyEvent.KEYCODE_DPAD_UP -> focus(if (focused < 0) 0 else max(focused - COLUMNS, 0))
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER ->
                if (event.repeatCount == 0 && focused in tiles.indices) {
                    tiles[focused].performClick()
                    // Settings swapped the page: focus its first tile
                    if (panelOpen && focused < 0) focus(0)
                }
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_BUTTON_MODE ->
                if (event.repeatCount == 0) { if (settingsShown) { openPanel(settings = false); focus(0) } else close() }
        }
        return true
    }

    // the focused tile's circle gets a 3dp white ring
    private fun focus(index: Int) {
        tiles.forEachIndexed { i, circle ->
            circle.foreground = if (i == index) GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setStroke(dp(3), 0xFFFFFFFF.toInt())
            } else null
        }
        focused = index
    }

    // the pill against its side, at its height between the insets
    private fun placePill() {
        val params = pill.layoutParams as LayoutParams
        params.gravity = (if (side == 0) Gravity.END else Gravity.START) or Gravity.TOP
        val room = height - insetTop - insetBottom - dp(24) - pillHeight
        params.setMargins(dp(16) + insetLeft, insetTop + dp(12) + (max(room, 0) * heightFraction).toInt(), dp(16) + insetRight, 0)
        pill.layoutParams = params
        pill.translationX = 0f
        pill.translationY = 0f
    }

    // a tap opens or closes, a long press edits the shortcuts, a drag moves the pill along the nearest side
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
                            val top = pill.y - insetTop - dp(12)
                            val room = height - insetTop - insetBottom - dp(24) - pillHeight
                            side = if (cx < width / 2f) 1 else 0
                            heightFraction = if (room > 0) (top / room).coerceIn(0f, 1f) else 0f
                            preferences(activity).edit().putInt(KEY_SIDE, side).putFloat(KEY_HEIGHT, heightFraction).apply()
                            placePill()
                        }
                        !longPressed -> if (panelOpen) close() else openPanel(settings = false)
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

    private class Tile(val id: Int, val label: CharSequence, val icon: Int, val style: Int)

    private fun mainTiles(): List<Tile> {
        val shown = available()
        val chosen = chosenShortcuts().filter { it in shown && it !in FIXED }
        val list = ArrayList<Tile>()
        chosen.forEach { list += Tile(it, title(it), iconOf(it), STYLE_PLAIN) }
        if (R.id.emulator_menu_settings in shown) list += Tile(ID_SETTINGS, context.getString(R.string.emulator_pill_settings_title), R.drawable.emulator_menu_ic_settings, STYLE_PLAIN)
        if (R.id.emulator_menu_exit in shown) list += Tile(R.id.emulator_menu_exit, context.getString(R.string.emulator_pill_home), R.drawable.emulator_menu_ic_punica, STYLE_HOME)
        list += Tile(ID_EDIT, context.getString(R.string.emulator_pill_edit), R.drawable.emulator_menu_ic_tune, STYLE_EDIT)
        return list
    }

    // the Settings page: back, the core's pages, its System settings and this game's
    private fun settingsTiles(): List<Tile> {
        val shown = available()
        val list = arrayListOf(Tile(ID_BACK, context.getString(R.string.emulator_pill_back), R.drawable.emulator_menu_ic_restore, STYLE_EDIT))
        if (openPage != null) {
            list += Tile(ID_PAGE_VIDEO, context.getString(R.string.emulator_pill_video), R.drawable.emulator_menu_ic_video, STYLE_SUB)
            list += Tile(ID_PAGE_AUDIO, context.getString(R.string.emulator_pill_audio), R.drawable.emulator_menu_ic_audio, STYLE_SUB)
            list += Tile(ID_PAGE_CONTROLS, context.getString(R.string.emulator_pill_controls), R.drawable.emulator_menu_ic_controller, STYLE_SUB)
        }
        list += Tile(R.id.emulator_menu_settings, context.getString(R.string.emulator_pill_system), R.drawable.emulator_menu_ic_settings, STYLE_SUB)
        if (R.id.emulator_menu_game_settings in shown) {
            list += Tile(R.id.emulator_menu_game_settings, context.getString(R.string.emulator_pill_game), R.drawable.emulator_menu_ic_game_settings, STYLE_SUB)
        }
        return list
    }

    private fun openPanel(settings: Boolean) {
        clearPanel()
        panelOpen = true
        settingsShown = settings
        focused = -1
        pill.animate().cancel()
        pill.alpha = 1f
        pill.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK)
        if (scrim.visibility != VISIBLE) {
            scrim.visibility = VISIBLE
            scrim.animate().alpha(1f).setDuration(120).start()
        }
        val items = if (settings) settingsTiles() else mainTiles()
        val tileWidth = dp(96) // PerfectSuite: 64 + 32 (§7.2), two-line labels fit
        val padding = dp(12)
        val grid = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding, padding, padding)
            background = GradientDrawable().apply {
                cornerRadius = dp(24).toFloat()
                setColor(ContextCompat.getColor(activity, R.color.emulator_menu_surface))
                setStroke(dp(1), 0xFF2B2E35.toInt())
            }
            elevation = dp(8).toFloat()
            isClickable = true // a touch between the tiles stays on the panel
        }
        if (settings) {
            grid.addView(TextView(activity).apply {
                text = context.getString(R.string.emulator_pill_settings_title)
                textSize = 16f
                typeface = ResourcesCompat.getFont(activity, R.font.punica_bricolage_grotesque_bold)
                setTextColor(ContextCompat.getColor(activity, R.color.emulator_menu_on_surface))
                setPadding(dp(4), 0, dp(4), dp(8))
            })
        }
        items.chunked(COLUMNS).forEach { row ->
            val line = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
            row.forEach { tile -> line.addView(tileView(tile), LinearLayout.LayoutParams(tileWidth, LinearLayout.LayoutParams.WRAP_CONTENT)) }
            grid.addView(line)
        }
        val scroll = ScrollView(activity).apply {
            isVerticalScrollBarEnabled = false
            addView(grid)
        }
        val panelWidth = tileWidth * min(COLUMNS, items.size) + 2 * padding
        // beside the pill, toward the screen's middle, or above the layout's Punica button; inside the screen
        val anchor = anchor
        val maxHeight = height - insetTop - insetBottom - dp(24)
        grid.measure(MeasureSpec.makeMeasureSpec(panelWidth, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
        val panelHeight = min(grid.measuredHeight, maxHeight)
        val left: Int
        val top: Int
        if (anchor != null) {
            val here = IntArray(2).also { getLocationInWindow(it) }
            val there = IntArray(2).also { anchor.getLocationInWindow(it) }
            // null: the anchor is the button itself; an empty area (controls not placed yet): the anchor's bottom middle
            val area = (anchorAreaOf?.invoke() ?: anchorArea)?.let { if (it.isEmpty) Rect(0, anchor.height, anchor.width, anchor.height) else it }
            val cx = there[0] - here[0] + (area?.exactCenterX() ?: (anchor.width / 2f))
            val buttonTop = there[1] - here[1] + (area?.top ?: 0)
            left = (cx - panelWidth / 2f).toInt()
            top = buttonTop - dp(12) - panelHeight
        } else {
            left = if (side == 0) (pill.x + pill.width - panelWidth).toInt() else pill.x.toInt()
            val below = pill.y + pill.height + dp(8)
            top = if (pill.y + pill.height / 2f < height / 2f) below.toInt() else (pill.y - dp(8) - panelHeight).toInt()
        }
        val params = LayoutParams(panelWidth, panelHeight).apply {
            gravity = Gravity.TOP or Gravity.START
            leftMargin = left.coerceIn(insetLeft + dp(8), max(insetLeft + dp(8), width - insetRight - dp(8) - panelWidth))
            topMargin = top.coerceIn(insetTop + dp(12), max(insetTop + dp(12), height - insetBottom - dp(12) - panelHeight))
        }
        addView(scroll, params)
        panel = scroll
        // fast: a short fade and scale from the pill's side
        scroll.pivotX = if (anchor != null) panelWidth / 2f else if (side == 0) panelWidth.toFloat() else 0f
        scroll.pivotY = if (anchor != null || params.topMargin < pill.y) panelHeight.toFloat() else 0f
        scroll.alpha = 0f
        scroll.scaleX = 0.92f; scroll.scaleY = 0.92f
        scroll.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(120).start()
        pill.bringToFront()
    }

    private fun tileView(tile: Tile): View {
        val column = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(4), dp(4), dp(4), dp(8))
        }
        val size = dp(48)
        val circle = ImageView(activity).apply {
            setImageResource(tile.icon)
            scaleType = ImageView.ScaleType.CENTER_INSIDE
            setPadding(size / 4, size / 4, size / 4, size / 4)
            val fill = when (tile.style) {
                STYLE_HOME -> 0xFFFFFFFF.toInt()
                STYLE_SUB -> ContextCompat.getColor(activity, R.color.emulator_menu_primary_container)
                STYLE_EDIT -> 0x00000000
                else -> ContextCompat.getColor(activity, R.color.emulator_menu_secondary_container)
            }
            background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(fill)
                setStroke(dp(1), if (tile.style == STYLE_EDIT) 0xFF9EA3AC.toInt() else 0x33FFFFFF)
            }
            if (tile.style != STYLE_HOME) {
                imageTintList = ColorStateList.valueOf(
                    ContextCompat.getColor(activity, if (tile.style == STYLE_SUB) R.color.emulator_menu_on_primary_container else R.color.emulator_menu_on_surface)
                )
            } else {
                setPadding(size / 6, size / 6, size / 6, size / 6)
            }
            contentDescription = tile.label
            isFocusable = true
            setOnClickListener { onTile(tile.id) }
        }
        column.addView(circle, LinearLayout.LayoutParams(size, size))
        column.addView(TextView(activity).apply {
            text = tile.label
            textSize = 12f
            setTextColor(ContextCompat.getColor(activity, R.color.emulator_menu_on_surface))
            gravity = Gravity.CENTER_HORIZONTAL
            maxLines = 2
            ellipsize = android.text.TextUtils.TruncateAt.END
        }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(4) })
        // the whole tile answers the touch, as its circle
        column.setOnClickListener { circle.performClick() }
        tiles += circle
        return column
    }

    private fun onTile(id: Int) {
        when (id) {
            ID_EDIT -> { close(); editShortcuts() }
            ID_BACK -> openPanel(settings = false)
            // a core without its pages nor per-game settings: straight to its settings, no page of one choice
            ID_SETTINGS -> if (openPage == null && R.id.emulator_menu_game_settings !in available()) { close(); onSelect(R.id.emulator_menu_settings) }
                else openPanel(settings = true)
            ID_PAGE_VIDEO -> { close(); openPage?.invoke(SettingsPage.VIDEO) }
            ID_PAGE_AUDIO -> { close(); openPage?.invoke(SettingsPage.AUDIO) }
            ID_PAGE_CONTROLS -> { close(); openPage?.invoke(SettingsPage.CONTROLS) }
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
        if (!panelOpen) return
        panelOpen = false
        settingsShown = false
        focused = -1
        clearPanel()
        scrim.animate().alpha(0f).setDuration(120).withEndAction { scrim.visibility = GONE }.start()
        rest()
    }

    private fun clearPanel() {
        panel?.let { removeView(it) }
        panel = null
        tiles.clear()
    }

    // the shortcuts the user picks and orders: each entry the core shows, ticked or not, with its arrows; the
    // ticked ones show in the panel in this list's order
    private fun editShortcuts() {
        val choices = available().filter { it !in FIXED && entries.findItem(it) != null }
        if (choices.isEmpty()) return
        val chosen = chosenShortcuts().filter { it in choices }.toMutableList()
        // the chosen first, in their order, then the others in the menu's
        val order = (chosen + choices.filter { it !in chosen }).toMutableList()
        val list = LinearLayout(themed).apply { orientation = LinearLayout.VERTICAL }
        fun refresh() {
            list.removeAllViews()
            list.addView(TextView(themed).apply {
                text = context.getString(R.string.emulator_pill_edit_hint)
                textSize = 12f
                setTextColor(ContextCompat.getColor(activity, R.color.emulator_menu_on_surface_variant))
                setPadding(dp(24), dp(4), dp(24), dp(8))
            })
            order.forEachIndexed { index, id ->
                val row = LinearLayout(themed).apply {
                    orientation = LinearLayout.HORIZONTAL
                    gravity = Gravity.CENTER_VERTICAL
                    minimumHeight = dp(48)
                    setPadding(dp(16), 0, dp(8), 0)
                }
                val box = android.widget.CheckBox(themed).apply {
                    text = title(id)
                    isChecked = id in chosen
                    setOnCheckedChangeListener { view, isChecked ->
                        if (isChecked && id !in chosen) {
                            if (chosen.size >= MAX_SHORTCUTS) {
                                view.isChecked = false
                                Toast.makeText(activity, R.string.emulator_pill_too_many, Toast.LENGTH_SHORT).show()
                                return@setOnCheckedChangeListener
                            }
                            chosen += id
                        } else if (!isChecked) {
                            chosen -= id
                        }
                    }
                }
                row.addView(box, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
                fun arrow(up: Boolean) = android.widget.ImageButton(themed).apply {
                    setImageResource(R.drawable.emulator_menu_ic_arrow_up)
                    rotation = if (up) 0f else 180f
                    background = null
                    contentDescription = context.getString(if (up) R.string.emulator_pill_move_up else R.string.emulator_pill_move_down)
                    imageTintList = ColorStateList.valueOf(ContextCompat.getColor(activity, R.color.emulator_menu_on_surface))
                    val target = if (up) index - 1 else index + 1
                    isEnabled = target in order.indices
                    alpha = if (isEnabled) 1f else 0.3f
                    setOnClickListener {
                        java.util.Collections.swap(order, index, target)
                        refresh()
                    }
                }
                row.addView(arrow(up = true), LinearLayout.LayoutParams(dp(48), dp(48)))
                row.addView(arrow(up = false), LinearLayout.LayoutParams(dp(48), dp(48)))
                list.addView(row)
            }
        }
        refresh()
        AlertDialog.Builder(themed)
            .setTitle(R.string.emulator_pill_edit_title)
            .setView(ScrollView(themed).apply { addView(list) })
            .setPositiveButton(android.R.string.ok) { _, _ -> saveShortcuts(order.filter { it in chosen }) }
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
        R.id.emulator_menu_home to R.drawable.emulator_menu_ic_home,
        R.id.emulator_menu_performance_overlay to R.drawable.emulator_menu_ic_tune,
    )
}

private const val PILL_COLOR = 0xA6111215.toInt()
private const val FAINT_ALPHA = 0.55f
private const val AUTO_HIDE_DELAY_MS = 4000L
private const val STYLE_PLAIN = 0
private const val STYLE_HOME = 1
private const val STYLE_SUB = 2
private const val STYLE_EDIT = 3
private const val ID_EDIT = -1
private const val ID_BACK = -2
private const val ID_SETTINGS = -3
private const val ID_PAGE_VIDEO = -4
private const val ID_PAGE_AUDIO = -5
private const val ID_PAGE_CONTROLS = -6
