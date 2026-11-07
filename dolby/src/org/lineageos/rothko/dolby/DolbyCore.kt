// SPDX-License-Identifier: Apache-2.0
package org.lineageos.rothko.dolby

import android.content.Context
import android.media.audiofx.AudioEffect
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID
import java.util.concurrent.Executors

// Protocol observed in the original rothko MiSound DolbyAudioEffect wrapper.
// This is not Samsung's parameter-0/19 DAP protocol.
object DolbyCore {
    val worker = Executors.newSingleThreadExecutor()
    private const val PROFILE = 0x0a000000
    private const val PROFILE_COUNT = 0x03000000
    private val implementation = UUID.fromString("9d4921da-8225-4f29-aefa-39537a04bcaa")
    private var effect: AudioEffect? = null
    data class State(val enabled: Boolean, val profile: Int, val count: Int)

    private fun checkStatus(result: Int) { check(result >= 0) { "DAP status $result" } }
    private fun effect(): AudioEffect {
        effect?.let { if (it.hasControl()) return it else it.release() }
        effect = null
        return AudioEffect(AudioEffect.EFFECT_TYPE_NULL, implementation, 0, 0).also {
            if (!it.hasControl()) { it.release(); error("DAP control unavailable") }
            effect = it
        }
    }
    private fun words(vararg values: Int): ByteArray =
        ByteBuffer.allocate(values.size * 4).order(ByteOrder.LITTLE_ENDIAN).apply {
            values.forEach { putInt(it) }
        }.array()
    private fun query(e: AudioEffect, id: Int): Int {
        val output = words(id, 0, 0)
        val size = e.getParameter(id + 5, output)
        check(size >= 4) { "Incomplete DAP result: $size" }
        return ByteBuffer.wrap(output).order(ByteOrder.LITTLE_ENDIAN).int
    }
    @Synchronized fun state(): State {
        val e = effect()
        val count = query(e, PROFILE_COUNT)
        val profile = query(e, PROFILE)
        check(count > 0 && profile in 0 until count) { "Invalid DAP profile $profile / $count" }
        return State(query(e, 0) != 0 && e.enabled, profile, count)
    }
    @Synchronized fun enable(context: Context, value: Boolean): State {
        val previous = state()
        val e = effect()
        checkStatus(e.setParameter(5, words(0, 1, if (value) 1 else 0)))
        checkStatus(e.setEnabled(value))
        prefs(context).edit().putBoolean("enabled", value).apply()
        // Stock MiSound treats a successful SET_PARAM as acceptance. Its DMS
        // query path can still return the prior state while the update propagates.
        return previous.copy(enabled = value)
    }
    @Synchronized fun profile(context: Context, value: Int): State {
        val previous = state()
        val e = effect()
        check(value in 0..3 && value < query(e, PROFILE_COUNT))
        checkStatus(e.setParameter(5, words(PROFILE, 1, value)))
        prefs(context).edit().putInt("profile", value).apply()
        return previous.copy(profile = value)
    }
    // Frequencies and 1/16 dB protocol units from stock dax-default.xml / MiSound.
    val frequencies = intArrayOf(47, 141, 234, 328, 469, 656, 844, 1031, 1313, 1688,
        2250, 3000, 3750, 4688, 5813, 7125, 9000, 11250, 13875, 19688)
    private const val CUSTOM = 3
    private const val GEQ_ENABLE = 106
    private const val GEQ_GAINS = 110
    @Synchronized fun bands(context: Context): IntArray {
        val saved = prefs(context).getString("custom_eq_steps", null)
        return saved?.split(',')?.mapNotNull { it.toIntOrNull() }?.toIntArray()
            ?.takeIf { it.size == frequencies.size && it.all { step -> step in -12..12 } }
            ?: IntArray(frequencies.size)
    }
    private fun setProfileParameter(e: AudioEffect, id: Int, values: IntArray) {
        check(CUSTOM < query(e, PROFILE_COUNT))
        val status = e.setParameter(5, words(0x01000000, values.size + 1, CUSTOM, id, *values))
        check(status >= 0) { "DAP Custom parameter $id write returned $status" }
    }
    private fun writeBands(steps: IntArray) {
        require(steps.size == frequencies.size && steps.all { it in -12..12 })
        val e = effect()
        // UI half-dB steps -> Dolby 1/16 dB. Match stock mean removal: shape
        // changes do not add an unbounded broadband gain. Do not alter other modes.
        val gains = steps.map { it * 8 }.toIntArray()
        val mean = gains.sum() / gains.size
        setProfileParameter(e, GEQ_GAINS, gains.map { it - mean }.toIntArray())
        setProfileParameter(e, GEQ_ENABLE, intArrayOf(1))
    }
    @Synchronized fun equalizer(context: Context, steps: IntArray): State {
        val current = state()
        check(current.enabled && current.profile == CUSTOM) { "EQ needs enabled Custom profile: $current" }
        val previous = bands(context)
        try {
            writeBands(steps)
        } catch (failure: Exception) {
            runCatching { writeBands(previous) }
            throw failure
        }
        // Engine status acknowledges the write. No unsupported native readback
        // is invented here; the stock UI also persists the editable curve.
        prefs(context).edit().putString("custom_eq_steps", steps.joinToString(",")).apply()
        return current
    }
    private fun prefs(context: Context) = context.createDeviceProtectedStorageContext()
        .getSharedPreferences("dolby", Context.MODE_PRIVATE)
    @Synchronized fun restore(context: Context) {
        val prefs = prefs(context)
        if (prefs.contains("custom_eq_steps")) writeBands(bands(context))
        // A new installation retains the original effect's default state.
        if (prefs.contains("profile")) profile(context, prefs.getInt("profile", 0))
        if (prefs.contains("enabled")) enable(context, prefs.getBoolean("enabled", false))
    }
}
