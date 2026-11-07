// SPDX-License-Identifier: Apache-2.0
package org.lineageos.rothko.dolby

import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log

class DolbyApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Persistent application owns the global AudioEffect across Settings exits.
        DolbyCore.worker.execute { restore(this) }
    }
    companion object {
        fun restore(context: Context) {
            runCatching { DolbyCore.restore(context) }
                .onFailure { Log.w("RothkoDolby", "Could not restore DAP", it) }
        }
    }
}
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_LOCKED_BOOT_COMPLETED) return
        val pending = goAsync()
        DolbyCore.worker.execute {
            try { DolbyApplication.restore(context) } finally { pending.finish() }
        }
    }
}
