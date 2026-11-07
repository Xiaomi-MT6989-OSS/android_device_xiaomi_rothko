// SPDX-License-Identifier: Apache-2.0
package org.lineageos.rothko.dolby

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.os.Bundle
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import kotlin.math.abs
import kotlin.math.ln
import kotlin.math.roundToInt

/** Editable control curve, not a measurement of the speaker's frequency response. */
class EqualizerCurveView(context: Context, attrs: AttributeSet? = null) : View(context, attrs) {
    var onSelection: ((Int) -> Unit)? = null
    var onEdit: ((Int, Int) -> Unit)? = null
    private var values = IntArray(20)
    private var selected = 0
    private var baseline = IntArray(20)
    private var edited = false
    private val density = resources.displayMetrics.density
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val plot = RectF()
    private val accent: Int
    private val foreground: Int
    private val surface: Int
    init {
        fun color(attr: Int): Int {
            val array = context.obtainStyledAttributes(intArrayOf(attr))
            return try { array.getColor(0,Color.GRAY) } finally { array.recycle() }
        }
        accent = color(android.R.attr.colorAccent)
        foreground = color(android.R.attr.textColorPrimary)
        surface = color(android.R.attr.colorBackground)
        isFocusable = true
        isClickable = true
        importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
    }
    fun bind(curve: IntArray, band: Int) {
        values = curve.copyOf()
        selected = band.coerceIn(values.indices)
        describe()
        invalidate()
    }
    private fun describe() {
        contentDescription = context.getString(R.string.dolby_band_value,
            DolbyCore.frequencies[selected], values[selected] * 0.5)
    }
    private fun x(index: Int): Float {
        val frequencies = DolbyCore.frequencies
        val fraction = ln(frequencies[index].toDouble() / frequencies.first()) /
            ln(frequencies.last().toDouble() / frequencies.first())
        return plot.left + (fraction * plot.width()).toFloat()
    }
    private fun y(step: Int) = plot.centerY() - step / 12f * plot.height() / 2
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val d = density
        plot.set(40*d, 54*d, width-18*d, height-32*d)
        if (plot.width() <= 0 || plot.height() <= 0) return
        paint.shader = null
        paint.color = surface
        paint.style = Paint.Style.FILL
        canvas.drawRoundRect(RectF(0f, 0f, width.toFloat(), height.toFloat()), 24*d, 24*d, paint)
        paint.textSize = 13 * resources.displayMetrics.scaledDensity
        paint.color = foreground
        paint.alpha = 190
        canvas.drawText(context.getString(R.string.dolby_band_value,
            DolbyCore.frequencies[selected], values[selected] * 0.5), 16*d, 28*d, paint)
        for (step in intArrayOf(-12,-6,0,6,12)) {
            paint.color = foreground
            paint.alpha = if (step == 0) 65 else 25
            paint.strokeWidth = d
            canvas.drawLine(plot.left, y(step), plot.right, y(step), paint)
            paint.alpha = 150
            canvas.drawText((step/2).toString(), 12*d, y(step)+4*d, paint)
        }
        val curve = Path().apply {
            moveTo(x(0), y(values[0]))
            for (i in 1 until values.size) lineTo(x(i), y(values[i]))
        }
        val fill = Path(curve).apply {
            lineTo(plot.right,plot.bottom); lineTo(plot.left,plot.bottom); close()
        }
        paint.style = Paint.Style.FILL
        paint.alpha = 255
        val translucent = (accent and 0x00ffffff) or (65 shl 24)
        paint.shader = LinearGradient(0f,plot.top,0f,plot.bottom,translucent,
            Color.TRANSPARENT, Shader.TileMode.CLAMP)
        canvas.drawPath(fill,paint)
        paint.shader = null
        paint.color = accent
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = 2.5f*d
        paint.strokeJoin = Paint.Join.ROUND
        canvas.drawPath(curve,paint)
        paint.style = Paint.Style.FILL
        for (i in values.indices) {
            paint.alpha = if (i == selected) 255 else 160
            canvas.drawCircle(x(i),y(values[i]),if(i==selected) 7*d else 2.5f*d,paint)
        }
        paint.color = foreground
        paint.alpha = 160
        paint.textSize = 11 * resources.displayMetrics.scaledDensity
        canvas.drawText("47 Hz",plot.left, height-8*d,paint)
        canvas.drawText("1 kHz",plot.left+plot.width()*(ln(1000.0/47)/ln(19688.0/47)).toFloat(),height-8*d,paint)
        paint.textAlign = Paint.Align.RIGHT
        canvas.drawText("20 kHz",plot.right,height-8*d,paint)
        paint.textAlign = Paint.Align.LEFT
        paint.alpha = 255
    }
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEnabled || plot.width() <= 0) return false
        when(event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (!plot.contains(event.x,event.y)) return false
                parent.requestDisallowInterceptTouchEvent(true)
                baseline = values.copyOf()
                selected = values.indices.minByOrNull { abs(x(it)-event.x) } ?: 0
                edited = false
                onSelection?.invoke(selected)
                requestFocus()
                invalidate()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                val value = ((plot.centerY()-event.y) * 24 / plot.height()).roundToInt().coerceIn(-12,12)
                edited = edited || value != values[selected]
                values[selected] = value
                describe()
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP -> {
                parent.requestDisallowInterceptTouchEvent(false)
                if (edited) onEdit?.invoke(selected,values[selected]) else performClick()
                describe()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                parent.requestDisallowInterceptTouchEvent(false)
                values = baseline.copyOf()
                describe(); invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }
    override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo) {
        super.onInitializeAccessibilityNodeInfo(info)
        info.className = "android.widget.SeekBar"
        info.rangeInfo = AccessibilityNodeInfo.RangeInfo.obtain(
            AccessibilityNodeInfo.RangeInfo.RANGE_TYPE_INT,0f,values.lastIndex.toFloat(),selected.toFloat())
        if (isEnabled) {
            info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_PROGRESS)
            if (selected < values.lastIndex) info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD)
            if (selected > 0) info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
        }
    }
    override fun performAccessibilityAction(action: Int,args: Bundle?): Boolean {
        if (!isEnabled) return super.performAccessibilityAction(action,args)
        val band = when(action) {
            AccessibilityNodeInfo.ACTION_SCROLL_FORWARD -> selected + 1
            AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD -> selected - 1
            android.R.id.accessibilityActionSetProgress ->
                args?.getFloat(AccessibilityNodeInfo.ACTION_ARGUMENT_PROGRESS_VALUE)?.roundToInt()
                    ?: return false
            else -> return super.performAccessibilityAction(action,args)
        }
        selected = band.coerceIn(values.indices)
        onSelection?.invoke(selected)
        describe(); invalidate(); sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SELECTED)
        return true
    }
    override fun performClick(): Boolean { super.performClick(); return true }
    override fun onKeyDown(keyCode: Int,event: KeyEvent): Boolean {
        if (!isEnabled) return super.onKeyDown(keyCode,event)
        when(keyCode) {
            KeyEvent.KEYCODE_DPAD_LEFT -> selected = (selected-1).coerceAtLeast(0)
            KeyEvent.KEYCODE_DPAD_RIGHT -> selected = (selected+1).coerceAtMost(values.lastIndex)
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN -> {
                val delta = if(keyCode==KeyEvent.KEYCODE_DPAD_UP) 1 else -1
                onEdit?.invoke(selected,(values[selected]+delta).coerceIn(-12,12))
                return true
            }
            else -> return super.onKeyDown(keyCode,event)
        }
        onSelection?.invoke(selected)
        describe(); invalidate(); announceForAccessibility(contentDescription)
        return true
    }
}
