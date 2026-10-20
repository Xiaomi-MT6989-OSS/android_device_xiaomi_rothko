/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.rothko.brightness;

import android.content.Context;
import android.os.FileObserver;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.util.Slog;
import android.view.Display;

import com.android.server.display.brightness.DeviceBrightnessPolicy;

import java.io.FileInputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.IntConsumer;

/** Factory stock display thermal limits. Read-only: never writes thermal or panel nodes. */
public final class RothkoBrightnessPolicy extends DeviceBrightnessPolicy {
    private static final String TAG = "RothkoBrightness";
    private static final String CONFIG =
            "/product/etc/displayconfig/common_multi_factor_thermal_brightness_control.xml";
    private static final String THERMAL = "/sys/class/thermal/thermal_message/";
    private static final long POLL_MILLIS = 1000;
    private static final long MAX_AGE_MILLIS = 2500;
    // Minimum positive cap in the pinned factory XML (not an invented temperature threshold).
    private static final float FACTORY_MIN_CAP_NITS = 160f;

    private final Handler mDisplayHandler;
    private final Runnable mOnChanged;
    private final HandlerThread mThread;
    private final Handler mWorker;
    private final Injector mInjector;
    private final Runnable mPoll = this::poll;
    private final Runnable mExpiry;
    private final AtomicBoolean mEventRefreshQueued = new AtomicBoolean();
    private final AtomicLong mWatchInvalidations = new AtomicLong();
    // FileObserver registrations are created and released only on the IO worker.
    private final List<FileObserver> mObservers = new ArrayList<>();
    private long mWatchGeneration = -1;
    private long mRegisteredWatchEpoch = -1;
    private volatile int mWatchRequests;
    // Generation and active/stopped are also read on the IO worker.
    private volatile long mGeneration;
    private volatile boolean mActive;
    private volatile boolean mStopped;
    private volatile boolean mNtcEnabled;
    private volatile boolean mNtcFlagRead;
    private StockBrightnessConfig mConfig; // Immutable, worker only after first load.

    // Everything below is display-handler confined.
    private StockBrightnessState mState;
    private boolean mAutomatic;
    private float mLux = Float.NaN;
    private boolean mValid;
    private long mSampleTime;
    private int mCondition;
    private float mSafetyNits;
    private float mFailureCap = FACTORY_MIN_CAP_NITS;
    private float mLastCap = FACTORY_MIN_CAP_NITS;
    private String mStatus = "awaiting factory config and thermal sample";

    public RothkoBrightnessPolicy(Context context, Handler handler, Runnable onChanged) {
        this(handler, null, onChanged, new Injector());
    }

    // Host lifecycle tests inject IO and queued handlers; production always owns its worker.
    RothkoBrightnessPolicy(Handler handler, Handler worker, Runnable onChanged, Injector injector) {
        mDisplayHandler = handler;
        mOnChanged = onChanged;
        mExpiry = () -> mOnChanged.run();
        mInjector = injector;
        if (worker == null) {
            mThread = new HandlerThread(TAG);
            mThread.start();
            mWorker = new Handler(mThread.getLooper());
        } else {
            mThread = null;
            mWorker = worker;
        }
    }

    @Override
    public void setDisplayState(int state) {
        boolean active = state == Display.STATE_ON;
        if (mStopped || active == mActive) return;
        mActive = active;
        mGeneration++;
        mValid = false;
        mLux = Float.NaN; // ALS from a previous screen-on session cannot permit a new high limit.
        mStatus = active ? "awaiting fresh sample after screen on" : "screen inactive";
        mDisplayHandler.removeCallbacks(mExpiry);
        mWorker.removeCallbacks(mPoll);
        if (active) mWorker.post(mPoll);
        else mWorker.post(() -> {
            if (!mActive || mStopped) stopObservers();
        });
        mOnChanged.run();
    }

    @Override
    public void setAutomaticBrightness(boolean enabled) {
        if (mAutomatic == enabled) return;
        mAutomatic = enabled;
        mOnChanged.run();
    }

    @Override
    public void setAmbientLux(float lux) {
        if (Float.compare(mLux, lux) == 0) return;
        mLux = lux;
        mOnChanged.run();
    }

    @Override
    public float getMaxBrightnessNits() {
        if (!mValid || mState == null || mStopped
                || SystemClock.elapsedRealtime() - mSampleTime >= MAX_AGE_MILLIS) {
            return Math.min(mFailureCap, mLastCap);
        }
        mLastCap = mState.cap(mAutomatic, mLux, mCondition, mSafetyNits, mNtcEnabled);
        return mLastCap;
    }

    private void stopObservers() {
        for (FileObserver observer : mObservers) observer.stopWatching();
        mObservers.clear();
        mWatchRequests = 0;
        mWatchGeneration = -1;
    }

    private void ensureObservers(long generation) {
        long watchEpoch = mWatchInvalidations.get();
        if (mWatchGeneration == generation && mRegisteredWatchEpoch == watchEpoch) return;
        stopObservers();
        List<String> nodes = new ArrayList<>(List.of(
                "board_sensor_temp", "sconfig", "thermal_max_brightness"));
        if (mNtcEnabled) nodes.add("display_therm_temp");
        for (String node : nodes) {
            FileObserver observer = mInjector.newObserver(node,
                    event -> onNodeEvent(generation, event));
            mObservers.add(observer); // Keep a strong reference while inotify is registered.
            observer.startWatching();
        }
        mWatchGeneration = generation;
        mRegisteredWatchEpoch = watchEpoch;
        // This is the number of requested watches; FileObserver does not expose registration status.
        mWatchRequests = mObservers.size();
    }

    private void onNodeEvent(long generation, int event) {
        if (mStopped || !mActive || generation != mGeneration) return;
        int gone = event & (FileObserver.DELETE_SELF | FileObserver.MOVE_SELF);
        if ((event & FileObserver.MODIFY) == 0 && gone == 0) return;
        if (gone != 0) {
            mWatchInvalidations.incrementAndGet();
            mDisplayHandler.post(() -> {
                if (mStopped || !mActive || generation != mGeneration) return;
                mValid = false;
                mStatus = "thermal node moved/deleted; re-registering";
                mOnChanged.run();
            });
        }
        // Match stock IN_MODIFY notification, coalescing bursts before one complete snapshot.
        if (!mEventRefreshQueued.compareAndSet(false, true)) return;
        mWorker.post(() -> {
            mEventRefreshQueued.set(false);
            if (mStopped || !mActive || generation != mGeneration) return;
            mWorker.removeCallbacks(mPoll);
            mWorker.post(mPoll);
        });
    }

    private void poll() {
        if (mStopped || !mActive) return;
        long generation = mGeneration;
        long sampleTime = SystemClock.elapsedRealtime();
        try {
            if (mConfig == null) {
                StockBrightnessConfig loaded = mInjector.loadConfig();
                if (Float.compare(loaded.minimumCapNits, FACTORY_MIN_CAP_NITS) != 0) {
                    throw new IOException("Factory config minimum differs from pinned policy");
                }
                mConfig = loaded;
            }
            if (!mNtcFlagRead) {
                mNtcEnabled = mInjector.ntcEnabled();
                mNtcFlagRead = true;
            }
            // Register before the initial sample so writes during setup are not missed.
            ensureObservers(generation);
            long watchEpoch = mRegisteredWatchEpoch;
            float board = Integer.parseInt(mInjector.readNode("board_sensor_temp")) / 1000f;
            int condition = Integer.parseInt(mInjector.readNode("sconfig"));
            float safety = Float.parseFloat(mInjector.readNode("thermal_max_brightness"));
            float ntc = mNtcEnabled
                    ? Integer.parseInt(mInjector.readNode("display_therm_temp")) / 1000f : Float.NaN;
            if (!Float.isFinite(safety) || safety < 0) throw new IOException("Invalid safety cap");
            StockBrightnessConfig config = mConfig;
            mDisplayHandler.post(() -> {
                if (mStopped || !mActive || generation != mGeneration
                        || watchEpoch != mWatchInvalidations.get()) return;
                if (mState == null) mState = new StockBrightnessState(config);
                mFailureCap = config.minimumCapNits;
                mState.update(board, ntc);
                mSampleTime = sampleTime; // Start of IO, never completion of a blocked read.
                mCondition = condition;
                mSafetyNits = safety;
                mValid = true;
                mStatus = "factory fallback; sysfs sample valid";
                mDisplayHandler.removeCallbacks(mExpiry);
                long age = SystemClock.elapsedRealtime() - sampleTime;
                mDisplayHandler.postDelayed(mExpiry, Math.max(0, MAX_AGE_MILLIS - age));
                mOnChanged.run();
            });
        } catch (Exception e) {
            String reason = e.getClass().getSimpleName() + ": " + e.getMessage();
            mDisplayHandler.post(() -> {
                if (mStopped || !mActive || generation != mGeneration) return;
                if (!reason.equals(mStatus)) Slog.w(TAG, "Thermal input unavailable: " + reason);
                mStatus = reason;
                mValid = false;
                if (mState != null) mState.invalidate();
                mDisplayHandler.removeCallbacks(mExpiry);
                mOnChanged.run();
            });
        } finally {
            // A generation change already queued its fresh poll; do not duplicate it here.
            if (!mStopped && mActive && generation == mGeneration) {
                mWorker.removeCallbacks(mPoll);
                mWorker.postDelayed(mPoll, POLL_MILLIS);
            }
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("  RothkoBrightnessPolicy: source=" + CONFIG);
        writer.println("    active=" + mActive + " automatic=" + mAutomatic + " lux=" + mLux
                + " ntcEnabled=" + mNtcEnabled + " valid=" + mValid);
        writer.println("    requestedInotifyWatches=" + mWatchRequests);
        writer.println("    condition=" + mCondition + " boardC="
                + (mState == null ? Float.NaN : mState.appliedBoardTemperature())
                + " safetyNits=" + mSafetyNits + " capNits=" + getMaxBrightnessNits());
        writer.println("    sampleAgeMs=" + (SystemClock.elapsedRealtime() - mSampleTime)
                + " status=" + mStatus);
    }

    @Override
    public void stop() {
        mStopped = true;
        mGeneration++;
        mActive = false;
        mValid = false;
        mDisplayHandler.removeCallbacks(mExpiry);
        mWorker.removeCallbacks(mPoll);
        mWorker.post(this::stopObservers);
        if (mThread != null) mThread.quitSafely();
    }

    static class Injector {
        FileObserver newObserver(String name, IntConsumer onEvent) {
            return new FileObserver(new File(THERMAL + name),
                    FileObserver.MODIFY | FileObserver.DELETE_SELF | FileObserver.MOVE_SELF) {
                @Override public void onEvent(int event, String path) {
                    onEvent.accept(event);
                }
            };
        }
        StockBrightnessConfig loadConfig() throws Exception {
            try (FileInputStream input = new FileInputStream(CONFIG)) {
                return StockBrightnessConfig.read(input);
            }
        }
        boolean ntcEnabled() {
            return SystemProperties.getBoolean("persist.vendor.ntc.flag", false);
        }
        String readNode(String name) throws IOException {
            try (FileInputStream input = new FileInputStream(THERMAL + name)) {
                byte[] bytes = input.readNBytes(257);
                if (bytes.length == 0 || bytes.length > 256) {
                    throw new IOException("Invalid " + name);
                }
                return new String(bytes, StandardCharsets.US_ASCII).trim();
            }
        }
    }
}
