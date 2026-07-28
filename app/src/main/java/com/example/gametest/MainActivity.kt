package com.example.gametest

import android.media.AudioManager
import android.media.ToneGenerator
import android.content.Intent
import android.view.View
import android.view.HapticFeedbackConstants
import androidx.annotation.Keep
import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {
    private val toneGenerator by lazy { ToneGenerator(AudioManager.STREAM_MUSIC, 35) }

    companion object {
        init {
            System.loadLibrary("gametest")
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUi()
        }
    }

    private fun hideSystemUi() {
        val decorView = window.decorView
        decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN)
    }

    @Keep
    fun gameFeedback(kind: Int, soundEnabled: Boolean, hapticsEnabled: Boolean) {
        runOnUiThread {
            if (hapticsEnabled) {
                val feedback = if (kind >= 2) {
                    HapticFeedbackConstants.CONFIRM
                } else {
                    HapticFeedbackConstants.KEYBOARD_TAP
                }
                window.decorView.performHapticFeedback(feedback)
            }
            if (soundEnabled) {
                val tone = when (kind) {
                    0 -> ToneGenerator.TONE_PROP_NACK
                    2 -> ToneGenerator.TONE_PROP_ACK
                    else -> ToneGenerator.TONE_PROP_BEEP
                }
                toneGenerator.startTone(tone, if (kind >= 2) 90 else 35)
            }
        }
    }

    @Keep
    fun shareScore(score: Int, mode: Int) {
        val modeNames = arrayOf("Classic", "Journey", "Daily", "Zen", "Rush")
        runOnUiThread {
            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(
                    Intent.EXTRA_TEXT,
                    "I scored $score in ${modeNames.getOrElse(mode) { "Classic" }} mode " +
                            "in Lumen Blocks. Can you restore more light?"
                )
            }
            startActivity(Intent.createChooser(shareIntent, "Share Lumen Blocks score"))
        }
    }

    override fun onDestroy() {
        toneGenerator.release()
        super.onDestroy()
    }
}
