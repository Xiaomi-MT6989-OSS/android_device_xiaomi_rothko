/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.rothko.brightness;

/** Deterministic stock table/state semantics, independent of Android and IO. */
final class StockBrightnessState {
    private final StockBrightnessConfig mConfig;
    private float mBoard = Float.NaN;
    private float mNtc = Float.NaN;

    StockBrightnessState(StockBrightnessConfig config) { mConfig = config; }

    void invalidate() { mBoard = Float.NaN; mNtc = Float.NaN; }

    void update(float board, float ntc) {
        if (!sameTemperature(mBoard, board, false)) mBoard = board;
        if (!sameTemperature(mNtc, ntc, true)) mNtc = ntc;
    }

    // Exact packaged ThermalBrightnessController.asSameTemperature default gap semantics.
    // This is magnitude-based stock deadband, not a time-constant filter.
    static boolean sameTemperature(float a, float b, boolean ntc) {
        if (a == b || (Float.isNaN(a) && Float.isNaN(b))) return true;
        if (!Float.isFinite(a) || !Float.isFinite(b)) return false;
        if (Math.abs((int) a - (int) b) < 1) return true;
        return Math.abs(a - b) < (ntc ? 1f : 0.5f);
    }

    float cap(boolean automatic, float ambientLux, int condition, float safetyNits,
            boolean ntcEnabled) {
        // Out-of-range/missing input cannot turn a thermal bound into an unlimited value.
        float failureCap = Float.isFinite(safetyNits) && safetyNits > 0
                ? Math.min(mConfig.minimumCapNits, safetyNits) : mConfig.minimumCapNits;
        if (!Float.isFinite(mBoard) || mBoard >= mConfig.maximumBoardTemperature
                || !Float.isFinite(safetyNits) || safetyNits < 0
                || (ntcEnabled && (!Float.isFinite(mNtc)
                        || !Float.isFinite(mConfig.maximumNtcTemperature)
                        || mNtc >= mConfig.maximumNtcTemperature))) return failureCap;
        float lux = automatic ? ambientLux : 6000f; // Original manual-mode thermal band input.
        float cap = Float.isFinite(lux) && lux >= 0
                ? mConfig.cap(condition, lux, mBoard)
                : mConfig.capWithoutLux(condition, mBoard);
        if (ntcEnabled) {
            // NTC is an additional cap, never a substitute for the board temperature.
            cap = Math.min(cap, mConfig.ntcCap(Float.isFinite(lux) ? lux : 0, mNtc));
        }
        // Stock zero means no separate daemon safety bound; it does not mean zero brightness.
        if (safetyNits > 0) cap = Math.min(cap, safetyNits);
        return cap;
    }

    float appliedBoardTemperature() { return mBoard; }
}
