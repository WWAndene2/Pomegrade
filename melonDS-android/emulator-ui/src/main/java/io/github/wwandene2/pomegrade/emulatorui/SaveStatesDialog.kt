package io.github.wwandene2.pomegrade.emulatorui

import android.content.Context
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.view.ContextThemeWrapper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/**
 * The save state slots dialog of both consoles: pick a slot to save to or load from, or delete one.
 * When loading, empty slots can't be picked.
 */
object SaveStatesDialog {

    /**
     * @param onSlotDeleted deletes a slot and returns the updated slots, or null when the core
     * can't delete save states (the delete buttons are then hidden)
     * @param onCancel the dialog was closed without picking a slot
     * @param onDismiss the dialog went away, picked or not
     */
    fun show(
        context: Context,
        saving: Boolean,
        slots: List<SaveStateSlotUi>,
        onSlotPicked: (SaveStateSlotUi) -> Unit,
        onSlotDeleted: ((SaveStateSlotUi) -> List<SaveStateSlotUi>?)?,
        onCancel: () -> Unit,
        onDismiss: () -> Unit,
    ): AlertDialog {
        val themedContext = ContextThemeWrapper(context, R.style.Theme_Pomegrade_EmulatorMenu)
        var dialog: AlertDialog? = null
        val adapter = SlotAdapter(
            saving = saving,
            slots = slots,
            onSlotPicked = {
                dialog?.dismiss()
                onSlotPicked(it)
            },
            onSlotDeleted = onSlotDeleted,
        )
        val list = RecyclerView(themedContext).apply {
            layoutManager = LinearLayoutManager(themedContext)
            this.adapter = adapter
            descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS
        }

        val shown = MaterialAlertDialogBuilder(themedContext)
            .setTitle(if (saving) R.string.emulator_save_states_save_title else R.string.emulator_save_states_load_title)
            .setView(list)
            .setNegativeButton(R.string.emulator_save_states_cancel) { d, _ -> d.cancel() }
            .setOnCancelListener { onCancel() }
            .setOnDismissListener { onDismiss() }
            .show()
        dialog = shown
        return shown
    }

    private class SlotAdapter(
        private val saving: Boolean,
        slots: List<SaveStateSlotUi>,
        private val onSlotPicked: (SaveStateSlotUi) -> Unit,
        private val onSlotDeleted: ((SaveStateSlotUi) -> List<SaveStateSlotUi>?)?,
    ) : RecyclerView.Adapter<SlotAdapter.ViewHolder>() {

        private var items = slots

        override fun getItemCount() = items.size

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.emulator_save_state_slot, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(items[position])
        }

        inner class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
            private val screenshot = view.findViewById<ImageView>(R.id.save_state_slot_screenshot)
            private val name = view.findViewById<TextView>(R.id.save_state_slot_name)
            private val date = view.findViewById<TextView>(R.id.save_state_slot_date)
            private val delete = view.findViewById<ImageButton>(R.id.save_state_slot_delete)

            fun bind(slot: SaveStateSlotUi) {
                val context = itemView.context
                name.text = if (slot.isQuickSlot) {
                    context.getString(R.string.emulator_save_states_quick_slot)
                } else {
                    context.getString(R.string.emulator_save_states_slot, slot.slot)
                }
                date.text = slot.savedAt?.let {
                    "${DateFormat.getMediumDateFormat(context).format(it)} ${DateFormat.getTimeFormat(context).format(it)}"
                } ?: context.getString(R.string.emulator_save_states_empty)

                screenshot.setImageDrawable(null)
                if (slot.exists && slot.screenshot != null) {
                    // a missing or unreadable file leaves the view empty, which is then hidden
                    screenshot.setImageURI(slot.screenshot)
                }
                screenshot.visibility = if (screenshot.drawable != null) View.VISIBLE else View.GONE

                val pickable = saving || slot.exists
                itemView.isEnabled = pickable
                itemView.alpha = if (pickable) 1f else 0.5f
                itemView.setOnClickListener { if (pickable) onSlotPicked(slot) }

                delete.visibility = if (slot.exists && onSlotDeleted != null) View.VISIBLE else View.INVISIBLE
                delete.setOnClickListener {
                    onSlotDeleted?.invoke(slot)?.let { updated ->
                        items = updated
                        notifyDataSetChanged()
                    }
                }
            }
        }
    }
}
