package io.github.wwandene2.pomegrade.gba

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack

/**
 * Runs the loaded game: one frame per GBA frame (59.73 per second), the sound played as it comes.
 * The sound's blocking writes pace the frames; without sound (a muted or failed track) a clock does.
 */
internal class GbaEmulationThread(
    private val screen: GbaScreenView,
    private val keys: () -> Int,
) : Thread("GbaEmulation") {

    companion object {
        const val SAMPLE_RATE = 48000
        // the GBA's frame rate: 16,777,216 Hz / 280,896 cycles per frame
        private const val FRAME_NANOS = 280_896L * 1_000_000_000L / 16_777_216L
        const val FRAME_RATE = 16_777_216.0 / 280_896.0
    }

    @Volatile
    private var running = true

    @Volatile
    var paused = false

    /** Frames run since the start, for the performance overlay. */
    @Volatile
    var frames = 0L
        private set

    /** Work run between two frames on this thread (save states), so the core is never used twice at once. */
    private val pending = java.util.concurrent.ConcurrentLinkedQueue<() -> Unit>()

    fun runBetweenFrames(work: () -> Unit) {
        pending.add(work)
    }

    fun finish() {
        running = false
        join()
    }

    override fun run() {
        val audio = ShortArray(SAMPLE_RATE / 30 * 2)
        val track = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setAudioFormat(AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(SAMPLE_RATE).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
            .setBufferSizeInBytes(AudioTrack.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT) * 2)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        track.play()
        var next = System.nanoTime()
        try {
            while (running) {
                while (true) pending.poll()?.invoke() ?: break
                if (paused) {
                    sleep(16)
                    next = System.nanoTime()
                    continue
                }
                val samples = GbaNative.runFrame(keys(), audio)
                screen.publishFrame()
                frames++
                if (samples > 0 && track.playState == AudioTrack.PLAYSTATE_PLAYING) {
                    track.write(audio, 0, samples * 2, AudioTrack.WRITE_BLOCKING)
                    next = System.nanoTime()
                } else {
                    next += FRAME_NANOS
                    val wait = next - System.nanoTime()
                    if (wait > 0) sleep(wait / 1_000_000, (wait % 1_000_000).toInt())
                }
            }
            while (true) pending.poll()?.invoke() ?: break
        } finally {
            track.stop()
            track.release()
        }
    }
}
