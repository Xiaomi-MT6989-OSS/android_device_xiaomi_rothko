// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "StockPqReader.h"
#include <optional>

namespace rothko::color {

// IO-free state machine. The owner serializes access; all time is BOOTTIME ms.
// A short consumer lease is an adaptation watchdog, not an OEM filtering delay.
class HistogramSession {
public:
    static constexpr int64_t kLeaseMs = 1200;
    struct Command { int64_t generation; bool enable; };
    int64_t generation() const { return mGeneration; }
    bool oemAvailable() const { return mOem; }
    bool sceneKnown() const { return mKnown && mOem; }
    bool hdr() const { return mHdr; }
    void newSf(int64_t session, int64_t connection) {
        mSfSession = session;
        mSfConnection = connection;
        scene(false, false);
    }
    bool sameSf(int64_t session, int64_t connection) const {
        return session == mSfSession && connection == mSfConnection;
    }

    void invalidate() { ++mGeneration; mSample = {}; }
    void scene(bool known, bool hdr) { mKnown = known; mHdr = hdr; invalidate(); }
    // A previous controller process may have died with this OEM client enabled.
    // Establish ownership by disabling once before any new request is enabled.
    void oemConnected() { mOem = true; mHardware = true; mEnabledGeneration = -1; invalidate(); }
    void oemLost() { mOem = false; mHardware = false; invalidate(); }
    void release() { mRequested = false; mRequest = -1; invalidate(); }
    void request(int64_t request, bool enable, int64_t now) {
        if (request != mRequest || enable != mRequested ||
            (enable && now >= mLeaseUntil)) invalidate();
        mRequest = request;
        mRequested = enable;
        mLeaseUntil = now + kLeaseMs;
    }
    bool wanted(int64_t now) const {
        // HDR bypasses the ordinary SDR table, but stock's conditional peak still needs
        // valid gray statistics. The consumer requests them only when its lux/mode need it.
        return mRequested && now >= 0 && now < mLeaseUntil && sceneKnown();
    }
    bool readable(int64_t now) const {
        return wanted(now) && mHardware && mEnabledGeneration == mGeneration;
    }
    std::optional<Command> command(int64_t now) {
        if (mRequested && now >= mLeaseUntil) release();
        if (!mOem) return std::nullopt;
        // A new generation must disable before re-enabling. An old successful
        // enable may have changed hardware even though its request was cancelled.
        if (mHardware && (!wanted(now) || mEnabledGeneration != mGeneration)) {
            return Command{mGeneration, false};
        }
        if (!mHardware && wanted(now)) return Command{mGeneration, true};
        return std::nullopt;
    }
    void complete(Command command, bool ok) {
        if (!mOem) return;
        if (!ok) {
            // The failed transaction may have reached the server. Force a
            // disabling command before any retry; never grant readable state.
            mHardware = true;
            mEnabledGeneration = -1;
            invalidate();
            return;
        }
        mHardware = command.enable;
        mEnabledGeneration = command.enable ? command.generation : -1;
    }
    bool publish(int64_t generation, const PqReadback& sample, int64_t now) {
        if (generation != mGeneration || !readable(now)) return false;
        mSample = sample;
        return true;
    }
    PqReadback snapshot(int64_t now) const { return readable(now) ? mSample : PqReadback{}; }

private:
    int64_t mGeneration = 1;
    int64_t mSfSession = 0, mSfConnection = 0;
    int64_t mRequest = -1;
    int64_t mLeaseUntil = -1;
    int64_t mEnabledGeneration = -1;
    bool mRequested = false, mKnown = false, mHdr = false;
    bool mOem = false, mHardware = false;
    PqReadback mSample;
};
} // namespace rothko::color
