// SPDX-License-Identifier: Apache-2.0
package org.lineageos.rothko.dolby

import android.content.Context
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder

class EqualizerCurvePreference(context: Context) : Preference(context) {
    var curve = IntArray(20)
    var band = 0
    var onSelection: ((Int) -> Unit)? = null
    var onEdit: ((Int,Int) -> Unit)? = null
    init {
        layoutResource = R.layout.equalizer_curve
        isPersistent = false
        isSelectable = false
    }
    fun update(values: IntArray, selected: Int) {
        curve = values.copyOf()
        band = selected
        notifyChanged()
    }
    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        (holder.findViewById(R.id.eq_curve) as EqualizerCurveView).apply {
            bind(curve,band)
            isEnabled = this@EqualizerCurvePreference.isEnabled
            onSelection = this@EqualizerCurvePreference.onSelection
            onEdit = this@EqualizerCurvePreference.onEdit
        }
    }
}
