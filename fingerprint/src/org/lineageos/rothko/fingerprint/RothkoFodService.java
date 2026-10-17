/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.fingerprint;

import android.app.ActivityManager;
import android.content.Context;
import android.database.ContentObserver;
import android.hardware.display.DisplayManager;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Slog;
import android.view.Display;

import com.android.server.LocalServices;
import com.android.server.SystemService;
import com.android.server.biometrics.sensors.fingerprint.UdfpsLifecycle;

import java.io.PrintWriter;

/** Device-only bridge to the original FodEngine. No authentication or illumination emulation. */
public final class RothkoFodService extends SystemService {
    private static final String TAG = "RothkoFod";
    private static final int SENSOR_ID = 5;
    private static final String DESCRIPTOR =
            "vendor.xiaomi.hardware.fingerprintextension.IXiaomiFingerprint";
    private static final String INSTANCE = DESCRIPTOR + "/default";
    private final HandlerThread mThread = new HandlerThread(TAG);
    private Handler mHandler;
    private DisplayManager mDisplays;
    private FodStateMachine mState;
    private IBinder mExtension;
    private boolean mNeedsBaseline = true;
    private int mCurrentUser;
    private volatile String mSummary = "starting";
    private volatile long mTransactionSince;

    public RothkoFodService(Context context) { super(context); }

    @Override
    public void onStart() {
        mThread.start();
        mHandler = new Handler(mThread.getLooper());
        mCurrentUser = ActivityManager.getCurrentUser();
        mState = new FodStateMachine(mCurrentUser);
        mDisplays = getContext().getSystemService(DisplayManager.class);
        getContext().getContentResolver().registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.SCREEN_OFF_UNLOCK_UDFPS_ENABLED),
                false, new ContentObserver(mHandler) {
                    @Override public void onChange(boolean selfChange) {
                        post(() -> updateScreenOffPreference());
                    }
                }, UserHandle.USER_ALL);
        LocalServices.addService(UdfpsLifecycle.class, new UdfpsLifecycle() {
            @Override public void onOperationStarted(int sensor, long request, int user, int state) {
                if (sensor == SENSOR_ID) post(() -> mState.start(request, user, state));
            }
            @Override public void onOperationFinished(int sensor, long request) {
                if (sensor == SENSOR_ID) post(() -> mState.finish(request));
            }
            @Override public void onOverlayChanged(int sensor, long request, boolean attached) {
                if (sensor == SENSOR_ID) post(() -> mState.overlay(request, attached));
            }
            @Override public void onControllerUnavailable() { post(() -> mState.cancel()); }
            @Override public void onHalUnavailable(int sensor) {
                if (sensor == SENSOR_ID) post(() -> mState.cancel());
            }
            @Override public void onLockoutChanged(int sensor, int user, int mode) {
                if (sensor == SENSOR_ID) post(() -> mState.lockout(user, mode));
            }
            @Override public void onDisplayChanged(DisplayState state) {
                post(() -> updateDisplay(state));
            }
            @Override public void dump(PrintWriter writer) {
                writer.println("RothkoFod: " + mSummary);
                final long since = mTransactionSince;
                writer.println("  OEM transaction pendingMs="
                        + (since == 0 ? 0 : SystemClock.elapsedRealtime() - since));
                writer.println("  capturePolicy=ON or opted-in keyguard OFF/DOZE; no polling or root helper");
            }
        });
        mDisplays.registerDisplayListener(new DisplayManager.DisplayListener() {
            @Override public void onDisplayAdded(int displayId) { onDisplayChanged(displayId); }
            @Override public void onDisplayRemoved(int displayId) { onDisplayChanged(displayId); }
            @Override public void onDisplayChanged(int displayId) {
                if (displayId == Display.DEFAULT_DISPLAY) {
                    updateAndApply(() -> updateDisplay(UdfpsLifecycle.getDefaultDisplayState()));
                }
            }
        }, mHandler);
        post(() -> {
            updateScreenOffPreference();
            updateDisplay(UdfpsLifecycle.getDefaultDisplayState());
        });
        Slog.i(TAG, "Registered stock fingerprint lifecycle bridge");
    }

    @Override
    public void onUserSwitching(TargetUser from, TargetUser to) {
        post(() -> {
            mCurrentUser = to.getUserIdentifier();
            mState.user(mCurrentUser);
            updateScreenOffPreference();
        });
    }

    @Override
    public void onUserStopping(TargetUser user) {
        // Stop any current capture. A subsequent framework request establishes its own user.
        post(() -> mState.cancel());
    }

    private void post(Runnable event) { mHandler.post(() -> updateAndApply(event)); }

    private void updateScreenOffPreference() {
        final boolean supported = getContext().getResources().getBoolean(
                com.android.internal.R.bool.config_screen_off_udfps_enabled);
        final boolean defaultOn = getContext().getResources().getBoolean(
                com.android.internal.R.bool.config_screen_off_udfps_default_on);
        mState.screenOffUnlock(supported && Settings.Secure.getIntForUser(
                getContext().getContentResolver(), Settings.Secure.SCREEN_OFF_UNLOCK_UDFPS_ENABLED,
                defaultOn ? 1 : 0, mCurrentUser) != 0);
    }

    private void updateDisplay(UdfpsLifecycle.DisplayState state) {
        final Display display = mDisplays.getDisplay(Display.DEFAULT_DISPLAY);
        mState.display(state == null ? Display.STATE_UNKNOWN : state.state,
                display == null ? Display.STATE_UNKNOWN : display.getState(),
                state == null || state.colorFade);
    }

    private void updateAndApply(Runnable event) {
        try {
            event.run();
            if (connect()) mState.apply(this::send);
            mSummary = (mExtension == null ? "unavailable " : "connected ") + mState.summary();
        } catch (Exception exception) {
            mState.cancel();
            boolean cleaned = true;
            if (mExtension != null && mExtension.isBinderAlive()) {
                for (int[] stop : new int[][] {{1, 0}, {4, 2}, {4, 4}, {7, 0}}) {
                    try { send(stop[0], stop[1]); }
                    catch (Exception cleanupFailure) { cleaned = false; }
                }
            } else {
                cleaned = false;
            }
            mState.forgetTransport();
            mNeedsBaseline = true;
            mSummary = "fault=" + exception.getClass().getSimpleName()
                    + " cleanupConfirmed=" + cleaned + " " + mState.summary();
            Slog.e(TAG, mSummary);
        }
    }

    private boolean connect() throws Exception {
        if (mExtension != null && !mExtension.isBinderAlive()) {
            mExtension = null;
            mNeedsBaseline = true;
            mState.cancel();
            mState.forgetTransport();
        }
        if (mExtension == null) {
            final IBinder binder = ServiceManager.checkService(INSTANCE);
            if (binder == null) return false;
            Binder.allowBlocking(binder);
            final Parcel data = Parcel.obtain();
            final Parcel reply = Parcel.obtain();
            try {
                data.writeInterfaceToken(DESCRIPTOR);
                mTransactionSince = SystemClock.elapsedRealtime();
                if (!binder.transact(16777215, data, reply, 0)) {
                    throw new IllegalStateException("OEM interface version unavailable");
                }
                reply.readException();
                if (reply.readInt() != 1) throw new IllegalStateException("Unsupported OEM version");
            } finally {
                mTransactionSince = 0;
                reply.recycle();
                data.recycle();
            }
            binder.linkToDeath(() -> mHandler.post(() -> {
                if (mExtension != binder) return;
                mExtension = null;
                mNeedsBaseline = true;
                mState.cancel();
                mState.forgetTransport();
                mSummary = "OEM service died; waiting for a fresh framework request";
                Slog.w(TAG, mSummary);
            }), 0);
            mExtension = binder;
            mNeedsBaseline = true;
        }
        if (mNeedsBaseline) {
            // Explicit stop for both operation kinds after attachment/recovery, never replay a
            // stale hardware state. No template, calibration or authentication-token operations.
            send(1, 0);
            send(4, 2);
            send(4, 4);
            send(7, 0);
            mState.forgetTransport();
            mNeedsBaseline = false;
        }
        return true;
    }

    private void send(int command, int value) throws Exception {
        if (!((command == 1 || command == 2 || command == 6 || command == 7)
                        && (value == 0 || value == 1))
                && !(command == 3 && value >= 0 && value <= 3)
                && !(command == 4 && value >= 1 && value <= 4)
                && !(command == 5 && value >= 0 && value <= 2)) {
            throw new IllegalArgumentException("Unverified OEM condition");
        }
        final Parcel data = Parcel.obtain();
        final Parcel reply = Parcel.obtain();
        try {
            data.writeInterfaceToken(DESCRIPTOR);
            data.writeInt(command);
            data.writeInt(value);
            mTransactionSince = SystemClock.elapsedRealtime();
            if (!mExtension.transact(1, data, reply, 0)) {
                throw new IllegalStateException("OEM transaction not implemented");
            }
            reply.readException();
            if (reply.readInt() != 0) throw new IllegalStateException("OEM condition rejected");
        } finally {
            mTransactionSince = 0;
            reply.recycle();
            data.recycle();
        }
    }
}
