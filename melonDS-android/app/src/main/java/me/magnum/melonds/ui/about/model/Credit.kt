package me.magnum.melonds.ui.about.model

import androidx.annotation.StringRes

/** A project Pomegrade is built on or ships, as listed on the about screen. */
data class Credit(
    val name: String,
    @StringRes val role: Int,
    val authors: String,
    val licence: String,
    val url: String,
)
