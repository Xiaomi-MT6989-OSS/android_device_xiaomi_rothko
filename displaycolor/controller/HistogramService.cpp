// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#include "HistogramService.h"
#include <android/binder_manager.h>
#include <time.h>

namespace rothko::color {
namespace {
int64_t nowMs() {
    timespec value{};
    if (clock_gettime(CLOCK_BOOTTIME, &value)) return -1;
    return value.tv_sec * 1000LL + value.tv_nsec / 1000000;
}
constexpr const char* kPqService =
        "vendor.mediatek.hardware.pq_aidl.IPictureQuality_AIDL/default";
} // namespace

class HistogramService::Watch {
public:
    Watch(ndk::SpAIBinder binder, const std::shared_ptr<std::atomic<bool>>& dead)
          : mBinder(std::move(binder)),
            mRecipient(AIBinder_DeathRecipient_new([](void* raw) {
                auto* cookie = static_cast<std::weak_ptr<std::atomic<bool>>*>(raw);
                if (auto flag = cookie->lock()) flag->store(true);
            })) {
        AIBinder_DeathRecipient_setOnUnlinked(mRecipient.get(), [](void* raw) {
            delete static_cast<std::weak_ptr<std::atomic<bool>>*>(raw);
        });
        if (AIBinder_linkToDeath(mBinder.get(), mRecipient.get(),
                new std::weak_ptr<std::atomic<bool>>(dead)) != STATUS_OK) dead->store(true);
    }
private:
    ndk::SpAIBinder mBinder;
    ndk::ScopedAIBinder_DeathRecipient mRecipient;
};

HistogramService::HistogramService() : mWorker(&HistogramService::run, this) {}
HistogramService::~HistogramService() {
    { std::lock_guard lock(mMutex); mStop = true; mSession.release(); }
    mChanged.notify_all();
    if (mWorker.joinable()) mWorker.join();
}
void HistogramService::scene(bool known, bool hdr, int64_t token) {
    std::lock_guard lock(mMutex);
    if (token < mSceneToken) return;
    mSceneToken = token;
    mSession.scene(known, hdr);
    mChanged.notify_all();
}
void HistogramService::newSf(int64_t session, int64_t connection) {
    std::lock_guard lock(mMutex);
    mSession.newSf(session, connection);
    mSfDead.reset();
    mSfReaped = false;
    mChanged.notify_all();
}
void HistogramService::sfGuard(std::shared_ptr<std::atomic<bool>> dead,
        int64_t session, int64_t connection) {
    std::lock_guard lock(mMutex);
    // A blocked worker may finish attaching the previous SF's death recipient
    // after Binder already installed a newer listener. Do not replace its guard.
    // Scene revisions within one SF connection do not reject a valid guard.
    if (!mSession.sameSf(session, connection)) return;
    mSfDead = std::move(dead);
    mSfReaped = false;
    mChanged.notify_all();
}
void HistogramService::backend(std::shared_ptr<std::atomic<bool>> dead) {
    std::lock_guard lock(mMutex);
    mOemDead = std::move(dead);
    if (mOemDead) mSession.oemConnected();
    else mSession.oemLost();
    mNextCommand = 0;
    mChanged.notify_all();
}
void HistogramService::reapLocked() {
    if (mOemDead && mOemDead->load() && mSession.oemAvailable()) mSession.oemLost();
    if (mSfDead && mSfDead->load() && !mSfReaped) {
        mSfReaped = true;
        mSession.scene(false, false);
    }
    if (mClientDead && mClientDead->load() && !mClientReaped) {
        mClientReaped = true;
        mSession.release();
    }
    if (mPqDead && mPqDead->load() && !mPqReaped) {
        mPqReaped = true;
        mSession.invalidate();
    }
}

ndk::ScopedAStatus HistogramService::readSample(const ndk::SpAIBinder& client,
        int64_t request, bool enable, brightness::HistogramSample* out) {
    *out = {};
    out->request = request;
    if (!client.get() || request <= 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard lock(mMutex);
    reapLocked();
    if (mClient.get() != client.get()) {
        // A late release from the old client cannot stop a new client session.
        if (!enable) {
            out->generation = mSession.generation();
            out->sceneKnown = mSession.sceneKnown();
            out->hdr = mSession.hdr();
            return ndk::ScopedAStatus::ok();
        }
        mSession.release();
        mClient = client;
        mClientDead = std::make_shared<std::atomic<bool>>(false);
        mClientReaped = false;
        mClientWatch = std::make_unique<Watch>(client, mClientDead);
    }
    if (mClientDead->load()) return ndk::ScopedAStatus::ok();
    const int64_t now = nowMs();
    mSession.request(request, enable, now);
    out->generation = mSession.generation();
    out->sceneKnown = mSession.sceneKnown();
    out->hdr = mSession.hdr();
    out->active = mSession.readable(now);
    const auto sample = mSession.snapshot(now);
    out->readStartedAt = sample.readStartedAt;
    out->transportOk = sample.transportOk;
    out->shapeValid = sample.shapeValid;
    out->result = sample.result;
    out->frame = sample.frame;
    out->channel = sample.channel;
    out->format = sample.format;
    out->binCount = sample.binCount;
    out->bins.assign(sample.bins.begin(), sample.bins.end());
    mChanged.notify_all();
    return ndk::ScopedAStatus::ok();
}

void HistogramService::drive(
        aidl::vendor::xiaomi::hardware::displayfeature_aidl::IDisplayFeature& service,
        int64_t sceneToken) {
    std::optional<HistogramSession::Command> command;
    {
        std::lock_guard lock(mMutex);
        reapLocked();
        const int64_t now = nowMs();
        if (now < mNextCommand) return;
        command = mSession.command(now);
        // The worker may still be processing the previous SF scene while a
        // Binder thread already reported the new one. Cleanup is always safe;
        // enabling requires notifications for the same scene revision.
        if (command && command->enable && sceneToken != mSceneToken) return;
    }
    if (!command) return;
    // Factory feature 56; 0x90ff identifies client 9 with cookie 255.
    // Binder OK acknowledges a command, not a valid histogram measurement.
    const bool ok = service.setFeature(0, 56, command->enable ? 1 : 0, 0x90ff).isOk();
    {
        std::lock_guard lock(mMutex);
        reapLocked();
        mSession.complete(*command, ok);
        if (!ok) mNextCommand = nowMs() + 1000;
    }
    mChanged.notify_all();
}

void HistogramService::run() {
    StockPqReader reader(nowMs);
    std::shared_ptr<pq::IPictureQuality_AIDL> service;
    std::unique_ptr<Watch> watch;
    std::shared_ptr<std::atomic<bool>> dead;
    int64_t nextLookup = 0;
    auto nextRead = std::chrono::steady_clock::time_point{};
    while (true) {
        int64_t generation;
        {
            std::unique_lock lock(mMutex);
            // No polling/wakeups while OFF/DOZE, unrequested or unavailable.
            // Consumer renewals do not produce extra PQ reads when active.
            mChanged.wait(lock, [&] {
                reapLocked();
                return mStop || mSession.readable(nowMs());
            });
            mChanged.wait_until(lock, nextRead, [&] {
                reapLocked();
                return mStop || !mSession.readable(nowMs());
            });
            if (mStop) break;
            reapLocked();
            if (!mSession.readable(nowMs())) continue;
            generation = mSession.generation();
            nextRead = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        }
        if (service && dead->load()) { service.reset(); watch.reset(); }
        if (!service) {
            if (nowMs() < nextLookup) continue;
            nextLookup = nowMs() + 1000;
            ndk::SpAIBinder binder(AServiceManager_checkService(kPqService));
            if (!binder.get()) continue;
            auto candidate = pq::IPictureQuality_AIDL::fromBinder(binder);
            if (!candidate || !reader.compatible(*candidate)) continue;
            auto fresh = std::make_shared<std::atomic<bool>>(false);
            auto freshWatch = std::make_unique<Watch>(binder, fresh);
            if (fresh->load()) continue;
            service = std::move(candidate);
            dead = std::move(fresh);
            watch = std::move(freshWatch);
            {
                std::lock_guard lock(mMutex);
                mPqDead = dead;
                mPqReaped = false;
                mSession.invalidate(); // New PQ connection needs a fresh enabled session/baseline.
            }
            continue;
        }
        const auto sample = reader.read(*service); // No lock is held across vendor IPC.
        {
            std::lock_guard lock(mMutex);
            reapLocked();
            mSession.publish(generation, sample, nowMs());
            if (!sample.transportOk) mSession.invalidate();
        }
        if (!sample.transportOk) { service.reset(); watch.reset(); }
    }
}
} // namespace rothko::color
