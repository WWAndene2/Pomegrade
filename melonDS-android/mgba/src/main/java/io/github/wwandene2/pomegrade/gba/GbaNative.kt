package io.github.wwandene2.pomegrade.gba

import java.nio.ByteBuffer

/**
 * The GBA core (mGBA, GbaCore.cpp). One game at a time, every call from the emulation thread except
 * [load] and [unload], which the screen makes while that thread is stopped.
 */
internal object GbaNative {

    const val WIDTH = 240
    const val HEIGHT = 160

    init {
        System.loadLibrary("pomegrade-gba")
    }

    /**
     * Loads [romPath], its battery save at [savePath] (created when the game first saves) and the
     * BIOS at [biosPath], or mGBA's own when null. Frames are written into [frameBuffer], a direct
     * buffer of [WIDTH] x [HEIGHT] RGBA pixels; sound comes at [sampleRate] Hz.
     */
    external fun load(romPath: String, savePath: String, biosPath: String?, frameBuffer: ByteBuffer, sampleRate: Int): Boolean

    /** Runs one frame with [keys] held ([GbaKeys] bits); returns the stereo samples written into [audio]. */
    external fun runFrame(keys: Int, audio: ShortArray): Int

    external fun saveState(path: String): Boolean

    external fun loadState(path: String): Boolean

    /**
     * Goes one step back in the rewind buffer (mGBA's, filled as the game runs: about 40 seconds) and shows
     * it; false when there is nothing left to go back to.
     */
    external fun rewindStep(): Boolean

    external fun reset()

    external fun unload()
}
