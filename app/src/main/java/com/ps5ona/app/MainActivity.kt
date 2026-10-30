package com.ps5ona.app

import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
    companion object { init { System.loadLibrary("ps5ona") } }

    private external fun nativeStatus(): String
    private val prefs by lazy { getSharedPreferences("ps5ona", MODE_PRIVATE) }

    private val pickGame = registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let {
            contentResolver.takePersistableUriPermission(it, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            prefs.edit().putString("game_uri", it.toString()).apply()
            recreate()
        }
    }

    private val pickTurnip = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let {
            contentResolver.takePersistableUriPermission(it, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            prefs.edit().putString("turnip_uri", it.toString()).putString("turnip_name", displayName(it)).apply()
        }
    }

    private val pickLossless = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let {
            contentResolver.takePersistableUriPermission(it, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            prefs.edit().putString("lossless_uri", it.toString()).putString("lossless_name", displayName(it)).apply()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.statusBarColor = Color.BLACK
        window.navigationBarColor = Color.BLACK
        setContentView(buildHome())
    }

    private fun buildHome(): View {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(40), dp(24), dp(40), dp(24))
            setBackgroundColor(Color.rgb(9, 11, 15))
        }

        val top = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        top.addView(TextView(this).apply {
            text = "PS5onA"
            setTextColor(Color.WHITE)
            textSize = 24f
            setTypeface(typeface, Typeface.BOLD)
        }, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        top.addView(chip("+ Game") { pickGame.launch(null) })
        top.addView(space(10))
        top.addView(chip("Settings") { showSettings() })
        root.addView(top)

        root.addView(TextView(this).apply {
            text = "Library"
            setTextColor(Color.rgb(145, 150, 160))
            textSize = 13f
            setPadding(0, dp(28), 0, dp(14))
        })

        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        if (prefs.getString("game_uri", null) == null) {
            row.addView(boxCard("Add a game", "Choose a game folder", true) { pickGame.launch(null) })
        } else {
            row.addView(boxCard("Selected game", "Box art / metadata scan pending", false) {})
            row.addView(space(20))
            row.addView(boxCard("Add another", "Build your library", true) { pickGame.launch(null) })
        }

        root.addView(HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            addView(row)
        }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))

        root.addView(TextView(this).apply {
            text = runCatching { nativeStatus().lineSequence().first() }.getOrDefault("Native bootstrap unavailable")
            setTextColor(Color.rgb(80, 85, 95))
            textSize = 10f
            gravity = Gravity.END
        })
        return root
    }

    private fun showSettings() {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(24), 0, dp(24), dp(8))
        }

        panel.addView(header("Graphics"))
        val scaleLabel = TextView(this)
        val current = prefs.getInt("resolution_scale", 100)
        scaleLabel.text = "Internal resolution: $current%"
        panel.addView(scaleLabel)
        panel.addView(SeekBar(this).apply {
            max = 175
            progress = current - 25
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(bar: SeekBar?, progress: Int, fromUser: Boolean) {
                    val value = progress + 25
                    scaleLabel.text = "Internal resolution: $value%"
                    if (fromUser) prefs.edit().putInt("resolution_scale", value).apply()
                }
                override fun onStartTrackingTouch(bar: SeekBar?) = Unit
                override fun onStopTrackingTouch(bar: SeekBar?) = Unit
            })
        })

        panel.addView(header("Vulkan driver"))
        panel.addView(TextView(this).apply {
            text = prefs.getString("turnip_name", null) ?: "System driver"
        })
        panel.addView(Button(this).apply {
            text = "Select Turnip driver package"
            setOnClickListener { pickTurnip.launch(arrayOf("application/zip", "application/octet-stream")) }
        })
        panel.addView(Button(this).apply {
            text = "Use system driver"
            setOnClickListener { prefs.edit().remove("turnip_uri").remove("turnip_name").apply() }
        })

        panel.addView(header("Frame generation"))
        panel.addView(CheckBox(this).apply {
            text = "Enable frame generation"
            isChecked = prefs.getBoolean("framegen_enabled", false)
            setOnCheckedChangeListener { _, enabled -> prefs.edit().putBoolean("framegen_enabled", enabled).apply() }
        })
        panel.addView(TextView(this).apply {
            text = prefs.getString("lossless_name", null) ?: "No Lossless.dll imported"
        })
        panel.addView(Button(this).apply {
            text = "Import Lossless.dll"
            setOnClickListener { pickLossless.launch(arrayOf("application/octet-stream", "*/*")) }
        })
        panel.addView(Spinner(this).apply {
            val options = arrayOf("2x frame generation", "3x frame generation", "4x frame generation")
            adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, options)
            setSelection(prefs.getInt("framegen_multiplier", 0))
            onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                    prefs.edit().putInt("framegen_multiplier", position).apply()
                }
                override fun onNothingSelected(parent: AdapterView<*>?) = Unit
            }
        })
        panel.addView(TextView(this).apply {
            text = "PS5onA will only import a Lossless.dll you provide. The proprietary DLL is not bundled or downloaded by the app."
            textSize = 11f
            setTextColor(Color.GRAY)
            setPadding(0, dp(8), 0, 0)
        })

        AlertDialog.Builder(this)
            .setTitle("Settings")
            .setView(ScrollView(this).apply { addView(panel) })
            .setPositiveButton("Done", null)
            .show()
    }

    private fun boxCard(title: String, subtitle: String, add: Boolean, action: () -> Unit) =
        LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(dp(16), dp(16), dp(16), dp(16))
            background = rounded(if (add) Color.rgb(28, 31, 38) else Color.rgb(19, 22, 28), 18)
            setOnClickListener { action() }
            addView(TextView(this@MainActivity).apply {
                text = if (add) "+" else "PS5"
                textSize = if (add) 42f else 32f
                setTextColor(Color.WHITE)
                gravity = Gravity.CENTER
            }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
            addView(TextView(this@MainActivity).apply {
                text = title
                textSize = 16f
                setTextColor(Color.WHITE)
                setTypeface(typeface, Typeface.BOLD)
            })
            addView(TextView(this@MainActivity).apply {
                text = subtitle
                textSize = 12f
                setTextColor(Color.rgb(130, 136, 146))
            })
            layoutParams = LinearLayout.LayoutParams(dp(240), dp(350))
        }

    private fun chip(text: String, action: () -> Unit) = TextView(this).apply {
        this.text = text
        setTextColor(Color.WHITE)
        textSize = 14f
        setPadding(dp(18), dp(10), dp(18), dp(10))
        background = rounded(Color.rgb(27, 30, 37), 18)
        setOnClickListener { action() }
    }

    private fun header(text: String) = TextView(this).apply {
        this.text = text
        textSize = 18f
        setTypeface(typeface, Typeface.BOLD)
        setPadding(0, dp(18), 0, dp(8))
    }

    private fun displayName(uri: Uri): String {
        var name = uri.lastPathSegment ?: "Selected file"
        contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use {
            if (it.moveToFirst()) name = it.getString(0)
        }
        return name
    }

    private fun rounded(color: Int, radiusDp: Int) = GradientDrawable().apply {
        setColor(color)
        cornerRadius = dp(radiusDp).toFloat()
    }

    private fun space(widthDp: Int) = Space(this).apply {
        layoutParams = LinearLayout.LayoutParams(dp(widthDp), 1)
    }

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()
}
