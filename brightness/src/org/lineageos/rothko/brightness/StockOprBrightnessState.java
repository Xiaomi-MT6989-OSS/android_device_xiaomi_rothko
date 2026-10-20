/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.rothko.brightness;

/**
 * OS3.0.307.0 SDR OPR table and validated PQ histogram state, without Android or IO.
 *
 * The transport adapter must supply the
 * full PQ result (not DisplayFeature callback 95000), and begin a new session for every display,
 * HDR/SDR, statistics request or Binder connection generation. Calls are handler-confined.
 */
final class StockOprBrightnessState {
    // Packaged framework-ext base and MiuiFrameworkResOverlay; not a guessed HBM lux threshold.
    static final float AMBIENT_LUX_THRESHOLD = 20000f;
    static final float FALLBACK_NITS = 1000f;
    // Adaptation watchdog, not a stock delay or a smoothing/filtering time constant.
    static final long MAX_AGE_MILLIS = 2500;
    private static final int[] GRAY_THRESHOLDS = {
            120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230
    };
    private static final float[] NITS = {
            1600, 1550, 1500, 1450, 1400, 1350, 1300, 1250, 1200, 1150, 1100, 1050, 1000
    };

    enum Reception { INACTIVE, OLD_SESSION, OLD_REPLY, INVALID, BASELINE, DUPLICATE, RESET, VALID }

    private long mGeneration;
    private boolean mActive;
    private boolean mHaveFrame;
    private boolean mValid;
    private int mFrame;
    private int mGray = -1;
    private long mPixels;
    private long mSampleTime = -1;
    private long mNewestReadStart = -1;

    long beginSession() {
        mActive = false;
        mValid = false;
        if (mGeneration == Long.MAX_VALUE) throw new IllegalStateException("OPR generation exhausted");
        mGeneration++;
        mHaveFrame = false;
        mGray = -1;
        mPixels = 0;
        mSampleTime = -1;
        mNewestReadStart = -1;
        mActive = true;
        return mGeneration;
    }

    void endSession(long generation) {
        if (generation != mGeneration) return;
        mActive = false;
        mValid = false;
    }

    /**
     * Validates the full channel-0 result configured by stock: format 9, 256 bins. Kernel cached
     * frame counter zero means not ready. First read establishes a baseline: a cached pre-request
     * frame must not grant boost to the new session. Duplicate frames never renew the deadline.
     */
    Reception accept(long generation, long readStartedAt, long now, boolean transportOk,
            int result, int frame, int channel, int format, int binCount, int[] bins) {
        if (generation != mGeneration) return Reception.OLD_SESSION;
        if (!mActive) return Reception.INACTIVE;
        if (readStartedAt >= 0 && readStartedAt < mNewestReadStart) return Reception.OLD_REPLY;
        if (readStartedAt < 0 || now < readStartedAt
                || now - readStartedAt >= MAX_AGE_MILLIS) {
            mValid = false;
            return Reception.INVALID;
        }
        mNewestReadStart = readStartedAt;
        if (!transportOk || result != 0 || frame == 0 || channel != 0 || format != 9
                || binCount != 256 || bins == null || bins.length != 256) {
            mValid = false;
            return Reception.INVALID;
        }
        long pixels = 0;
        long weighted = 0;
        for (int i = 0; i < bins.length; i++) {
            // This panel has fewer than 2^31 pixels. A negative signed parcel count is invalid,
            // not permission to treat an unsigned/corrupt count as a dark screen.
            if (bins[i] < 0) {
                mValid = false;
                return Reception.INVALID;
            }
            pixels += bins[i];
            weighted += (long) bins[i] * i;
        }
        if (pixels == 0) {
            mValid = false;
            return Reception.INVALID;
        }
        if (!mHaveFrame) {
            mFrame = frame;
            mHaveFrame = true;
            mValid = false;
            return Reception.BASELINE;
        }
        // Unsigned 32-bit modular ordering allows wrap, but refuses backwards/reset counters.
        long delta = Integer.toUnsignedLong(frame - mFrame);
        if (delta == 0) return Reception.DUPLICATE;
        mFrame = frame;
        if (delta >= 0x80000000L) {
            mValid = false;
            return Reception.RESET;
        }
        mGray = (int) (weighted / pixels); // Same integer bin-weighted mean as stock, no filter.
        mPixels = pixels;
        mSampleTime = readStartedAt; // Time spent blocked in IO cannot extend permission.
        mValid = true;
        return Reception.VALID;
    }

    float cap(boolean automatic, float ambientLux, boolean hdrScene, long now) {
        // The separate stock HDR OPR table is disabled/empty on this device. Never use this SDR
        // table to grant the special HDR peak or identify pictures/videos by application name.
        if (!automatic || hdrScene) return Float.POSITIVE_INFINITY;
        if (!Float.isFinite(ambientLux) || ambientLux <= AMBIENT_LUX_THRESHOLD || !mActive
                || !mValid || now < mSampleTime || now - mSampleTime >= MAX_AGE_MILLIS) {
            return FALLBACK_NITS;
        }
        return capForGray(mGray);
    }

    private static float capForGray(int gray) {
        for (int i = 0; i < GRAY_THRESHOLDS.length; i++) {
            if (gray <= GRAY_THRESHOLDS[i]) return NITS[i];
        }
        return NITS[NITS.length - 1];
    }

    int gray() { return mGray; }
    long pixels() { return mPixels; }
    long validUntil() { return mValid ? mSampleTime + MAX_AGE_MILLIS : -1; }
}
