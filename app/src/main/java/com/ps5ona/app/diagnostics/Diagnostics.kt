package com.ps5ona.app.diagnostics

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.content.FileProvider
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Central logging and crash-report support.
 *
 * Layout inside the app's private storage:
 *   files/logs/native.log(.1)   rolling log written by the native logger (Kotlin logs go here too)
 *   files/crashes/*.txt         one report per native signal or uncaught Java/Kotlin exception
 *   cache/exports/*.zip         bundles produced by [exportBundle] for the share sheet
 */
object Diagnostics {
    private const val TAG = "PS5onA"
    private const val PREFS = "ps5ona_diagnostics"
    private const val KEY_LAST_SEEN_CRASH = "last_seen_crash_ms"
    private const val MAX_CRASH_REPORTS = 20
    private const val LOGCAT_LINES = "5000"

    private lateinit var appContext: Context
    private lateinit var logDir: File
    private lateinit var crashDir: File

    @Volatile
    var nativeReady = false
        private set

    fun init(context: Context) {
        appContext = context.applicationContext
        logDir = File(appContext.filesDir, "logs").apply { mkdirs() }
        crashDir = File(appContext.filesDir, "crashes").apply { mkdirs() }
        pruneOldCrashReports()
        installJavaCrashHandler()

        nativeReady = runCatching {
            System.loadLibrary("ps5ona")
            NativeDiagnostics.nativeInit(logDir.absolutePath, crashDir.absolutePath, deviceSummary())
            true
        }.getOrElse {
            Log.e(TAG, "Native library failed to load; native logging disabled", it)
            false
        }

        i("Diagnostics initialized (app ${appVersion()}, native=${nativeReady})")
    }

    // ---- logging ----

    fun d(message: String) = write(Log.DEBUG, message)
    fun i(message: String) = write(Log.INFO, message)
    fun w(message: String) = write(Log.WARN, message)
    fun e(message: String, error: Throwable? = null) =
        write(Log.ERROR, if (error == null) message else "$message\n${stackTrace(error)}")

    private fun write(level: Int, message: String) {
        if (nativeReady) {
            // Native side mirrors to logcat and appends to native.log.
            NativeDiagnostics.nativeLog(level, TAG, message)
        } else {
            Log.println(level, TAG, message)
        }
    }

    // ---- crash reports ----

    fun crashReports(): List<File> =
        crashDir.listFiles { f -> f.isFile && f.name.endsWith(".txt") }
            ?.sortedByDescending { it.lastModified() }
            .orEmpty()

    /** Crash reports written since the user last saw or dismissed the crash prompt. */
    fun unseenCrashReports(): List<File> {
        val lastSeen = prefs().getLong(KEY_LAST_SEEN_CRASH, 0L)
        return crashReports().filter { it.lastModified() > lastSeen }
    }

    fun markCrashReportsSeen() {
        val newest = crashReports().maxOfOrNull { it.lastModified() } ?: System.currentTimeMillis()
        prefs().edit().putLong(KEY_LAST_SEEN_CRASH, newest).apply()
    }

    fun clearAll() {
        crashDir.listFiles()?.forEach { it.delete() }
        logDir.listFiles()?.forEach { if (it.name != "native.log") it.delete() }
        // native.log stays open in the native logger; truncate instead of deleting.
        File(logDir, "native.log").takeIf { it.exists() }?.writeText("")
        File(appContext.cacheDir, "exports").deleteRecursively()
        markCrashReportsSeen()
        i("Logs and crash reports cleared by user")
    }

    fun triggerTestCrash(native: Boolean) {
        if (native && nativeReady) {
            NativeDiagnostics.nativeTestCrash()
        } else {
            throw IllegalStateException("PS5onA test crash (Kotlin)")
        }
    }

    private fun installJavaCrashHandler() {
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            runCatching {
                val report = buildString {
                    appendLine("PS5onA Java/Kotlin crash report")
                    appendLine("===============================")
                    appendLine("Time: ${timestamp()}")
                    appendLine("Thread: ${thread.name} (id ${thread.id})")
                    appendLine()
                    appendLine(deviceSummary())
                    appendLine()
                    appendLine("Exception:")
                    append(stackTrace(error))
                }
                File(crashDir, "java-crash-${System.currentTimeMillis() / 1000}.txt").writeText(report)
                write(Log.ERROR, "Uncaught exception on ${thread.name}: $error")
                if (nativeReady) NativeDiagnostics.nativeFlush()
            }
            previous?.uncaughtException(thread, error)
        }
    }

    private fun pruneOldCrashReports() {
        crashReports().drop(MAX_CRASH_REPORTS).forEach { it.delete() }
    }

    // ---- export ----

    /**
     * Builds a zip with device info, settings, logs, crash reports and recent
     * logcat output. Does disk and process I/O, so call it off the main thread.
     */
    fun exportBundle(settings: Map<String, *>): File {
        if (nativeReady) NativeDiagnostics.nativeFlush()
        val exportDir = File(appContext.cacheDir, "exports").apply {
            deleteRecursively()
            mkdirs()
        }
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val zip = File(exportDir, "PS5onA-diagnostics-$stamp.zip")

        ZipOutputStream(zip.outputStream().buffered()).use { out ->
            fun addText(name: String, text: String) {
                out.putNextEntry(ZipEntry(name))
                out.write(text.toByteArray())
                out.closeEntry()
            }
            fun addFile(name: String, file: File) {
                out.putNextEntry(ZipEntry(name))
                file.inputStream().use { it.copyTo(out) }
                out.closeEntry()
            }

            addText("device.txt", deviceSummary() + "\nExported: ${timestamp()}\n")
            addText("settings.txt", settings.toSortedMap().entries.joinToString("\n") { "${it.key} = ${it.value}" })
            logDir.listFiles()?.filter { it.isFile }?.forEach { addFile("logs/${it.name}", it) }
            crashReports().forEach { addFile("crashes/${it.name}", it) }
            addText("logcat.txt", captureLogcat())
        }
        i("Exported diagnostics bundle ${zip.name} (${zip.length()} bytes)")
        return zip
    }

    fun shareIntent(bundle: File): Intent {
        val uri = FileProvider.getUriForFile(appContext, "${appContext.packageName}.diagnostics", bundle)
        val send = Intent(Intent.ACTION_SEND).apply {
            type = "application/zip"
            putExtra(Intent.EXTRA_STREAM, uri)
            putExtra(Intent.EXTRA_SUBJECT, "PS5onA diagnostics ${appVersion()}")
            putExtra(Intent.EXTRA_TEXT, "PS5onA diagnostics bundle (logs, crash reports, device info).")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        return Intent.createChooser(send, "Export diagnostics")
    }

    private fun captureLogcat(): String = runCatching {
        // Apps may read their own process's log buffer without extra permissions.
        val process = ProcessBuilder(
            "logcat", "-d", "-v", "threadtime", "-t", LOGCAT_LINES, "--pid", android.os.Process.myPid().toString()
        ).redirectErrorStream(true).start()
        val text = process.inputStream.bufferedReader().use { it.readText() }
        process.waitFor()
        text
    }.getOrElse { "logcat capture failed: $it" }

    // ---- helpers ----

    fun deviceSummary(): String = buildString {
        appendLine("App: PS5onA ${appVersion()}")
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
        appendLine("Hardware: ${Build.HARDWARE}, board ${Build.BOARD}")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            appendLine("SoC: ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
        }
        appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}), security patch ${Build.VERSION.SECURITY_PATCH}")
        appendLine("Build: ${Build.FINGERPRINT}")
        append("ABIs: ${Build.SUPPORTED_ABIS.joinToString()}")
    }

    private fun appVersion(): String = runCatching {
        val info = appContext.packageManager.getPackageInfo(appContext.packageName, 0)
        "${info.versionName} (${info.longVersionCode})"
    }.getOrDefault("unknown")

    private fun prefs() = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    private fun timestamp() = SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z", Locale.US).format(Date())

    private fun stackTrace(error: Throwable) = StringWriter().also { error.printStackTrace(PrintWriter(it)) }.toString()
}
