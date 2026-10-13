package me.magnum.melonds.impl

import android.content.Context
import android.net.Uri
import androidx.preference.PreferenceManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * The games the user starred on the home screen, kept by URI in the app's preferences (so they are in the
 * settings copy saved to the Punica folder too). A game whose file moves loses its star, as its URI changes.
 */
class FavoriteGames(context: Context) {

    private companion object {
        const val KEY = "punica_favorite_games"
    }

    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)
    private val _favorites = MutableStateFlow(preferences.getStringSet(KEY, emptySet()).orEmpty().toSet())
    val favorites: StateFlow<Set<String>> = _favorites.asStateFlow()

    fun isFavorite(uri: Uri) = uri.toString() in _favorites.value

    fun toggle(uri: Uri) {
        val key = uri.toString()
        val updated = if (key in _favorites.value) _favorites.value - key else _favorites.value + key
        preferences.edit().putStringSet(KEY, updated).apply()
        _favorites.value = updated
    }
}
