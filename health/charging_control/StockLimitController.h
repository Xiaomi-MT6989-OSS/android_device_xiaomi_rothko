/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

namespace rothko::charging {

struct LimitRequest {
    int min;
    int max;

    bool operator==(const LimitRequest& other) const {
        return min == other.min && max == other.max;
    }
};

// Encodes only record 1 of the stock smart_chg protocol. No global/zero-selector
// commands, input_suspend, charging-current or voltage controls are used.
inline std::optional<int32_t> encodeLimit(const LimitRequest& request) {
    if (request.max == 100 && request.min == 0) {
        return (100 << 16) | 2;  // Withdraw this record, not every stock policy.
    }
    if (request.max < 5 || request.max >= 100 || request.min != request.max - 5) {
        return std::nullopt;
    }
    return (request.max << 16) | 3;
}

// Deliberately no file implementation or Android service is installed yet.
// Implementations must report short/failed writes and parse the real flags;
// flags do not contain record 1's target percentage.
class StockLimitIo {
  public:
    virtual ~StockLimitIo() = default;
    virtual std::optional<uint32_t> readFlags() = 0;
    virtual bool writePacket(int32_t packet) = 0;
};

enum class SubmitResult {
    Submitted,
    Unchanged,
    InvalidRequest,
    ReadFailed,
    WriteFailed,
    VerificationFailed,
    OtherRecordsChanged,
};

// Candidate command submission logic, not a hardware target-value getter.
// Deduplication requires exclusive ownership of record 1. The future service
// must invalidate on a driver lifecycle change; an external same-enable target
// overwrite cannot be detected through the stock flags interface.
class StockLimitController {
  public:
    explicit StockLimitController(StockLimitIo& io) : mIo(io) {}

    SubmitResult submit(const LimitRequest& request) {
        const auto packet = encodeLimit(request);
        if (!packet) return SubmitResult::InvalidRequest;

        std::lock_guard<std::mutex> guard(mMutex);
        const auto before = mIo.readFlags();
        if (!before) {
            mLastSubmitted.reset();
            return SubmitResult::ReadFailed;
        }
        const bool enabled = request.max != 100;
        if (mLastSubmitted && *mLastSubmitted == request &&
            ((*before & kOwnEnable) != 0) == enabled) {
            return SubmitResult::Unchanged;
        }

        mLastSubmitted.reset();
        if (!mIo.writePacket(*packet)) return SubmitResult::WriteFailed;
        const auto after = mIo.readFlags();
        if (!after) return SubmitResult::ReadFailed;
        if (((*after & kOwnEnable) != 0) != enabled) {
            return SubmitResult::VerificationFailed;
        }
        if ((*before & kOtherEnables) != (*after & kOtherEnables)) {
            // Do not overwrite or roll back another client's concurrent change.
            return SubmitResult::OtherRecordsChanged;
        }
        // This records an acknowledged command plus enable-bit observation.
        // It is private deduplication state, not an independent target readback.
        mLastSubmitted = request;
        return SubmitResult::Submitted;
    }

    void invalidate() {
        std::lock_guard<std::mutex> guard(mMutex);
        mLastSubmitted.reset();
    }

  private:
    static constexpr uint32_t kOwnEnable = 1u << 1;
    // The low bit is a derived global flag; high16 belongs to another record.
    static constexpr uint32_t kOtherEnables = 0xfffcu;
    StockLimitIo& mIo;
    std::mutex mMutex;
    std::optional<LimitRequest> mLastSubmitted;
};

}  // namespace rothko::charging
