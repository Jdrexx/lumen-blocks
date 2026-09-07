package com.jdrexx.lumenblocks

import android.content.Intent
import android.media.AudioManager
import android.media.ToneGenerator
import android.net.Uri
import android.view.HapticFeedbackConstants
import android.view.View
import androidx.annotation.Keep
import com.google.android.gms.games.PlayGames
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
        decorView.systemUiVisibility =
            (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN)
    }

    @Keep
    fun gameFeedback(kind: Int, soundEnabled: Boolean, hapticsEnabled: Boolean) {
        runOnUiThread {
            if (hapticsEnabled) {
                val feedback =
                    if (kind >= 2) {
                        HapticFeedbackConstants.CONFIRM
                    } else {
                        HapticFeedbackConstants.KEYBOARD_TAP
                    }
                window.decorView.performHapticFeedback(feedback)
            }
            if (soundEnabled) {
                val tone =
                    when (kind) {
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
            val shareIntent =
                Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(
                        Intent.EXTRA_TEXT,
                        "I scored $score in ${modeNames.getOrElse(mode) { "Classic" }} mode " +
                            "in Lumen Blocks. Can you restore more light?",
                    )
                }
            startActivity(Intent.createChooser(shareIntent, "Share Lumen Blocks score"))
        }
    }

    @Keep
    fun syncPlayGames(
        score: Int,
        mode: Int,
        totalLines: Int,
        totalPieces: Int,
        maxCombo: Int,
        lumen: Int,
    ) {
        if (!BuildConfig.PLAY_GAMES_ENABLED) return
        runOnUiThread {
            val signInClient = PlayGames.getGamesSignInClient(this)
            signInClient.isAuthenticated.addOnSuccessListener { result ->
                if (!result.isAuthenticated) return@addOnSuccessListener

                val leaderboardIds =
                    intArrayOf(
                        R.string.leaderboard_classic,
                        R.string.leaderboard_journey,
                        R.string.leaderboard_daily,
                        R.string.leaderboard_zen,
                        R.string.leaderboard_rush,
                    )
                PlayGames.getLeaderboardsClient(this)
                    .submitScore(
                        getString(leaderboardIds.getOrElse(mode) { leaderboardIds[0] }),
                        score.toLong(),
                    )

                val achievements = PlayGames.getAchievementsClient(this)
                if (totalLines >= 1)
                    achievements.unlock(getString(R.string.achievement_first_light))
                if (totalLines >= 50)
                    achievements.unlock(getString(R.string.achievement_line_keeper))
                if (totalPieces >= 250)
                    achievements.unlock(getString(R.string.achievement_architect))
                if (maxCombo >= 5) achievements.unlock(getString(R.string.achievement_combo_five))
                if (lumen >= 1000) achievements.unlock(getString(R.string.achievement_bright_world))
                if (score >= 5000 && mode == 0) {
                    achievements.unlock(getString(R.string.achievement_master))
                }
            }
        }
    }

    @Keep
    fun showPlayGames(view: Int) {
        if (!BuildConfig.PLAY_GAMES_ENABLED) return
        runOnUiThread {
            val signInClient = PlayGames.getGamesSignInClient(this)
            signInClient.isAuthenticated.addOnSuccessListener { result ->
                if (!result.isAuthenticated) {
                    signInClient.signIn()
                    return@addOnSuccessListener
                }
                val intentTask =
                    if (view == 0) {
                        PlayGames.getLeaderboardsClient(this).allLeaderboardsIntent
                    } else {
                        PlayGames.getAchievementsClient(this).achievementsIntent
                    }
                intentTask.addOnSuccessListener(::startActivity)
            }
        }
    }

    @Keep
    fun openPrivacyPolicy() {
        runOnUiThread {
            startActivity(
                Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse("https://github.com/Jdrexx/lumen-blocks/blob/main/PRIVACY_POLICY.md"),
                )
            )
        }
    }

    override fun onDestroy() {
        toneGenerator.release()
        super.onDestroy()
    }
}