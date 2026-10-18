/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.fingerprint;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** Single-owner state machine for the stock rothko FodEngine condition protocol. */
final class FodStateMachine {
    interface Transport {
        void send(int command, int value) throws Exception;
    }

    private static final class Operation {
        final long request;
        final int user;
        final int state;
        boolean attached;

        Operation(long request, int user, int state) {
            this.request = request;
            this.user = user;
            this.state = state;
        }

        boolean enrolling() { return state == 1; }
        boolean keyguard() { return state == 2; }
    }

    private final Map<Integer, Integer> mLockouts = new HashMap<>();
    private final int[] mSent = new int[8];
    private Operation mOperation;
    private Operation mAppliedOperation;
    private int mUser;
    private int mDesiredDisplay;
    private int mActualDisplay;
    private boolean mFade = true;
    private boolean mScreenOffUnlock;

    FodStateMachine(int user) {
        mUser = user;
        forgetTransport();
    }

    void start(long request, int user, int state) {
        if (request <= 0 || state < 1 || state > 4 || user != mUser) return;
        if (mOperation != null && mOperation.request == request && mOperation.user == user) return;
        mOperation = new Operation(request, user, state);
    }

    void finish(long request) {
        if (mOperation != null && mOperation.request == request) mOperation = null;
    }

    void overlay(long request, boolean attached) {
        if (mOperation != null && mOperation.request == request) mOperation.attached = attached;
    }

    void display(int desired, int actual, boolean fade) {
        mDesiredDisplay = desired;
        mActualDisplay = actual;
        mFade = fade;
    }

    void lockout(int user, int mode) {
        if (mode >= 0 && mode <= 2) mLockouts.put(user, mode);
    }

    void screenOffUnlock(boolean enabled) { mScreenOffUnlock = enabled; }

    void user(int user) {
        mOperation = null;
        mUser = user;
    }

    void cancel() { mOperation = null; }

    void forgetTransport() {
        Arrays.fill(mSent, Integer.MIN_VALUE);
        mAppliedOperation = null;
    }

    private void send(Transport transport, int command, int value) throws Exception {
        if (mSent[command] == value) return;
        transport.send(command, value);
        mSent[command] = value;
    }

    void apply(Transport transport) throws Exception {
        final Integer lockout = mLockouts.get(mUser);
        final boolean screenOn = mDesiredDisplay == 2 && mActualDisplay == 2 && !mFade;
        final boolean stableLowPower = mDesiredDisplay == mActualDisplay
                && (mActualDisplay == 1 || mActualDisplay == 3 || mActualDisplay == 4);
        // Stock FodEngine accepts real OFF/DOZE power with an attached keyguard UI.
        // A persistent ColorFade surface while OFF is reported truthfully to the HAL;
        // it must not be confused with an ON-screen transition. Enroll/app auth remain ON-only.
        final boolean screenOffKeyguard = mScreenOffUnlock && stableLowPower
                && mOperation != null && mOperation.keyguard();
        // A real, already-applied keyguard request remains valid while OFF/DOZE/ON
        // and ColorFade catch up with one another. Stopping and replaying that
        // request here clears stock FodEngine's in-flight local-HBM request and
        // loses the finger which initiated wake. Keep reporting physical power
        // and fade below; this neither starts a new request nor fakes panel ON.
        final boolean continuingKeyguard = mScreenOffUnlock && mOperation != null
                && mOperation.keyguard() && mAppliedOperation == mOperation
                && mDesiredDisplay >= 1 && mDesiredDisplay <= 4
                && mActualDisplay >= 1 && mActualDisplay <= 4;
        final boolean active = mOperation != null && mOperation.attached
                && mOperation.user == mUser && lockout != null && lockout == 0
                && (screenOn || screenOffKeyguard || continuingKeyguard);
        if (mAppliedOperation != null && (!active || mAppliedOperation != mOperation)) {
            send(transport, 1, 0);
            send(transport, 4, mAppliedOperation.enrolling() ? 2 : 4);
            mAppliedOperation = null;
            send(transport, 7, 0);
        }
        // Display.STATE -> SurfaceControl POWER_MODE, verified against platform constants.
        final int power = mActualDisplay == 1 ? 0 : mActualDisplay == 2 ? 2
                : mActualDisplay == 3 ? 1 : mActualDisplay == 4 ? 3 : -1;
        if (power != -1) send(transport, 3, power);
        send(transport, 2, mFade ? 1 : 0);
        if (lockout != null) send(transport, 5, lockout);
        if (active) {
            send(transport, 7, mOperation.keyguard() ? 1 : 0);
            if (mAppliedOperation == null) {
                send(transport, 4, mOperation.enrolling() ? 1 : 3);
                mAppliedOperation = mOperation;
            }
            send(transport, 1, 1);
        } else {
            send(transport, 1, 0);
            send(transport, 7, 0);
        }
    }

    String summary() {
        return "operation=" + (mOperation == null ? "idle" : mOperation.enrolling() ? "enroll" : "auth")
                + " overlay=" + (mOperation != null && mOperation.attached)
                + " vendorOperation=" + (mAppliedOperation != null)
                + " desiredDisplay=" + mDesiredDisplay + " actualDisplay=" + mActualDisplay
                + " colorFade=" + mFade + " lockout=" + mLockouts.get(mUser)
                + " screenOffUnlock=" + mScreenOffUnlock
                + " uiCondition=" + mSent[1] + " keyguardLowBrightness=" + mSent[7];
    }
}
