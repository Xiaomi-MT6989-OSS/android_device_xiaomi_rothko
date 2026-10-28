// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "RothkoDisplayColor"
#if defined(__ANDROID_VENDOR__)
#error Device-private Binder stubs require the platform static compilation variant
#endif
#include "Controller.h"
#include "StockDisplayFeature.h"
#include "HistogramService.h"
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <android/binder_manager.h>
#include <chrono>
#include <fcntl.h>
#include <limits>
#include <stdio.h>
#include <ui/GraphicTypes.h>
#include <unistd.h>
#include <utility>

namespace rothko::color {
namespace df = aidl::vendor::xiaomi::hardware::displayfeature_aidl;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using android::ui::Dataspace;

constexpr const char* kOemService =
        "vendor.xiaomi.hardware.displayfeature_aidl.IDisplayFeature/default";
constexpr const char* kFrozenHash = "e57e364757bc7b63a3a7e586585c5827115966d7";
// Created by init (post-fs-data) and relabelled by the device policy.
constexpr const char* kDirectory = "/data/vendor/rothko_display_color";
// Service-specific errors surface through the frozen void AIDL setter and are
// mapped to failure by the Lineage SDK.
constexpr int kUnavailable = 1, kTimeout = 2, kPersistence = 3;
// The stock backend reloads a 62-table C3D LUT while it applies a profile, so
// the intent echo arrives seconds after the request, not immediately.
constexpr auto kRequestTimeout = 12000ms;
constexpr auto kApplyTimeout = 9000ms;
constexpr auto kRepublishTimeout = 1500ms;
constexpr auto kPollInterval = 25ms;
constexpr auto kLookupInterval = 1000ms;

constexpr int32_t ds(Dataspace value) { return static_cast<int32_t>(value); }

struct Mode {
    int32_t id;
    const char* name;
    int32_t feature;
    int32_t intent;
};
// OEM user-mode table. Ids are the stable identifiers exposed to Settings; the
// feature/value pair and the render intent are the observed stock mapping.
constexpr Mode kModes[] = {
        {3, "Original", 2, 257},
        {1, "Vivid", 0, 258},
        {2, "Saturated", 1, 256},
};
static const Mode* findMode(int32_t id) {
    for (const auto& mode : kModes) {
        if (mode.id == id) return &mode;
    }
    return nullptr;
}
static live::DisplayMode parcelMode(int32_t id) {
    live::DisplayMode value;
    value.id = id;
    if (const auto* mode = findMode(id)) value.name = mode->name;
    return value;
}
static ndk::ScopedAStatus unavailable(int error) {
    return ndk::ScopedAStatus::fromServiceSpecificError(error);
}
// A user mode may only be requested while real SDR composition is on screen:
// connected primary, accepted power transition to On, managed content and no
// HDR candidate. HDR/HLG is a separate OEM notification, never a user mode.
static bool sdrReady(const bridge::DisplayState& s) {
    return s.connected && s.powerKnown && s.accepted && s.lastAttempted == 2 && s.contentKnown &&
           s.enabled && s.managed && s.hdrCandidate == ds(Dataspace::UNKNOWN);
}
// Generation (sfSession/connection) is verified separately; this compares only
// the content and power inputs that decide the OEM notification batch.
static bool sameScene(const bridge::DisplayState& a, const bridge::DisplayState& b) {
    return a.physicalId == b.physicalId && a.connected == b.connected &&
           a.powerKnown == b.powerKnown && a.accepted == b.accepted &&
           a.lastAttempted == b.lastAttempted && a.contentKnown == b.contentKnown &&
           a.enabled == b.enabled && a.managed == b.managed &&
           a.hdrCandidate == b.hdrCandidate && a.outputCandidate == b.outputCandidate &&
           a.forceHdrClientComposition == b.forceHdrClientComposition;
}
static bool sameGeneration(const bridge::DisplayState& s, int64_t session, int64_t connection) {
    return s.sfSession == session && s.connection == connection;
}

struct Observation {
    std::shared_ptr<std::atomic<bool>> dead = std::make_shared<std::atomic<bool>>(false);
    std::atomic<uint64_t> intents{0};
    std::atomic<int32_t> lastIntent{-1};
    std::atomic<int32_t> wcg{-1};
    // Bumped only for a contract-valid value (0/1) so the worker can republish
    // a policy revision when the gamut state changes after the mode confirm.
    std::atomic<uint64_t> wcgVersion{0};
    std::atomic<uint64_t> invalidWcg{0};
};

class Callback final : public df::BnDisplayFeatureCallback {
public:
    explicit Callback(std::shared_ptr<Observation> observation)
          : mObservation(std::move(observation)) {}
    ndk::ScopedAStatus displayfeatureInfoChanged(int32_t event, int32_t value, float, float,
                                                 float) override {
        // The OEM broadcasts while holding its own global mutex. Only bounded
        // atomics are recorded: no command mutex, Binder call, wait or payload log.
        if (event == 30000) {
            mObservation->lastIntent.store(value, std::memory_order_relaxed);
            mObservation->intents.fetch_add(1, std::memory_order_release);
        } else if (event == 10000) {
            if (value == 0 || value == 1) {
                mObservation->wcg.store(value, std::memory_order_relaxed);
                mObservation->wcgVersion.fetch_add(1, std::memory_order_release);
            } else {
                // Out-of-contract values are dropped, never promoted into policy
                // and never used to tear down an otherwise healthy generation.
                mObservation->invalidWcg.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return ndk::ScopedAStatus::ok();
    }

private:
    std::shared_ptr<Observation> mObservation;
};

// Links a binder death notification to a process-lifetime flag. The cookie is
// released by onUnlinked, which Binder calls exactly once per successful link
// and after any pending death callback.
class DeathWatch {
public:
    DeathWatch(ndk::SpAIBinder binder, const std::shared_ptr<std::atomic<bool>>& dead)
          : mBinder(std::move(binder)),
            mRecipient(AIBinder_DeathRecipient_new([](void* raw) {
                auto* cookie = static_cast<std::weak_ptr<std::atomic<bool>>*>(raw);
                if (auto value = cookie->lock()) value->store(true);
            })) {
        AIBinder_DeathRecipient_setOnUnlinked(mRecipient.get(), [](void* raw) {
            delete static_cast<std::weak_ptr<std::atomic<bool>>*>(raw);
        });
        if (AIBinder_linkToDeath(mBinder.get(), mRecipient.get(),
                                 new std::weak_ptr<std::atomic<bool>>(dead)) != STATUS_OK) {
            dead->store(true);
        }
    }

private:
    ndk::SpAIBinder mBinder;
    ndk::ScopedAIBinder_DeathRecipient mRecipient;
};

Controller::Controller() : mHistogram(ndk::SharedRefBase::make<HistogramService>()) {
    // Read the persisted default before the worker starts. Only a single digit
    // followed by a newline and a known mode id is accepted.
    android::base::unique_fd dir(open(kDirectory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (dir.ok()) {
        android::base::unique_fd fd(
                openat(dir.get(), "default", O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        char bytes[8] = {};
        if (fd.ok()) {
            const ssize_t n = read(fd.get(), bytes, sizeof(bytes));
            if (n == 2 && bytes[1] == '\n' && findMode(bytes[0] - '0')) {
                mDefault.store(bytes[0] - '0');
            }
        }
    }
    mWorker = std::thread(&Controller::run, this);
}

Controller::~Controller() {
    mStop.store(true);
    mChanged.notify_all();
    if (mWorker.joinable()) mWorker.join();
    std::lock_guard lock(mMutex);
    if (mDozeRequest) mDozeRequest->completion.set_value(false);
}

bool Controller::applyDozeBrightness(int64_t physicalDisplayId, int32_t mode) {
    // Pinned built-in panel; never route a secondary display to OEM display 0.
    if (physicalDisplayId != 4627039422300187648LL || mode < 0 || mode > 2) return false;
    auto request = std::make_shared<DozeRequest>();
    request->mode = mode;
    auto completion = request->completion.get_future();
    {
        std::lock_guard lock(mMutex);
        if (mStop.load() || mDozeRequest) return false;
        mDozeRequest = request;
    }
    mChanged.notify_all();
    // Ordered with actual display power work. No timeout that lets an in-flight
    // OEM write complete after the caller has advanced to a newer power state.
    return completion.get();
}

ndk::ScopedAStatus Controller::setListener(
        int64_t session, int64_t connection,
        const std::shared_ptr<bridge::IColorPolicyCallback>& listener) {
    if (session <= 0 || connection <= 0 || !listener) return unavailable(kUnavailable);
    std::shared_ptr<Request> cancelled;
    {
        std::lock_guard lock(mMutex);
        if (mGeneration == std::numeric_limits<uint64_t>::max()) return unavailable(kUnavailable);
        ++mGeneration;
        cancelled = std::exchange(mActiveRequest, nullptr);
        if (cancelled) cancelled->abandoned.store(true);
        mApplyingRequest.reset();
        mListener = listener;
        mSession = session;
        mConnection = connection;
        mLastSequence = 0;
        mLastPublished = 0;
        mState.reset();
        mRequest.reset();
        mReady.store(false);
        mCurrent.store(-1);
        mReportLogged = false;
        mHistogram->newSf(session, connection);
        mHistogram->scene(false, false, ++mSceneRevision);
    }
    // Wake the replaced caller without waiting for the OEM worker to return.
    // finish() is identity-checked and must run outside mMutex.
    finish(cancelled, false, kUnavailable);
    LOG(INFO) << "listener attached session=" << session << " connection=" << connection;
    mChanged.notify_all();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Controller::reportState(const bridge::DisplayState& state) {
    std::lock_guard lock(mMutex);
    if (state.sfSession == mSession && state.connection == mConnection &&
        state.sequence > mLastSequence) {
        mLastSequence = state.sequence;
        if (!mState || !sameScene(*mState, state) ||
                mState->sfSession != state.sfSession || mState->connection != state.connection ||
                mState->synchronizedState != state.synchronizedState) {
            const bool known = state.synchronizedState && state.connected && state.powerKnown &&
                    state.accepted && state.lastAttempted == 2 && state.contentKnown &&
                    state.enabled && state.managed;
            mHistogram->scene(known, state.hdrCandidate != ds(Dataspace::UNKNOWN), ++mSceneRevision);
        }
        mState = state;
        if (!mReportLogged) {
            mReportLogged = true;
            LOG(INFO) << "first state: connected=" << state.connected
                      << " power=" << (state.powerKnown ? state.lastAttempted : -1)
                      << " accepted=" << state.accepted
                      << " content=" << (state.contentKnown ? 1 : 0)
                      << " managed=" << state.managed
                      << " profileKnown=" << state.profileKnown;
        }
        mChanged.notify_all();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Controller::setMode(int32_t mode, bool makeDefault) {
    if (!findMode(mode)) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    auto request = std::make_shared<Request>();
    request->mode = mode;
    request->makeDefault = makeDefault;
    {
        std::lock_guard lock(mMutex);
        if (mStop.load() || !mReady.load() || mActiveRequest) {
            return unavailable(kUnavailable);
        }
        request->generation = mGeneration;
        request->connection = mConnection;
        mActiveRequest = request;
        mRequest = request;
    }
    LOG(INFO) << "setMode " << mode << " accepted";
    mChanged.notify_all();
    std::unique_lock lock(request->mutex);
    if (!request->changed.wait_for(lock, kRequestTimeout, [&] { return request->done; })) {
        lock.unlock();
        abandonFromCaller(request, mode);
        return unavailable(kTimeout);
    }
    return request->ok ? ndk::ScopedAStatus::ok() : unavailable(request->error);
}

void Controller::abandonFromCaller(const std::shared_ptr<Request>& request, int32_t modeId) {
    // A timeout is uncertain: it is not cancellation, and a late callback cannot
    // be attributed to this request. Record the intent so a later confirmation of
    // the very same value is refused rather than silently accepted.
    // Clear the SurfaceFlinger-side policy from this thread. The OEM worker may
    // be blocked inside a vendor call and must not be required to make progress.
    {
        std::lock_guard lock(mMutex);
        if (request->generation != mGeneration || mActiveRequest != request) return;
        request->abandoned.store(true);
        const Mode* mode = findMode(modeId);
        if (mode && mApplyingRequest == request) mUnresolvedIntent.store(mode->intent);
        mActiveRequest.reset();
        mReady.store(false);
        // The worker may remain blocked after this caller has timed out.
        // Its eventual cleanup must not be needed to invalidate stale current.
        mCurrent.store(-1);
    }
    publishPolicy(request->generation, request->connection,
                  mPolicySequence.fetch_add(1), -1, true);
    mChanged.notify_all();
    LOG(WARNING) << "Display mode request abandoned; OEM outcome unknown";
}

void Controller::finish(const std::shared_ptr<Request>& request, bool ok, int error) {
    if (!request) return;
    {
        std::lock_guard lock(request->mutex);
        if (request->done) return;
        request->ok = ok;
        request->error = error;
        request->done = true;
    }
    {
        std::lock_guard lock(mMutex);
        if (mActiveRequest == request) mActiveRequest.reset();
        if (mApplyingRequest == request) mApplyingRequest.reset();
    }
    request->changed.notify_all();
}

bool Controller::publishPolicy(uint64_t generation, int64_t connection, int64_t request,
                               int32_t intent, bool wcg) {
    std::shared_ptr<bridge::IColorPolicyCallback> listener;
    {
        std::lock_guard lock(mMutex);
        if (generation != mGeneration || connection != mConnection ||
                request <= mLastPublished) return false;
        mLastPublished = request;
        listener = mListener;
    }
    if (!listener || !listener->onPolicy(connection, request, intent, wcg).isOk()) {
        std::lock_guard lock(mMutex);
        if (generation == mGeneration) mReady.store(false);
        return false;
    }
    return true;
}

bool Controller::persistDefault(int32_t mode, const std::function<bool()>& aborted) {
    android::base::unique_fd dir(open(kDirectory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!dir.ok()) return false;
    android::base::unique_fd fd(openat(dir.get(), "default.new",
                                       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                                       0600));
    const std::string value = std::to_string(mode) + "\n";
    if (!fd.ok() || !android::base::WriteFully(fd.get(), value.data(), value.size())) return false;
    if (fsync(fd.get())) {
        unlinkat(dir.get(), "default.new", 0);
        return false;
    }
    // The caller may have timed out while the write was blocked in the kernel.
    // Recheck before the rename so an abandoned request cannot commit a change.
    if (aborted && aborted()) {
        unlinkat(dir.get(), "default.new", 0);
        return false;
    }
    if (renameat(dir.get(), "default.new", dir.get(), "default")) {
        unlinkat(dir.get(), "default.new", 0);
        return false;
    }
    // The rename is committed and visible. A late timeout is reported as failure
    // without pretending the visible change was rolled back.
    mDefault.store(mode);
    return fsync(dir.get()) == 0;
}

void Controller::run() {
    std::shared_ptr<df::IDisplayFeature> backend;
    std::shared_ptr<Observation> observation;
    std::shared_ptr<Callback> callback;
    std::unique_ptr<DeathWatch> backendDeath;
    std::unique_ptr<DeathWatch> sfDeath;
    std::shared_ptr<std::atomic<bool>> sfDead;

    std::optional<bridge::DisplayState> previous;   // last OEM notification batch
    std::optional<bridge::DisplayState> applyScene; // scene when the apply started
    std::shared_ptr<Request> pending;
    std::shared_ptr<Request> nextQueued;
    const Mode* applying = nullptr;
    int64_t pendingPolicy = 0;
    int32_t pendingIntent = 0;
    int32_t confirmedMode = -1;
    int32_t confirmedIntent = 0;
    uint64_t publishedWcgVersion = 0;
    int64_t activeSession = 0, activeConnection = 0;
    uint64_t activeGeneration = 0;
    uint64_t intentBoundary = 0;
    bool restore = true, policyActive = false;
    int32_t previousPower = -1;
    Clock::time_point deadline{}, nextLookup{}, nextSceneAttempt{};

    // Registration can change while this worker is blocked in OEM IPC. State
    // from the old iteration must not become the new connection's ready/current.
    auto setReady = [&](bool ready) {
        std::lock_guard lock(mMutex);
        if (activeGeneration == mGeneration) mReady.store(ready);
    };
    auto setCurrent = [&](int32_t mode) {
        std::lock_guard lock(mMutex);
        if (activeGeneration == mGeneration) mCurrent.store(mode);
    };
    auto setApplying = [&](const std::shared_ptr<Request>& request) {
        std::lock_guard lock(mMutex);
        if (activeGeneration == mGeneration) mApplyingRequest = request;
    };
    auto currentGeneration = [&]() {
        std::lock_guard lock(mMutex);
        return activeGeneration == mGeneration;
    };
    auto clearPolicyNow = [&]() {
        if (!policyActive) return;
        policyActive = false;
        pendingPolicy = 0;
        publishPolicy(activeGeneration, activeConnection, mPolicySequence.fetch_add(1), -1, true);
    };
    // `unattributed` is true only when an OEM mode request was actually issued
    // and its callback can no longer be matched to it.
    auto failPending = [&](int error, bool unattributed) {
        if (pending || applying) {
            LOG(WARNING) << "mode apply failed reason=" << error
                         << (unattributed ? " (unattributed)" : "");
        }
        if (pending) {
            finish(pending, false, error);
            pending = nullptr;
        }
        if (applying) {
            if (unattributed) mUnresolvedIntent.store(applying->intent);
            applying = nullptr;
        }
        setApplying(nullptr);
        applyScene.reset();
        setCurrent(-1);
        setReady(false);
        pendingPolicy = 0;
        confirmedMode = -1;
        confirmedIntent = 0;
        publishedWcgVersion = 0;
        clearPolicyNow();
    };

    uint64_t oemFailures = 0;
    while (!mStop.load()) {
        std::optional<bridge::DisplayState> state;
        std::shared_ptr<bridge::IColorPolicyCallback> listener;
        std::shared_ptr<Request> queued;
        std::shared_ptr<DozeRequest> doze;
        int64_t session = 0, connection = 0, sceneRevision = 0;
        uint64_t generation = 0;
        {
            std::unique_lock lock(mMutex);
            mChanged.wait_for(lock, kPollInterval);
            state = mState;
            listener = mListener;
            session = mSession;
            connection = mConnection;
            generation = mGeneration;
            sceneRevision = mSceneRevision;
            queued = std::exchange(mRequest, nullptr);
            doze = std::exchange(mDozeRequest, nullptr);
        }

        // 1. Controlled generation. A restarted SurfaceFlinger can reuse the same
        //    connection ordinal, so the session identity is part of the key.
        if (generation != activeGeneration || !listener) {
            if (pending || applying) failPending(kUnavailable, applying != nullptr);
            finish(std::exchange(nextQueued, nullptr), false, kUnavailable);
            activeGeneration = generation;
            activeSession = session;
            activeConnection = connection;
            previous.reset();
            nextSceneAttempt = {};
            applyScene.reset();
            pendingPolicy = 0;
            confirmedMode = -1;
            confirmedIntent = 0;
            publishedWcgVersion = 0;
            policyActive = false;
            restore = true;
            previousPower = -1;
            sfDeath.reset();
            sfDead = std::make_shared<std::atomic<bool>>(false);
            if (session > 0 && connection > 0 && listener) {
                sfDeath = std::make_unique<DeathWatch>(listener->asBinder(), sfDead);
            }
            mHistogram->sfGuard(sfDead, session, connection);
        }

        // 2. Accept at most one user request per generation, and only while the
        //    mode's OEM intent is not already unattributable.
        if (queued) {
            const Mode* mode = findMode(queued->mode);
            const bool ambiguous = mode && mUnresolvedIntent.load() == mode->intent;
            if (queued->abandoned.load() || queued->generation != activeGeneration ||
                    ambiguous || !state || !sdrReady(*state)) {
                LOG(WARNING) << "queued mode " << queued->mode << " rejected: ambiguous="
                             << ambiguous << " state=" << (state ? 1 : 0)
                             << " sdrReady=" << (state ? sdrReady(*state) : 0);
                finish(queued, false, kUnavailable);
            } else if (pending) {
                // A newer selection supersedes one that has not started yet.
                finish(pending, false, kUnavailable);
                pending = queued;
            } else if (applying) {
                // Queue behind the write in progress instead of failing the tap.
                if (nextQueued) finish(nextQueued, false, kUnavailable);
                nextQueued = queued;
            } else {
                pending = queued;
            }
        }

        if (!applying && !pending && nextQueued) {
            pending = std::exchange(nextQueued, nullptr);
        }

        if (pending && pending->abandoned.load()) {
            failPending(kTimeout, applying != nullptr);
        }

        // 3. OEM backend death invalidates the whole generation of correlation.
        if (observation && observation->dead->load()) {
            mHistogram->backend(nullptr);
            if (applying) failPending(kUnavailable, true);
            backendDeath.reset();
            callback.reset();
            observation.reset();
            backend.reset();
            previous.reset();
            restore = true;
            setReady(false);
            setCurrent(-1);
            clearPolicyNow();
        }

        // 4. One bounded lookup attempt per interval, one registration per
        //    generation. An OK registration alone does not prove a slot.
        if (!backend && Clock::now() >= nextLookup) {
            nextLookup = Clock::now() + kLookupInterval;
            ndk::SpAIBinder binder(AServiceManager_checkService(kOemService));
            if (!binder.get()) {
                const uint64_t attempt = ++oemFailures;
                if (attempt == 1 || attempt % 120 == 0) {
                    LOG(WARNING) << "display feature service not found: " << kOemService
                                 << " attempt=" << attempt;
                }
            }
            if (binder.get()) {
                auto candidate = df::IDisplayFeature::fromBinder(binder);
                auto fresh = std::make_shared<Observation>();
                auto watch = std::make_unique<DeathWatch>(binder, fresh->dead);
                auto freshCallback = ndk::SharedRefBase::make<Callback>(fresh);
                int32_t version = -1, callbackVersion = -1;
                std::string hash, callbackHash;
                const bool valid = candidate &&
                        candidate->getInterfaceVersion(&version).isOk() && version == 2 &&
                        candidate->getInterfaceHash(&hash).isOk() && hash == kFrozenHash &&
                        freshCallback->getInterfaceVersion(&callbackVersion).isOk() &&
                        callbackVersion == version &&
                        freshCallback->getInterfaceHash(&callbackHash).isOk() &&
                        callbackHash == hash &&
                        candidate->registerCallback(0, freshCallback).isOk();
                if (!valid) {
                    const uint64_t attempt = ++oemFailures;
                    if (attempt == 1 || attempt % 120 == 0) {
                        LOG(WARNING) << "display feature registration failed: candidate="
                                     << (candidate != nullptr) << " version=" << version
                                     << " hash_ok=" << (hash == kFrozenHash)
                                     << " callback_version=" << callbackVersion
                                     << " callback_hash_ok=" << (callbackHash == hash)
                                     << " attempt=" << attempt;
                    }
                }
                if (valid) {
                    backend = std::move(candidate);
                    observation = std::move(fresh);
                    callback = std::move(freshCallback);
                    backendDeath = std::move(watch);
                    mHistogram->backend(observation->dead);
                    previous.reset();
                    restore = true;
                }
            }
        }

        // Dedicated doze backlight belongs to LocalDisplayAdapter's ordered
        // power work, including the request just BEFORE entering suspend. It
        // cannot be gated on a later SF scene report or an SDR colour policy.
        // This remains the sole OEM command worker, shared with colour/PQ.
        if (doze) {
            const bool ok = backend && observation && !observation->dead->load()
                    && backend->setFeature(0, 25, doze->mode, 255).isOk()
                    && !observation->dead->load();
            doze->completion.set_value(ok);
        }

        // 5. A missing/dead SF scene cannot authorize either colour policy or
        //    sampling. Never dereference an empty optional during initial boot.
        if (!backend || !listener || !state || (sfDead && sfDead->load())) {
            // Also perform histogram cleanup when the scene disappears; it
            // does not depend on a colour request or a pending mode change.
            if (backend) mHistogram->drive(*backend, sceneRevision);
            setReady(false);
            if (pending && !applying) {
                finish(pending, false, kUnavailable);
                pending = nullptr;
            }
            if (applying) {
                failPending(kUnavailable, true);
                continue;
            }
            clearPolicyNow();
            continue;
        }

        // 6. An earlier unattributed request is resolved once the OEM is
        //    observed at a different intent.
        if (mUnresolvedIntent.load() >= 0) {
            const int32_t seen = observation->lastIntent.load();
            if (seen >= 0 && seen != mUnresolvedIntent.load()) mUnresolvedIntent.store(-1);
        }

        // 7. A real Off -> On power transition re-initialises the OEM, so an
        //    unattributed request can no longer be confused with a later one.
        if (state && state->powerKnown && state->accepted && previousPower == 1 &&
            state->lastAttempted == 2) {
            mUnresolvedIntent.store(-1);
        }

        // 8. Push the full composition/power notification batch on any change.
        const bool sceneChanged = state && (!previous || !sameScene(*previous, *state));
        if (applying && sceneChanged) {
            // The scene moved while a user mode request was outstanding. The
            // request is no longer meaningful and its callback is unattributable.
            failPending(kUnavailable, true);
            continue;
        }
        if (sceneChanged) {
            // Failed IPC must not mark this scene as delivered or allow a mode
            // write to bypass its prerequisite notifications. Reuse the backend
            // recovery cadence rather than retrying on every worker poll.
            if (Clock::now() < nextSceneAttempt) continue;
            bool ok = true;
            auto feature = [&](int32_t id, int32_t value) {
                if (ok) ok = backend->setFeature(0, id, value, 0).isOk();
            };
            if (state->powerKnown && state->accepted &&
                (!previous || !previous->powerKnown ||
                 previous->lastAttempted != state->lastAttempted)) {
                // Hook enum Off/On/Doze/DozeSuspend -> native 0/1/2/3. Only the
                // actually accepted transition is reported; DOZE falling back to
                // ON arrives here as On.
                const int32_t native[] = {0, 0, 1, 2, 3};
                if (state->lastAttempted >= 1 && state->lastAttempted <= 4) {
                    ok = backend->sendMessage(13, native[state->lastAttempted], "").isOk();
                }
            }
            if (state->connected && state->contentKnown && state->enabled && state->managed) {
                const bool hdr = state->hdrCandidate != ds(Dataspace::UNKNOWN);
                const bool hlg = state->hdrCandidate == ds(Dataspace::BT2020_HLG) ||
                                 state->hdrCandidate == ds(Dataspace::BT2020_ITU_HLG);
                const bool srgb = state->outputCandidate == ds(Dataspace::V0_SRGB);
                const bool p3 = state->outputCandidate == ds(Dataspace::DISPLAY_P3);
                if (p3 && !hdr) {
                    feature(318, 3);
                } else if (!p3 && !srgb &&
                           (!previous || previous->outputCandidate != state->outputCandidate)) {
                    feature(318, 1);
                }
                feature(10, hdr ? 1 : 0);
                feature(64, hlg ? 1 : 0);
                if (srgb) feature(318, 1);
            }
            restore = true;
            if (!ok) {
                previous.reset();
                nextSceneAttempt = Clock::now() + kLookupInterval;
                mHistogram->scene(false, false, sceneRevision);
                failPending(kUnavailable, false);
                continue;
            }
            previous = state;
            nextSceneAttempt = {};
            {
                // Failure disabled sampling. Restore its scene only if no
                // newer report/registration has superseded this batch.
                std::lock_guard lock(mMutex);
                if (activeGeneration == mGeneration && sceneRevision == mSceneRevision) {
                    const bool known = state->synchronizedState && state->connected &&
                            state->powerKnown && state->accepted && state->lastAttempted == 2 &&
                            state->contentKnown && state->enabled && state->managed;
                    mHistogram->scene(known, state->hdrCandidate != ds(Dataspace::UNKNOWN),
                                      sceneRevision);
                }
            }
        }
        if (state->powerKnown) previousPower = state->lastAttempted;
        mHistogram->drive(*backend, sceneRevision);

        // 9. A user mode may only be requested while real SDR content is up.
        //    Without an SF observation there is no content signal to gate on.
        if (state && !sdrReady(*state)) {
            setReady(false);
            clearPolicyNow();
            if (pending) {
                finish(pending, false, kUnavailable);
                pending = nullptr;
            }
            if (applying) {
                failPending(kUnavailable, true);
                continue;
            }
            continue;
        }

        // 10. Start a user request or re-drive the OEM baseline.
        if (!applying && (pending || restore)) {
            if (pending && pending->abandoned.load()) {
                finish(pending, false, kTimeout);
                pending = nullptr;
                continue;
            }
            const int32_t desired =
                    pending ? pending->mode
                            : (mCurrent.load() > 0 ? mCurrent.load() : mDefault.load());
            if (pending && !restore && desired == mCurrent.load()) {
                // The mode is already confirmed; only the default may change.
                const bool ok = !pending->makeDefault ||
                        persistDefault(desired, [&] { return pending->abandoned.load(); });
                finish(pending, ok, kPersistence);
                pending = nullptr;
                continue;
            }
            applying = findMode(desired);
            if (applying) {
                LOG(INFO) << (pending ? "applying requested mode " : "restoring default mode ")
                          << desired;
            }
            if (!applying) {
                if (pending) {
                    finish(pending, false, kUnavailable);
                    pending = nullptr;
                }
                restore = false;
                continue;
            }
            intentBoundary = observation->intents.load(std::memory_order_acquire);
            pendingPolicy = 0;
            pendingIntent = applying->intent;
            deadline = Clock::now() + kApplyTimeout;
            setApplying(pending);
            setReady(false);
            applyScene = state;
            if (!currentGeneration() || (pending && pending->abandoned.load())) {
                failPending(kUnavailable, false);
                continue;
            }
            if (!backend->setFeature(0, applying->feature, 2, 255).isOk()) {
                failPending(kUnavailable, true);
                continue;
            }
            if (!currentGeneration() || (pending && pending->abandoned.load())) {
                failPending(kUnavailable, true);
                continue;
            }
            restore = false;
        }

        if (applying) {
            // 11. Confirm the OEM mode echo, then the SF/HWC acceptance.
            // Binder OK only acknowledges dispatch into the asynchronous stock
            // pipeline. Elapsed time must not confirm or persist a user mode.
            if (Clock::now() >= deadline) {
                // A restore timeout must not poison attribution for the user's
                // next request of the same mode.
                failPending(kTimeout, pending != nullptr);
                continue;
            }
            const bool echoedNow =
                    observation->intents.load(std::memory_order_acquire) > intentBoundary &&
                    observation->lastIntent.load() == pendingIntent &&
                    mUnresolvedIntent.load() != pendingIntent;
            if (!pendingPolicy && echoedNow && observation->wcg.load() >= 0) {
                pendingPolicy = mPolicySequence.fetch_add(1);
                // A failed Binder transaction does not prove the peer observed
                // nothing. Clear the attempted policy through normal failure
                // cleanup, without waiting for an acknowledgement it cannot give.
                policyActive = true;
                if (!publishPolicy(activeGeneration, activeConnection, pendingPolicy,
                                   pendingIntent, observation->wcg.load() == 1)) {
                    failPending(kUnavailable, true);
                    continue;
                }
            }
            if (pendingPolicy && applyScene &&
                sameGeneration(*state, activeSession, activeConnection) &&
                sameScene(*applyScene, *state)) {
                if (state->profileRejected && state->policyRequestId == pendingPolicy) {
                    failPending(kUnavailable, true);
                    continue;
                }
                if (state->profileKnown && state->policyRequestId == pendingPolicy) {
                    if (state->hwcResult != 0 || state->renderIntent != pendingIntent ||
                        state->dataspace == ds(Dataspace::UNKNOWN)) {
                        failPending(kUnavailable, true);
                        continue;
                    }
                    if (!state->synchronizedState) continue;
                    LOG(INFO) << "mode " << applying->id << " confirmed";
                    setCurrent(applying->id);
                    bool ok = true;
                    if (pending && pending->makeDefault && !pending->abandoned.load()) {
                        ok = persistDefault(applying->id,
                                            [&] { return pending->abandoned.load(); });
                    }
                    confirmedMode = applying->id;
                    confirmedIntent = pendingIntent;
                    publishedWcgVersion = observation->wcgVersion.load();
                    finish(pending, ok, kPersistence);
                    pending = nullptr;
                    applying = nullptr;
                    applyScene.reset();
                    setApplying(nullptr);
                    policyActive = false;
                    pendingPolicy = 0;
                    setReady(true);
                }
            }
            continue;
        }

        // 12. The OEM gamut can change after software confirmation. Publish
        //     the same confirmed intent without reopening the completed request.
        if (!pending && confirmedMode >= 0 && previous && sameScene(*previous, *state) &&
            sameGeneration(*state, activeSession, activeConnection)) {
            const uint64_t version = observation->wcgVersion.load();
            const int32_t wcg = observation->wcg.load();
            if (!pendingPolicy && version != publishedWcgVersion && (wcg == 0 || wcg == 1)) {
                pendingPolicy = mPolicySequence.fetch_add(1);
                pendingIntent = confirmedIntent;
                applyScene = state;
                deadline = Clock::now() + kRepublishTimeout;
                publishedWcgVersion = version;
                policyActive = true;
                if (!publishPolicy(activeGeneration, activeConnection, pendingPolicy,
                                   confirmedIntent, wcg == 1)) {
                    failPending(kUnavailable, false);
                    continue;
                }
            }
            if (pendingPolicy && applyScene &&
                sameGeneration(*state, activeSession, activeConnection)) {
                const bool rejected =
                        state->profileRejected && state->policyRequestId == pendingPolicy;
                const bool observed =
                        state->profileKnown && state->policyRequestId == pendingPolicy;
                // These post-acceptance updates do not reopen the completed
                // caller request; any terminal outcome stops tracking them.
                if (rejected || observed || Clock::now() >= deadline) {
                    pendingPolicy = 0;
                    policyActive = false;
                }
            }
        }
        setReady(true);
    }
}

ndk::ScopedAStatus DisplayModes::getDisplayModes(std::vector<live::DisplayMode>* modes) {
    modes->clear();
    for (const auto& mode : kModes) modes->push_back(parcelMode(mode.id));
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus DisplayModes::getCurrentDisplayMode(live::DisplayMode* mode) {
    // A saved preference is not an observation of the current generation.
    // The SDK/UI already handle unavailable state and offer an explicit retry.
    const int32_t id = mController->currentMode();
    if (!findMode(id)) return unavailable(kUnavailable);
    *mode = parcelMode(id);
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus DisplayModes::getDefaultDisplayMode(live::DisplayMode* mode) {
    *mode = parcelMode(mController->defaultMode());
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus DisplayModes::setDisplayMode(int32_t mode, bool makeDefault) {
    return mController->setMode(mode, makeDefault);
}
ndk::ScopedAStatus DozeBrightness::apply(int64_t physicalDisplayId, int32_t mode, bool* applied) {
    *applied = false;
    if (AIBinder_getCallingUid() != 1000) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    *applied = mController->applyDozeBrightness(physicalDisplayId, mode);
    return ndk::ScopedAStatus::ok();
}
} // namespace rothko::color
