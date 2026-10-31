package com.ps5ona.app

import android.app.Application
import com.ps5ona.app.diagnostics.Diagnostics

class PS5onAApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Install logging and crash handlers before any other code runs.
        Diagnostics.init(this)
    }
}
