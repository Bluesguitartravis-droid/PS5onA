package com.ps5ona.app

import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    companion object {
        init {
            System.loadLibrary("ps5ona")
        }
    }

    private external fun nativeStatus(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val status = runCatching { nativeStatus() }
            .getOrElse { "Native bootstrap failed: ${it.stackTraceToString()}" }

        val text = TextView(this).apply {
            text = "PS5onA\n\n$status"
            textSize = 18f
            setPadding(48, 48, 48, 48)
        }

        setContentView(text)
    }
}
