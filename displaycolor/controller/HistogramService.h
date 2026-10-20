// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "HistogramSession.h"
#include "StockDisplayFeature.h"
#include <aidl/vendor/xiaomi/rothko/display/brightness/BnRothkoBrightness.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace rothko::color {
namespace brightness = aidl::vendor::xiaomi::rothko::display::brightness;

class HistogramService final : public brightness::BnRothkoBrightness {
public:
    HistogramService();
    ~HistogramService() override;
    ndk::ScopedAStatus readSample(const ndk::SpAIBinder& client, int64_t request,
            bool enable, brightness::HistogramSample* out) override;
    // Scene changes invalidate cached permission immediately, including while
    // the OEM command thread or PQ reader is blocked in a vendor transaction.
    void scene(bool known, bool hdr, int64_t token);
    void newSf(int64_t session, int64_t connection);
    void sfGuard(std::shared_ptr<std::atomic<bool>> dead, int64_t session, int64_t connection);
    void backend(std::shared_ptr<std::atomic<bool>> dead);
    // Existing OEM serial worker ONLY. No other method sends an OEM command.
    void drive(aidl::vendor::xiaomi::hardware::displayfeature_aidl::IDisplayFeature& service,
            int64_t sceneToken);
private:
    void run();
    void reapLocked();
    std::mutex mMutex;
    std::condition_variable mChanged;
    HistogramSession mSession;
    ndk::SpAIBinder mClient;
    std::shared_ptr<std::atomic<bool>> mClientDead, mSfDead, mOemDead, mPqDead;
    bool mClientReaped = false, mSfReaped = false, mPqReaped = false;
    // Defined out of line so callers do not need the death-recipient helper.
    class Watch;
    std::unique_ptr<Watch> mClientWatch;
    int64_t mNextCommand = 0;
    int64_t mSceneToken = 0;
    bool mStop = false;
    std::thread mWorker;
};
} // namespace rothko::color
