/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RothkoDisplayColorProvider"

#include <compositionengine/DisplayColorExtension.h>
#include <aidl/vendor/xiaomi/rothko/display/color/BnColorPolicyCallback.h>
#include <aidl/vendor/xiaomi/rothko/display/color/DisplayState.h>
#include <aidl/vendor/xiaomi/rothko/display/color/IRothkoDisplayColor.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace android::compositionengine {
namespace {

namespace transport = aidl::vendor::xiaomi::rothko::display::color;
using namespace std::chrono_literals;
constexpr char kService[] =
        "vendor.xiaomi.rothko.display.color.IRothkoDisplayColor/default";
constexpr size_t kPolicyQueueCapacity = 16;

struct PolicyMessage {
    int64_t connection = 0;
    int64_t requestId = 0;
    int32_t intent = -1;
    bool wcg = true;
};

// The OEM proxy never enters this process. This callback is the device-private
// controller transport and only enqueues bounded local messages.
class PolicyCallback final : public transport::BnColorPolicyCallback {
public:
    using Enqueue = std::function<void(const PolicyMessage&)>;
    PolicyCallback(int64_t generation, Enqueue enqueue)
          : mGeneration(generation), mEnqueue(std::move(enqueue)) {}

    ndk::ScopedAStatus onPolicy(int64_t connection, int64_t requestId, int32_t intent,
                                bool wideColorGamut) override {
        if (connection == mGeneration) {
            mEnqueue({connection, requestId, intent, wideColorGamut});
        }
        return ndk::ScopedAStatus::ok();
    }

private:
    const int64_t mGeneration;
    const Enqueue mEnqueue;
};

class RothkoDisplayColorProvider final : public DisplayColorExtension {
public:
    RothkoDisplayColorProvider()
          : mSfSession(std::chrono::steady_clock::now().time_since_epoch().count() &
                       std::numeric_limits<int64_t>::max()),
            mDeathRecipient(AIBinder_DeathRecipient_new(&RothkoDisplayColorProvider::onDeath)) {
        AIBinder_DeathRecipient_setOnUnlinked(mDeathRecipient.get(), [](void* opaque) {
            delete static_cast<DeathCookie*>(opaque);
        });
        ALOGI("Display colour provider constructed");
        // Process-lifetime provider; no thread, object or callback teardown runs
        // while SF/composition locks are held.
        std::thread([this] { run(); }).detach();
    }

    bool isEnabled() const override { return true; }

    void setInvalidateCallback(std::function<void()> invalidate) override {
        // Called during SF initialization, before the provider is used by CE.
        // A contended install must not park the worker forever: the callback
        // only triggers a repaint, while the controller link carries policy.
        {
            std::unique_lock lock(mStateMutex, std::try_to_lock);
            if (lock.owns_lock()) {
                mInvalidate = std::move(invalidate);
            } else {
                loseState();
                ALOGW("Invalidate callback install contended; starting without it");
            }
        }
        mStarted.store(true, std::memory_order_release);
    }

    void onDisplayConnected(PhysicalDisplayId id, bool internal) override {
        if (!internal || mPrimaryConnected.load(std::memory_order_acquire)) return;
        // Preserve the connect identity even if the coalesced snapshot lock is
        // contended. Overflow recovery can then rebuild identity as unknown
        // content/power rather than permanently losing the only connect event.
        mPrimaryId.store(id.value, std::memory_order_release);
        mPrimaryConnected.store(true, std::memory_order_release);
        update([&](auto& state) {
            // rothko has one internal panel. Never silently select a second
            // internal/external/virtual display while the first is connected.
            if (state.connected && state.physicalId != static_cast<int64_t>(id.value)) return;
            const auto sequence = state.sequence;
            state = {};
            state.sequence = sequence;
            state.physicalId = static_cast<int64_t>(id.value);
            state.connected = true;
            mPrimaryId.store(id.value, std::memory_order_release);
            mPrimaryConnected.store(true, std::memory_order_release);
            mPolicyEnabled.store(false, std::memory_order_release);
        });
    }

    void onDisplayDisconnected(PhysicalDisplayId id) override {
        if (!isPrimary(id)) return;
        mPrimaryConnected.store(false, std::memory_order_release);
        mPolicyEnabled.store(false, std::memory_order_release);
        update([&](auto& state) {
            state.connected = false;
            state.contentKnown = state.profileKnown = state.powerKnown = false;
            state.synchronizedState = false;
        });
        mReconnect.store(true, std::memory_order_release);
    }

    DisplayColorPolicy getPolicy(PhysicalDisplayId id) const override {
        if (!isPrimary(id) || mOverflow.load() || mReconnect.load() || mPowerChanged.load() ||
            !mPolicyEnabled.load()) return {};
        // One bounded seqlock read: readers neither wait nor spin. All fields
        // are atomic; a concurrent update simply uses upstream policy this frame.
        const auto before = mPolicySequence.load();
        if (before & 1) return {};
        const auto revision = mPolicyRevision.load();
        const auto intent = mPolicyIntent.load();
        const auto wcg = mPolicyWcg.load();
        if (before != mPolicySequence.load() || !mPolicyEnabled.load() ||
            mOverflow.load() || mReconnect.load() || mPowerChanged.load()) return {};
        return {.revision = revision, .renderIntent = static_cast<ui::RenderIntent>(intent),
                .wideColorGamut = wcg};
    }

    void onContent(PhysicalDisplayId id, const DisplayColorContent& content) override {
        if (!isPrimary(id)) return;
        update([&](auto& state) {
            state.contentKnown = true;
            state.enabled = content.enabled;
            state.managed = content.managed;
            state.sdrCandidate = static_cast<int32_t>(content.sdrCandidate);
            state.hdrCandidate = static_cast<int32_t>(content.hdrCandidate);
            state.outputCandidate = static_cast<int32_t>(content.outputCandidate);
            state.forceHdrClientComposition = content.forceHdrClientComposition;
            if ((!content.enabled || !content.managed ||
                 content.hdrCandidate != ui::Dataspace::UNKNOWN) && mPolicyEnabled.exchange(false)) {
                mPowerChanged.store(true, std::memory_order_release);
            }
        });
    }

    void onPolicyRejected(PhysicalDisplayId id, uint64_t revision) override {
        if (!isPrimary(id)) return;
        update([&](auto& state) {
            state.profileKnown = false;
            state.profileRejected = true;
            state.policyRevision = static_cast<int64_t>(revision);
            state.policyRequestId = requestForRevision(revision);
        });
    }

    void onColorProfile(PhysicalDisplayId id, uint64_t revision, ui::ColorMode mode,
                        ui::Dataspace dataspace, ui::RenderIntent intent,
                        status_t result) override {
        if (!isPrimary(id)) return;
        update([&](auto& state) {
            // A rejected device policy falls back to revision zero. Preserve
            // its rejection until the controller publishes a new request;
            // otherwise coalescing could erase the only failure observation.
            if (state.profileRejected && revision == 0) return;
            state.profileKnown = true;
            state.profileRejected = false;
            state.policyRevision = static_cast<int64_t>(revision);
            state.policyRequestId = requestForRevision(revision);
            state.colorMode = static_cast<int32_t>(mode);
            state.dataspace = static_cast<int32_t>(dataspace);
            state.renderIntent = static_cast<int32_t>(intent);
            state.hwcResult = result;
        });
    }

    void onPowerMode(PhysicalDisplayId id, DisplayColorPowerMode requested,
                     DisplayColorPowerMode lastAttempted, bool accepted) override {
        if (!isPrimary(id)) return;
        update([&](auto& state) {
            if (!accepted || !state.powerKnown ||
                state.lastAttempted != static_cast<int32_t>(lastAttempted)) {
                state.contentKnown = state.profileKnown = false;
                state.profileRejected = false;
                mPolicyEnabled.store(false);
                mPowerChanged.store(true, std::memory_order_release);
            }
            state.powerKnown = true;
            state.requested = static_cast<int32_t>(requested);
            state.lastAttempted = static_cast<int32_t>(lastAttempted);
            state.accepted = accepted;
        });
    }

private:
    // Binder frees each cookie only after all late death callbacks finish.
    struct DeathCookie {
        RothkoDisplayColorProvider* owner;
        int64_t connection;
    };

    struct RevisionMapping {
        std::atomic<uint64_t> revision{0};
        std::atomic<int64_t> request{0};
        std::atomic<int64_t> connection{0};
    };

    static void onDeath(void* opaque) {
        auto* cookie = static_cast<DeathCookie*>(opaque);
        if (cookie->owner->mLiveConnection.load(std::memory_order_acquire) == cookie->connection) {
            cookie->owner->mPolicyEnabled.store(false, std::memory_order_release);
            cookie->owner->mReconnect.store(true, std::memory_order_release);
        }
    }

    bool isPrimary(PhysicalDisplayId id) const {
        return mPrimaryConnected.load(std::memory_order_acquire) &&
                mPrimaryId.load(std::memory_order_acquire) == id.value;
    }

    void loseState() {
        mPolicyEnabled.store(false, std::memory_order_release);
        mOverflow.store(true, std::memory_order_release);
    }

    template <typename Mutate>
    void update(Mutate mutate) {
        std::unique_lock lock(mStateMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            loseState();
            return;
        }
        mutate(mState);
        ++mState.sequence;
        mState.synchronizedState = mState.connected && mState.contentKnown &&
                mState.profileKnown && mState.powerKnown && !mState.profileRejected;
        mDirty.store(true, std::memory_order_release);
    }

    int64_t requestForRevision(uint64_t revision) const {
        if (revision == 0) return 0;
        const auto& slot = mRevisionHistory[revision % mRevisionHistory.size()];
        const auto before = slot.revision.load();
        const auto request = slot.request.load();
        const auto connection = slot.connection.load();
        return before == revision && slot.revision.load() == before &&
                        connection == mLiveConnection.load()
                ? request : 0;
    }

    void enqueuePolicy(const PolicyMessage& message) {
        if (message.connection != mLiveConnection.load(std::memory_order_acquire)) return;
        std::unique_lock lock(mQueueMutex, std::try_to_lock);
        if (!lock.owns_lock() || mQueueSize == kPolicyQueueCapacity) {
            loseState();
            return;
        }
        mPolicyQueue[(mQueueHead + mQueueSize) % kPolicyQueueCapacity] = message;
        ++mQueueSize;
    }

    void invalidate() {
        std::function<void()> callback;
        {
            std::lock_guard lock(mStateMutex);
            callback = mInvalidate;
        }
        if (callback) callback();
    }

    void clearPolicy() {
        mPolicyEnabled.store(false, std::memory_order_release);
        mPolicySequence.fetch_add(1);
        mPolicyRevision.store(0);
        mPolicySequence.fetch_add(1);
        invalidate();
    }

    void disconnect() {
        mLiveConnection.store(0, std::memory_order_release);
        clearPolicy();
        if (mRemote && mDeathCookie) {
            AIBinder_unlinkToDeath(mRemote->asBinder().get(), mDeathRecipient.get(), mDeathCookie);
        }
        mRemote.reset();
        mCallback.reset();
        mDeathCookie = nullptr;
        mLastRequest = 0;
        {
            std::lock_guard lock(mStateMutex);
            // Content and power are continuously tracked by SF across an IPC
            // reconnect. Old controller request/profile correlation is not.
            mState.profileKnown = false;
            mState.profileRejected = false;
            mState.policyRequestId = mState.policyRevision = 0;
            mState.synchronizedState = false;
            ++mState.sequence;
        }
        {
            std::lock_guard lock(mQueueMutex);
            mQueueSize = mQueueHead = 0;
        }
    }

    void connect() {
        ndk::SpAIBinder binder(AServiceManager_checkService(kService));
        if (!binder.get()) {
            const uint64_t attempt = ++mLookupAttempts;
            if (attempt == 1 || attempt % 120 == 0) {
                ALOGW("Display colour controller service not found (attempt %llu)",
                      static_cast<unsigned long long>(attempt));
            }
            return;
        }
        auto remote = transport::IRothkoDisplayColor::fromBinder(binder);
        if (!remote) {
            const uint64_t attempt = ++mCastAttempts;
            if (attempt == 1 || attempt % 120 == 0) {
                ALOGW("Display colour controller interface cast failed (attempt %llu)",
                      static_cast<unsigned long long>(attempt));
            }
            return;
        }
        mLookupAttempts.store(0);
        mCastAttempts.store(0);
        if (mNextConnection == std::numeric_limits<int64_t>::max()) return;
        const int64_t generation = ++mNextConnection;
        // Binder owns cookie cleanup through onUnlinked, including failed
        // links; deletion waits for every late death callback to finish.
        auto* cookie = new DeathCookie{this, generation};
        // SurfaceFlinger runs a single-threaded binder server, so it cannot
        // register a death recipient for this connection. That is not a reason
        // to drop the connection: the periodic reportState call already detects
        // a dead controller. Binder frees the cookie through onUnlinked whether
        // or not the link succeeded.
        const bool deathLinked =
                AIBinder_linkToDeath(binder.get(), mDeathRecipient.get(), cookie) == STATUS_OK;
        if (!deathLinked) {
            ALOGW("No death recipient for the controller connection; "
                  "reportState failures remain the liveness check");
        }
        auto callback = ndk::SharedRefBase::make<PolicyCallback>(generation,
                [this](const auto& message) { enqueuePolicy(message); });
        mLiveConnection.store(generation, std::memory_order_release);
        const auto status = remote->setListener(mSfSession, generation, callback);
        if (!status.isOk()) {
            ALOGW("setListener rejected: %s (code %d)", status.getDescription().c_str(),
                  status.getExceptionCode());
            mLiveConnection.store(0, std::memory_order_release);
            if (deathLinked) {
                AIBinder_unlinkToDeath(binder.get(), mDeathRecipient.get(), cookie);
            }
            return;
        }
        mRemote = std::move(remote);
        mCallback = std::move(callback);
        mDeathCookie = deathLinked ? cookie : nullptr;
        ALOGI("Connected to the display colour controller (generation=%lld)",
              static_cast<long long>(generation));
        mDirty.store(true, std::memory_order_release);
        invalidate();
    }

    void handleOverflow() {
        disconnect();
        {
            std::lock_guard lock(mStateMutex);
            // No old content or power is promoted to known after data loss.
            // New content/profile follow invalidate; fresh power must come
            // from a real HWC transition, not an invented power notification.
            mState.contentKnown = mState.profileKnown = mState.powerKnown = false;
            mState.connected = mPrimaryConnected.load(std::memory_order_acquire);
            mState.physicalId = static_cast<int64_t>(mPrimaryId.load(std::memory_order_acquire));
            mState.profileRejected = false;
            mState.synchronizedState = false;
            ++mState.sequence;
        }
        mDirty.store(true, std::memory_order_release);
        invalidate();
        ALOGW("Lost local display-color state; policy cleared and fresh state required");
    }

    void applyPolicy(const PolicyMessage& message) {
        if (!mRemote || message.connection != mLiveConnection.load(std::memory_order_acquire) ||
            !mPrimaryConnected.load(std::memory_order_acquire)) return;
        const bool validIntent = message.intent == -1 ||
                (message.intent >= 256 && message.intent <= 258);
        if (!validIntent || message.requestId <= 0 || message.requestId < mLastRequest) {
            loseState();
            return;
        }
        if (message.requestId == mLastRequest) {
            if (message.intent != mLastIntent || message.wcg != mLastWcg) loseState();
            return;
        }
        mLastRequest = message.requestId;
        mLastIntent = message.intent;
        mLastWcg = message.wcg;
        if (message.intent == -1) {
            clearPolicy();
            return;
        }
        bool clearPolicyAfterUnlock = false;
        {
            std::lock_guard lock(mStateMutex);
            // A controller snapshot can race power/HDR changes. Never apply a
            // user SDR override against a newer local non-SDR/unknown state.
            if (!mState.contentKnown || !mState.enabled || !mState.managed ||
                mState.hdrCandidate != static_cast<int32_t>(ui::Dataspace::UNKNOWN) ||
                !mState.powerKnown || !mState.accepted ||
                mState.lastAttempted != static_cast<int32_t>(DisplayColorPowerMode::On)) {
                clearPolicyAfterUnlock = true;
            }
        }
        if (clearPolicyAfterUnlock) {
            clearPolicy();
            return;
        }
        if (mNextRevision == static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            loseState();
            return;
        }
        const auto revision = ++mNextRevision;
        auto& slot = mRevisionHistory[revision % mRevisionHistory.size()];
        slot.revision.store(0);
        slot.request.store(message.requestId);
        slot.connection.store(message.connection);
        slot.revision.store(revision);
        {
            std::lock_guard lock(mStateMutex);
            mState.profileKnown = false;
            mState.profileRejected = false;
            mState.synchronizedState = false;
            ++mState.sequence;
        }
        mPolicyEnabled.store(false);
        mPolicySequence.fetch_add(1);
        mPolicyIntent.store(message.intent);
        mPolicyWcg.store(message.wcg);
        mPolicyRevision.store(revision);
        mPolicySequence.fetch_add(1);
        mPolicyEnabled.store(true, std::memory_order_release);
        invalidate();
    }

    void run() {
        // Do not contend with the one-time SF invalidate callback install.
        while (!mStarted.load(std::memory_order_acquire)) std::this_thread::sleep_for(20ms);
        ALOGI("Display colour worker started");
        ABinderProcess_startThreadPool();
        auto nextLookup = std::chrono::steady_clock::now();
        for (;;) {
            if (mOverflow.exchange(false, std::memory_order_acq_rel)) handleOverflow();
            if (mReconnect.exchange(false, std::memory_order_acq_rel)) disconnect();
            if (mPowerChanged.exchange(false, std::memory_order_acq_rel)) clearPolicy();
            const auto now = std::chrono::steady_clock::now();
            if (!mRemote && now >= nextLookup) {
                connect();
                nextLookup = now + 500ms;
            }
            // Only this worker drains the finite policy ring.
            for (size_t count = 0; count < kPolicyQueueCapacity; ++count) {
                PolicyMessage message;
                {
                    std::lock_guard lock(mQueueMutex);
                    if (!mQueueSize) break;
                    message = mPolicyQueue[mQueueHead];
                    mQueueHead = (mQueueHead + 1) % kPolicyQueueCapacity;
                    --mQueueSize;
                }
                applyPolicy(message);
            }
            if (mRemote && mDirty.exchange(false, std::memory_order_acq_rel)) {
                transport::DisplayState state;
                {
                    std::lock_guard lock(mStateMutex);
                    state = mState;
                }
                state.sfSession = mSfSession;
                state.connection = mLiveConnection.load(std::memory_order_acquire);
                if (!mRemote->reportState(state).isOk()) disconnect();
            }
            std::this_thread::sleep_for(20ms);
        }
    }

    const int64_t mSfSession;
    std::mutex mStateMutex;
    transport::DisplayState mState;
    std::function<void()> mInvalidate;
    std::atomic<uint64_t> mPrimaryId{0};
    std::atomic<bool> mPrimaryConnected{false};
    std::atomic<bool> mDirty{false};
    std::atomic<bool> mOverflow{false};
    std::atomic<bool> mReconnect{false};
    std::atomic<bool> mPowerChanged{false};
    std::atomic<bool> mStarted{false};
    // Heartbeat every 120 attempts (about a minute), so a boot-time failure
    // that predates logd's socket is still visible later.
    std::atomic<uint64_t> mLookupAttempts{0};
    std::atomic<uint64_t> mCastAttempts{0};

    // Worker-only writer, bounded seqlock readers in CE.
    std::atomic<uint64_t> mPolicySequence{0};
    std::atomic<uint64_t> mPolicyRevision{0};
    std::atomic<int32_t> mPolicyIntent{0};
    std::atomic<bool> mPolicyWcg{true};
    std::atomic<bool> mPolicyEnabled{false};
    uint64_t mNextRevision = 0;
    std::array<RevisionMapping, 32> mRevisionHistory{};

    std::mutex mQueueMutex;
    std::array<PolicyMessage, kPolicyQueueCapacity> mPolicyQueue{};
    size_t mQueueHead = 0;
    size_t mQueueSize = 0;
    std::atomic<int64_t> mLiveConnection{0};
    int64_t mNextConnection = 0;
    int64_t mLastRequest = 0;
    int32_t mLastIntent = -1;
    bool mLastWcg = true;
    std::shared_ptr<transport::IRothkoDisplayColor> mRemote;
    std::shared_ptr<PolicyCallback> mCallback;
    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
    DeathCookie* mDeathCookie = nullptr;
};

} // namespace

DisplayColorExtension& getDisplayColorExtension() {
    static auto* provider = new RothkoDisplayColorProvider();
    return *provider;
}

} // namespace android::compositionengine
