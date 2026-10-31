package com.ps5ona.app.diagnostics

/** JNI bindings for the native logger and crash handler (see cpp/diagnostics). */
internal object NativeDiagnostics {
    external fun nativeInit(logDir: String, crashDir: String, header: String)
    external fun nativeLog(level: Int, tag: String, message: String)
    external fun nativeFlush()
    external fun nativeTestCrash()
}
