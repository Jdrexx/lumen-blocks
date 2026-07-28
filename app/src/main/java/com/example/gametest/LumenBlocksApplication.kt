package com.example.gametest

import android.app.Application
import com.google.android.gms.games.PlayGamesSdk

class LumenBlocksApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        if (BuildConfig.PLAY_GAMES_ENABLED) {
            PlayGamesSdk.initialize(this)
        }
    }
}
