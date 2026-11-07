// SPDX-License-Identifier: Apache-2.0
package org.lineageos.rothko.dolby

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Toast
import android.util.Log
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.SeekBarPreference
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity
import com.android.settingslib.widget.MainSwitchPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment

class DolbyActivity : CollapsingToolbarBaseActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) supportFragmentManager.beginTransaction()
            .replace(com.android.settingslib.collapsingtoolbar.R.id.content_frame, DolbyFragment()).commit()
    }
}
class DolbyFragment : SettingsBasePreferenceFragment() {
    private val main = Handler(Looper.getMainLooper())
    private lateinit var enabled: MainSwitchPreference
    private lateinit var profile: ListPreference
    private lateinit var status: Preference
    private lateinit var equalizer: PreferenceCategory
    private var curve = IntArray(DolbyCore.frequencies.size)
    private lateinit var curvePreference: EqualizerCurvePreference
    private lateinit var selectedGain: SeekBarPreference
    private var selectedBand = 0
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.dolby_settings, rootKey)
        enabled = findPreference("enabled")!!
        profile = findPreference("profile")!!
        status = findPreference("status")!!
        equalizer = findPreference("equalizer")!!
        equalizer.isVisible = false
        curvePreference = EqualizerCurvePreference(requireContext()).apply {
            key = "eq_curve"
            onSelection = {
                selectedBand = it
                curvePreference.band = it
                updateSelectedGain()
            }
            onEdit = { index, value -> editBand(index, value) }
        }
        equalizer.addPreference(curvePreference)
        selectedGain = SeekBarPreference(requireContext()).apply {
            key = "selected_band"
            isPersistent = false
            min = -12
            max = 12
            seekBarIncrement = 1
            updatesContinuously = false
            setOnPreferenceChangeListener { _, value ->
                editBand(selectedBand,value as Int)
                false
            }
        }
        equalizer.addPreference(selectedGain)
        equalizer.addPreference(Preference(requireContext()).apply {
            key = "reset_eq"
            setTitle(R.string.dolby_eq_reset)
            setOnPreferenceClickListener {
                val context = requireContext().applicationContext
                change { DolbyCore.equalizer(context, IntArray(DolbyCore.frequencies.size)) }
                true
            }
        })
        enabled.setOnPreferenceChangeListener { _, value ->
            val context = requireContext().applicationContext
            change { DolbyCore.enable(context, value as Boolean) }; false
        }
        profile.setOnPreferenceChangeListener { _, value ->
            val context = requireContext().applicationContext
            change { DolbyCore.profile(context, (value as String).toInt()) }; false
        }
        status.setOnPreferenceClickListener { refresh(); true }
    }
    override fun onResume() { super.onResume(); refresh() }
    private fun editBand(index: Int,value: Int) {
        selectedBand = index
        val context = requireContext().applicationContext
        val updated = curve.copyOf().also { it[index] = value }
        change { DolbyCore.equalizer(context,updated) }
    }
    private fun updateCurve() {
        curvePreference.update(curve,selectedBand)
        updateSelectedGain()
    }
    private fun updateSelectedGain() {
        selectedGain.title = "${DolbyCore.frequencies[selectedBand]} Hz"
        selectedGain.value = curve[selectedBand]
        selectedGain.summary = getString(R.string.dolby_gain,curve[selectedBand] * 0.5)
    }
    private fun change(action: () -> DolbyCore.State) { load(action) }
    private fun refresh() { load { null } }
    private fun load(action: () -> DolbyCore.State?) {
        enabled.isEnabled = false
        profile.isEnabled = false
        status.isEnabled = false
        equalizer.isEnabled = false
        val context = requireContext().applicationContext
        DolbyCore.worker.execute {
            val update = runCatching(action)
            // Render accepted changes immediately, as the stock controller does.
            // Lifecycle refreshes and explicit retry always query the actual engine.
            val result = runCatching {
                Pair(update.getOrNull() ?: DolbyCore.state(), DolbyCore.bands(context))
            }
            main.post {
                if (!isAdded) return@post
                update.exceptionOrNull()?.let {
                    Log.w("RothkoDolby", "DAP change failed", it)
                    Toast.makeText(requireContext(), R.string.dolby_error, Toast.LENGTH_LONG).show()
                }
                status.isEnabled = true
                result.onSuccess { (state, gains) ->
                    curve = gains
                    equalizer.isVisible = state.profile == 3
                    equalizer.isEnabled = state.enabled
                    updateCurve()
                    enabled.isEnabled = true
                    enabled.isChecked = state.enabled
                    profile.isEnabled = state.enabled
                    profile.value = state.profile.toString()
                    profile.summary = profile.entry
                    status.isVisible = false
                }.onFailure {
                    Log.w("RothkoDolby", "DAP state query failed", it)
                    status.isVisible = true
                }
            }
        }
    }
}
