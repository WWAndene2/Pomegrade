package me.magnum.melonds.impl.diagnostics

import android.content.Context
import android.os.Build
import io.github.muntashirakon.adb.AdbStream
import io.github.muntashirakon.adb.android.AdbMdns
import io.github.muntashirakon.adb.android.AndroidUtils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.impl.PunicaLayout
import java.io.File
import java.io.IOException
import java.io.OutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/**
 * What Punica does over wireless debugging (Android 11+): pair with the code Android shows, connect, run one of the
 * [DiagnosticCommand]s, disconnect. Nothing here takes a command from outside the enum.
 */
object WirelessDebugDiagnostics {

    // the pairing outlives the screen that started it: the code is typed in a notification, over Android's settings
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var pairingPort: Deferred<Int?>? = null

    sealed interface Result {
        data class Written(val file: File) : Result
        data class Started(val folder: File, val delaySeconds: Int) : Result
        data class Failed(val reason: String) : Result
    }

    val isSupported: Boolean get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R

    val isConnected: Boolean get() = runCatching { WirelessDebugConnection.instance.isConnected }.getOrDefault(false)

    /** The port of Android's "pair with a pairing code" dialog, found on the local network (mDNS), or null. */
    suspend fun findPairingPort(context: Context, timeoutSeconds: Long = 60): Int? = withContext(Dispatchers.IO) {
        val port = AtomicInteger(-1)
        val found = CountDownLatch(1)
        val mdns = AdbMdns(context.applicationContext, AdbMdns.SERVICE_TYPE_TLS_PAIRING) { _, discovered ->
            port.set(discovered)
            found.countDown()
        }
        mdns.start()
        try {
            found.await(timeoutSeconds, TimeUnit.SECONDS)
        } finally {
            mdns.stop()
        }
        port.get().takeIf { it > 0 }
    }

    /** Starts looking for the pairing dialog's port, for [pairAndConnect] when the code is typed. */
    fun startPairing(context: Context) {
        val appContext = context.applicationContext
        pairingPort?.cancel()
        pairingPort = scope.async { findPairingPort(appContext, timeoutSeconds = 120) }
    }

    /** Pairs with the code typed in the notification, on the port [startPairing] found; false if anything fails. */
    suspend fun pairAndConnect(context: Context, code: String): Boolean {
        val port = withTimeoutOrNull(5_000) { pairingPort?.await() } ?: return false
        return pairAndConnect(context, port, code)
    }

    /** Pairs with the code of Android's dialog, then connects; false if either fails. */
    suspend fun pairAndConnect(context: Context, port: Int, code: String): Boolean = withContext(Dispatchers.IO) {
        if (!isSupported) return@withContext false
        val connection = WirelessDebugConnection.instance
        val host = AndroidUtils.getHostIpAddress(context.applicationContext)
        runCatching { connection.pair(host, port, code.trim()) }.getOrDefault(false) &&
            runCatching { connection.connectTls(context.applicationContext, 5_000) }.getOrDefault(false)
    }

    suspend fun disconnect() = withContext(Dispatchers.IO) {
        runCatching { WirelessDebugConnection.instance.disconnect() }
    }

    /**
     * The whole output of [stream] copied into [output], up to adbd closing it. libadb-android (3.1.1, AdbStream.read) reports that close
     * by throwing IOException("Stream closed.") when it arrives while a read waits, instead of returning -1: every
     * command ended that way, and the detached ones (the system trace, the full analysis), which close at once,
     * always showed "Stream closed" although they had started (owner, 1 November). Once the stream is closed, the
     * exception is its end; before, it is a real failure.
     */
    private fun copyUntilClosed(stream: AdbStream, output: OutputStream) {
        val buffer = ByteArray(65536)
        val input = stream.openInputStream()
        while (true) {
            val read = try {
                input.read(buffer)
            } catch (e: IOException) {
                if (stream.isClosed) break else throw e
            }
            if (read < 0) break
            output.write(buffer, 0, read)
        }
    }

    /**
     * Runs [command]; its files go beside Eden's log. A delayed one (the system trace, the full analysis) starts after
     * [delaySeconds] in Punica's own process and returns at once: its steps run while Punica is in a game, each output
     * written by Punica (1 November: the shell-side version, detached with setsid and writing the files itself, left
     * nothing on the owner's phone). A step that fails leaves a punica-<stamp>-error.txt saying which and why.
     */
    suspend fun run(context: Context, command: DiagnosticCommand, delaySeconds: Int = 0): Result = withContext(Dispatchers.IO) {
        if (!isConnected) return@withContext Result.Failed("not connected")
        val folder = PomegradeFolder.consoleFolder(context, PunicaLayout.SWITCH, "${PunicaLayout.CORE}/log")
            ?: return@withContext Result.Failed("no Punica folder")
        folder.mkdirs()
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(Date())
        val steps = runCatching { command.steps(context.packageName, stamp) }
            .getOrElse { return@withContext Result.Failed(it.message ?: "bad argument") }
        if (command.isDelayed) {
            val appContext = context.applicationContext
            scope.launch {
                delay(delaySeconds * 1000L)
                runSteps(steps, folder, stamp, appContext)
            }
            return@withContext Result.Started(folder, delaySeconds)
        }
        runSteps(steps, folder, stamp, context.applicationContext)?.let { return@withContext Result.Failed(it) }
        Result.Written(File(folder, steps.last().fileName))
    }

    /**
     * Runs [steps] in order, each output into its file; the first failure stops them and is returned (and written).
     * The connection is made again first when it was lost: a delayed command runs while Punica is in a game, and by
     * then adbd had closed it (owner's full analysis, 3 November: "gpu-before.txt: connect() must be called first").
     * The pairing outlives a lost connection while Punica's process runs, so connecting again needs no new code.
     */
    private fun runSteps(steps: List<DiagnosticCommand.Step>, folder: File, stamp: String, context: Context): String? {
        for (step in steps) {
            val failure = runCatching {
                if (!isConnected && !WirelessDebugConnection.instance.connectTls(context, 5_000)) {
                    error("wireless debugging connection lost and could not be made again (is wireless debugging still on?)")
                }
                // the shell service with the command as is (LocalServices.SHELL would quote it, and refuses double quotes)
                // written as it is read: a minute's trace is about 150 MB, too much to hold in Punica's memory first
                WirelessDebugConnection.instance.openStream("shell:${step.shell}").use { stream ->
                    File(folder, step.fileName).outputStream().use { file -> copyUntilClosed(stream, file) }
                }
            }.exceptionOrNull() ?: continue
            val reason = "${step.fileName}: ${failure.message ?: failure.javaClass.simpleName}"
            runCatching { File(folder, "punica-$stamp-error.txt").writeText(reason) }
            return reason
        }
        return null
    }
}
