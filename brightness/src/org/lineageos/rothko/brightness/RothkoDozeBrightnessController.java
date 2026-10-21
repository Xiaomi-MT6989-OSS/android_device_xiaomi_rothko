/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Slog;
import android.view.Display;

import com.android.server.display.DeviceDozeBrightnessController;
import com.android.internal.display.BrightnessSynchronizer;

import vendor.xiaomi.rothko.display.brightness.IRothkoDozeBrightness;

/** Ordered dedicated backlight route, outside the global display lock. */
public final class RothkoDozeBrightnessController extends DeviceDozeBrightnessController {
    private static final String TAG = "RothkoDozeBrightness";
    static final long PANEL_ID = 4627039422300187648L;
    // Stock DisplayFeatureManagerService.DozeBrightnessMode ordinal values.
    static final int NORMAL = 0, HIGH = 1, LOW = 2;
    private final long mPhysicalId;
    private final Sink mSink;
    private boolean mRestorePending;
    private boolean mFailureLogged;

    /** No Binder lookup or IO here: LocalDisplayAdapter constructs under its lock. */
    public RothkoDozeBrightnessController(Context context, long physicalDisplayId) {
        this(physicalDisplayId, new Source());
    }

    RothkoDozeBrightnessController(long physicalDisplayId, Sink sink) {
        mPhysicalId = physicalDisplayId;
        mSink = sink;
    }

    @Override
    public boolean handlesDozeBrightness(int state) {
        return mPhysicalId == PANEL_ID
                && (state == Display.STATE_DOZE || state == Display.STATE_DOZE_SUSPEND);
    }

    @Override
    public void applyDozeBrightness(float brightness, float defaultDozeBrightness) {
        if (mPhysicalId != PANEL_ID) return;
        // Mark before sending: a failed Binder reply cannot prove the hardware
        // stayed unchanged. Exit must still issue NORMAL after normal backlight.
        mRestorePending = true;
        // Match stock's INTEGER-domain comparison, including quantization near
        // the 5-nit threshold. DDC supplies that calibrated low target. Neither
        // the raw backlight scale nor the framework normalization is hardcoded.
        // Screen-off dimming below that target retains the factory low mode.
        int mode = Float.isFinite(brightness) && Float.isFinite(defaultDozeBrightness)
                && BrightnessSynchronizer.brightnessFloatToInt(brightness)
                    > BrightnessSynchronizer.brightnessFloatToInt(defaultDozeBrightness)
                ? HIGH : LOW;
        send(mode);
    }

    @Override
    public void onNormalBrightnessApplied() {
        if (mPhysicalId == PANEL_ID && mRestorePending && send(NORMAL)) {
            mRestorePending = false;
        }
    }

    private boolean send(int mode) {
        boolean ok = mSink.apply(mPhysicalId, mode);
        if (!ok && !mFailureLogged) {
            Slog.e(TAG, "Dedicated AOD backlight unavailable, mode=" + mode);
        }
        mFailureLogged = !ok;
        return ok;
    }

    interface Sink { boolean apply(long physicalDisplayId, int mode); }

    private static final class Source implements Sink {
        private static final String NAME =
                "vendor.xiaomi.rothko.display.brightness.IRothkoDozeBrightness/default";
        private IRothkoDozeBrightness mService;
        @Override
        public boolean apply(long physicalDisplayId, int mode) {
            try {
                if (mService == null || !mService.asBinder().isBinderAlive()) {
                    IBinder binder = ServiceManager.checkService(NAME);
                    mService = binder == null ? null : IRothkoDozeBrightness.Stub.asInterface(binder);
                }
                return mService != null && mService.apply(physicalDisplayId, mode);
            } catch (RemoteException | RuntimeException e) {
                mService = null;
                return false;
            }
        }
    }
}
