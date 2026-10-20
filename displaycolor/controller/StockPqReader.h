// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "StockPictureQuality.h"
#include <array>
#include <functional>
#include <utility>

namespace rothko::color {
namespace pq = aidl::vendor::mediatek::hardware::pq_aidl;

// A complete read result, before session/frame/freshness policy is applied. This
// object is recreated for every request; a failed call never reuses old bins.
struct PqReadback {
    int64_t readStartedAt = -1; // CLOCK_BOOTTIME ms, same clock as elapsedRealtime()
    bool transportOk = false;
    bool shapeValid = false;
    int32_t result = -1;
    int32_t frame = 0;
    int32_t channel = -1;
    int32_t format = -1;
    int32_t binCount = 0;
    std::array<int32_t, 256> bins{};
};

// Synchronous vendor IPC: call ONLY on a dedicated IO worker, never on an OEM
// callback, SF thread, display handler, or private Binder service pool thread.
// Does not turn histogram hardware on/off. That remains an OEM-worker action.
class StockPqReader {
public:
    using Clock = std::function<int64_t()>;
    explicit StockPqReader(Clock clock) : mClock(std::move(clock)) {}
    bool compatible(pq::IPictureQuality_AIDL& service) const;
    PqReadback read(pq::IPictureQuality_AIDL& service) const;
private:
    Clock mClock;
};
} // namespace rothko::color
