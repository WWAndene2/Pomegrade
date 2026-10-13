package me.magnum.melonds.common.romprocessors

/** Text stored in a game's header (GBA cartridges, GameCube and Wii discs): fixed-length ASCII fields. */
internal object RomHeaderText {

    /** The field of [length] bytes at [offset]: printable ASCII up to the first NUL, trimmed; other bytes are dropped. */
    fun ascii(bytes: ByteArray, offset: Int, length: Int): String =
        bytes.copyOfRange(offset, offset + length)
            .takeWhile { it != 0.toByte() }
            .filter { it in 0x20..0x7E }
            .toByteArray()
            .toString(Charsets.US_ASCII)
            .trim()
}
