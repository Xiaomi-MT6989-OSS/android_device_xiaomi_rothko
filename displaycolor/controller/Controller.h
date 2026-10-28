// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <aidl/vendor/lineage/livedisplay/BnDisplayModes.h>
#include <aidl/vendor/xiaomi/rothko/display/color/BnRothkoDisplayColor.h>
#include <aidl/vendor/xiaomi/rothko/display/brightness/BnRothkoDozeBrightness.h>
#include <future>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace rothko::color {
namespace bridge = aidl::vendor::xiaomi::rothko::display::color;
namespace live = aidl::vendor::lineage::livedisplay;
class HistogramService;

class Controller final : public bridge::BnRothkoDisplayColor {
public:
    Controller();
    ~Controller() override;
    ndk::ScopedAStatus setListener(int64_t sfSession, int64_t connection,
            const std::shared_ptr<bridge::IColorPolicyCallback>& listener) override;
    ndk::ScopedAStatus reportState(const bridge::DisplayState& state) override;
    ndk::ScopedAStatus setMode(int32_t mode, bool makeDefault);
    bool applyDozeBrightness(int64_t physicalDisplayId, int32_t mode);
    int32_t currentMode() const { return mCurrent.load(); }
    int32_t defaultMode() const { return mDefault.load(); }
    const std::shared_ptr<HistogramService>& histogram() const { return mHistogram; }
private:
    struct Request {
        int32_t mode;
        bool makeDefault;
        uint64_t generation = 0;
        int64_t connection = 0;
        std::mutex mutex;
        std::condition_variable changed;
        std::atomic<bool> abandoned{false};
        bool done = false;
        bool ok = false;
        int error = 1;
    };
    struct DozeRequest {
        int32_t mode;
        std::promise<bool> completion;
    };
    void run();
    void finish(const std::shared_ptr<Request>& request, bool ok, int error);
    bool publishPolicy(uint64_t generation, int64_t connection, int64_t request,
            int32_t intent, bool wcg);
    // Atomic policy clear for the caller thread. The OEM worker may be blocked
    // in a vendor call, so a timeout must not depend on it making progress.
    void abandonFromCaller(const std::shared_ptr<Request>& request, int32_t mode);
    // `aborted` is polled after the file fsync and before the rename. Directory
    // durability failure after the rename is reported but never rolled back.
    bool persistDefault(int32_t mode, const std::function<bool()>& aborted);
    std::mutex mMutex;
    std::condition_variable mChanged;
    std::shared_ptr<bridge::IColorPolicyCallback> mListener;
    std::optional<bridge::DisplayState> mState;
    std::shared_ptr<Request> mRequest;
    // Admission ownership is separate from the worker's queue. Only finishing
    // this exact request may free its slot; a stale completion cannot free a new one.
    std::shared_ptr<Request> mActiveRequest;
    std::shared_ptr<Request> mApplyingRequest;
    std::shared_ptr<DozeRequest> mDozeRequest;
    uint64_t mGeneration = 0;
    int64_t mSession = 0;
    int64_t mConnection = 0;
    int64_t mLastSequence = 0;
    int64_t mSceneRevision = 0;
    int64_t mLastPublished = 0;
    // One diagnostic line per connection, so bring-up logs stay bounded.
    bool mReportLogged = false;
    std::atomic<int64_t> mPolicySequence{1};
    std::atomic<int32_t> mCurrent{-1};
    std::atomic<int32_t> mDefault{3};
    std::atomic<bool> mReady{false};
    std::atomic<bool> mStop{false};
    // An OEM mode request whose outcome we cannot attribute to any callback.
    // A later confirmation of this exact intent is ambiguous and is refused.
    std::atomic<int32_t> mUnresolvedIntent{-1};
    std::shared_ptr<HistogramService> mHistogram;
    std::thread mWorker;
};

class DisplayModes final : public live::BnDisplayModes {
public:
    explicit DisplayModes(std::shared_ptr<Controller> controller) : mController(std::move(controller)) {}
    ndk::ScopedAStatus getDisplayModes(std::vector<live::DisplayMode>* modes) override;
    ndk::ScopedAStatus getCurrentDisplayMode(live::DisplayMode* mode) override;
    ndk::ScopedAStatus getDefaultDisplayMode(live::DisplayMode* mode) override;
    ndk::ScopedAStatus setDisplayMode(int32_t mode, bool makeDefault) override;
private:
    std::shared_ptr<Controller> mController;
};

class DozeBrightness final : public
        aidl::vendor::xiaomi::rothko::display::brightness::BnRothkoDozeBrightness {
public:
    explicit DozeBrightness(std::shared_ptr<Controller> controller)
          : mController(std::move(controller)) {}
    ndk::ScopedAStatus apply(int64_t physicalDisplayId, int32_t mode, bool* applied) override;
private:
    std::shared_ptr<Controller> mController;
};
} // namespace rothko::color
