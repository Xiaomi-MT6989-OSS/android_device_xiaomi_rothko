/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.rothko.brightness;

import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import java.io.PrintWriter;
import java.util.concurrent.atomic.AtomicLong;
import vendor.xiaomi.rothko.display.brightness.HistogramSample;
import vendor.xiaomi.rothko.display.brightness.IRothkoBrightness;

/** Private cached transport; all Binder IO belongs to a separate worker. */
final class OprBrightnessClient {
    private static final long POLL_MS = 200;
    private final Handler mDisplay, mWorker;
    private final HandlerThread mThread;
    private final Runnable mChanged;
    private final Source mSource;
    private final StockOprBrightnessState mState = new StockOprBrightnessState();
    private final Runnable mRead = this::read;
    private final Runnable mExpiry;
    private volatile long mRequest;
    private volatile boolean mActive, mRequested, mStopped;
    // Remaining state is confined to the display handler.
    private long mLocalSession = mState.beginSession();
    private long mRemoteGeneration = -1, mSourceEpoch = -1, mResponseStarted = -1;
    private boolean mSceneKnown, mHdr;
    private String mStatus = "inactive";

    OprBrightnessClient(Handler display, Runnable changed) {
        this(display, null, changed, new Source());
    }
    OprBrightnessClient(Handler display, Handler worker, Runnable changed, Source source) {
        mDisplay = display;
        mChanged = changed;
        mExpiry = () -> mChanged.run();
        mSource = source;
        if (worker == null) {
            mThread = new HandlerThread("RothkoOpr");
            mThread.start();
            mWorker = new Handler(mThread.getLooper());
        } else {
            mThread = null;
            mWorker = worker;
        }
    }

    void update(boolean active, boolean requestHistogram) {
        update(active, requestHistogram, false);
    }

    void update(boolean active, boolean requestHistogram, boolean contentChanged) {
        if (mStopped || (!contentChanged && active == mActive && requestHistogram == mRequested)) return;
        mActive = active;
        mRequested = requestHistogram;
        long request = ++mRequest;
        reset("awaiting new request");
        mWorker.removeCallbacks(mRead);
        if (active) mWorker.post(mRead);
        else mWorker.post(() -> {
            // A queued release must not cancel a newer on-session.
            if (mRequest == request && !mActive) mSource.release(request);
        });
    }

    private void reset(String status) {
        mState.endSession(mLocalSession);
        mLocalSession = mState.beginSession();
        mRemoteGeneration = -1;
        mSourceEpoch = -1;
        mSceneKnown = false;
        mResponseStarted = -1;
        mStatus = status;
        mDisplay.removeCallbacks(mExpiry);
    }

    float cap(boolean automatic, float lux, boolean frameworkHdrKnown, boolean frameworkHdrPresent) {
        if (!automatic) return Float.POSITIVE_INFINITY;
        long now = SystemClock.elapsedRealtime();
        if (!freshTransport(now)) {
            return StockOprBrightnessState.FALLBACK_NITS;
        }
        // RGB gain-map layers need not choose a PQ/HLG output dataspace. The upstream
        // HDR-layer report complements that candidate, but cannot bypass freshness or a
        // disconnected/unknown native scene. This only removes the ordinary SDR OPR limit.
        // A known upstream report also wins over an old PQ candidate (for example, camera
        // layers intentionally excluded by SF). The candidate is only a fallback before it.
        boolean hdr = frameworkHdrKnown ? frameworkHdrPresent : mHdr;
        return mState.cap(true, lux, mSceneKnown && hdr, now);
    }

    private boolean freshTransport(long now) {
        return !mStopped && mActive && mSourceEpoch == mSource.epoch()
                && mResponseStarted >= 0 && now >= mResponseStarted
                && now - mResponseStarted < StockOprBrightnessState.MAX_AGE_MILLIS;
    }

    float peakCap(boolean automatic, float lux, boolean hdrKnown, boolean hdrPresent) {
        long now = SystemClock.elapsedRealtime();
        // A dataspace candidate alone cannot grant the special peak. Require actual upstream
        // HDR layers plus a current native scene and this session's validated histogram.
        if (!freshTransport(now) || !mSceneKnown || !hdrKnown || !hdrPresent) {
            return StockOprBrightnessState.NORMAL_MAX_NITS;
        }
        return mState.peakCap(automatic, lux, true, now);
    }

    private void read() {
        if (mStopped || !mActive) return;
        long request = mRequest;
        long started = SystemClock.elapsedRealtime();
        Response response;
        try { response = mSource.read(request, mRequested); }
        catch (RemoteException | RuntimeException e) { response = null; }
        Response received = response;
        mDisplay.post(() -> {
            if (mStopped || !mActive || request != mRequest) return;
            long now = SystemClock.elapsedRealtime();
            if (received == null || received.epoch != mSource.epoch()
                    || now < started || now - started >= StockOprBrightnessState.MAX_AGE_MILLIS
                    || received.sample == null || received.sample.request != request
                    || received.sample.generation <= 0) {
                reset("transport unavailable or stale");
                mChanged.run();
                return;
            }
            HistogramSample sample = received.sample;
            if (mSourceEpoch != received.epoch || mRemoteGeneration != sample.generation) {
                reset("new producer generation");
                mRemoteGeneration = sample.generation;
            }
            mSourceEpoch = received.epoch;
            mResponseStarted = started;
            mSceneKnown = sample.sceneKnown;
            mHdr = sample.hdr;
            mStatus = mState.accept(mLocalSession, sample.readStartedAt, now,
                    sample.active && sample.sceneKnown && sample.transportOk && sample.shapeValid,
                    sample.result, sample.frame, sample.channel, sample.format,
                    sample.binCount, sample.bins).name();
            mDisplay.removeCallbacks(mExpiry);
            // A duplicated cached frame must not postpone its original expiry.
            long deadline = Math.min(started + StockOprBrightnessState.MAX_AGE_MILLIS,
                    mState.validUntil());
            if (deadline > now) mDisplay.postDelayed(mExpiry, deadline - now);
            // HDR scene identity also expires if the cached transport stops responding.
            mDisplay.postDelayed(mExpiry, started + StockOprBrightnessState.MAX_AGE_MILLIS - now);
            mChanged.run();
        });
        if (!mStopped && mActive && request == mRequest) mWorker.postDelayed(mRead, POLL_MS);
    }

    void stop() {
        mStopped = true;
        mActive = false;
        long request = ++mRequest;
        reset("stopped");
        mWorker.removeCallbacks(mRead);
        mWorker.post(() -> mSource.release(request));
        if (mThread != null) mThread.quitSafely();
    }
    void dump(PrintWriter out) {
        out.println("    OPR: request=" + mRequest + " producer=" + mRemoteGeneration
                + " requested=" + mRequested + " sceneKnown=" + mSceneKnown + " hdr=" + mHdr
                + " gray=" + mState.gray() + " population=" + mState.pixels() + " status=" + mStatus);
    }

    static final class Response {
        final long epoch;
        final HistogramSample sample;
        Response(long epoch, HistogramSample sample) { this.epoch = epoch; this.sample = sample; }
    }
    // Worker-confined service binding; only the invalidation counter is read
    // elsewhere. A death callback revokes permission before queued work runs.
    static class Source {
        private static final String SERVICE =
                "vendor.xiaomi.rothko.display.brightness.IRothkoBrightness/default";
        private final IBinder mClient = new Binder();
        private final AtomicLong mEpoch = new AtomicLong();
        private IBinder mBinder;
        private IRothkoBrightness mService;
        private IBinder.DeathRecipient mDeath;
        long epoch() { return mEpoch.get(); }
        Response read(long request, boolean enable) throws RemoteException {
            if (mBinder == null || !mBinder.isBinderAlive()) {
                disconnect();
                IBinder binder = ServiceManager.checkService(SERVICE);
                if (binder == null) return null;
                mDeath = () -> mEpoch.incrementAndGet();
                binder.linkToDeath(mDeath, 0);
                mBinder = binder;
                mService = IRothkoBrightness.Stub.asInterface(binder);
                mEpoch.incrementAndGet();
            }
            long epoch = epoch();
            try { return new Response(epoch, mService.readSample(mClient, request, enable)); }
            catch (RemoteException | RuntimeException e) { disconnect(); throw e; }
        }
        void release(long request) {
            try {
                if (mService != null && mBinder.isBinderAlive()) mService.readSample(mClient, request, false);
            } catch (RemoteException | RuntimeException ignored) {
                // A crashed or blocked consumer also loses its native short lease.
            } finally { disconnect(); }
        }
        private void disconnect() {
            mEpoch.incrementAndGet();
            try {
                if (mBinder != null && mDeath != null) mBinder.unlinkToDeath(mDeath, 0);
            } catch (java.util.NoSuchElementException ignored) {
                // A dead recipient may already have been removed by Binder.
            }
            mBinder = null;
            mService = null;
            mDeath = null;
        }
    }
}
