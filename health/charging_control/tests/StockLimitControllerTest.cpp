/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../StockLimitController.h"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace rothko::charging;

static unsigned checks;
static void check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
    ++checks;
}

struct FakeIo : StockLimitIo {
    uint32_t flags = 0;
    int writes = 0;
    int32_t lastPacket = 0;
    bool readable = true;
    bool writable = true;
    bool applyEnable = true;
    bool loseReadAfterWrite = false;
    bool concurrentOtherChange = false;

    std::optional<uint32_t> readFlags() override {
        return readable ? std::optional<uint32_t>(flags) : std::nullopt;
    }
    bool writePacket(int32_t packet) override {
        ++writes;
        lastPacket = packet;
        if (!writable) return false;
        if (applyEnable) flags = (flags & ~2u) | ((packet & 1) ? 2u : 0u);
        if (concurrentOtherChange) flags ^= 0x10;
        if (loseReadAfterWrite) readable = false;
        return true;
    }
};

int main() {
    check(encodeLimit({75, 80}) == 5242883, "80 percent has record1 selector and enable");
    check(encodeLimit({0, 100}) == 6553602, "reset withdraws only record1");
    check(!encodeLimit({80, 80}), "reject unsupported hysteresis");
    check(!encodeLimit({-5, 0}), "reject zero-target pseudo toggle");
    check(!encodeLimit({95, 100}), "reject enabled100 instead of canonical reset");
    check(!encodeLimit({2147483647, -1}), "reject invalid integer endpoints");

    FakeIo io;
    io.flags = 0x90008;  // Other record enabled, unrelated high16 parameter.
    StockLimitController controller(io);
    check(io.writes == 0, "construction never writes");
    check(controller.submit({75, 80}) == SubmitResult::Submitted && io.writes == 1,
          "first valid request submits");
    check((io.flags & 0x90008) == 0x90008, "submission preserves other records");
    check(controller.submit({75, 80}) == SubmitResult::Unchanged && io.writes == 1,
          "duplicate avoids clearing stock triggered state");
    io.flags ^= 0x10;
    check(controller.submit({75, 80}) == SubmitResult::Unchanged && io.writes == 1,
          "unrelated policy change does not rewrite own record");
    io.flags &= ~2u;
    check(controller.submit({75, 80}) == SubmitResult::Submitted && io.writes == 2,
          "lost enable invalidates duplicate assumption");
    controller.invalidate();
    check(controller.submit({75, 80}) == SubmitResult::Submitted && io.writes == 3,
          "driver lifecycle invalidation forces fresh submission");
    check(controller.submit({0, 100}) == SubmitResult::Submitted && !(io.flags & 2),
          "reset clears own enable only");
    const int writesBeforeInvalid = io.writes;
    check(controller.submit({0, 80}) == SubmitResult::InvalidRequest &&
              io.writes == writesBeforeInvalid, "invalid request performs no write");

    io.readable = false;
    check(controller.submit({75, 80}) == SubmitResult::ReadFailed &&
              io.writes == writesBeforeInvalid, "unreadable state fails without blind write");
    io.readable = true; io.writable = false;
    check(controller.submit({75, 80}) == SubmitResult::WriteFailed, "write failure propagates");
    io.writable = true; io.applyEnable = false;
    check(controller.submit({75, 80}) == SubmitResult::VerificationFailed,
          "successful write without enable observation is not success");
    io.applyEnable = true; io.loseReadAfterWrite = true;
    check(controller.submit({75, 80}) == SubmitResult::ReadFailed,
          "post-write read failure leaves state unverified");
    io.readable = true; io.loseReadAfterWrite = false; io.concurrentOtherChange = true;
    check(controller.submit({75, 80}) == SubmitResult::OtherRecordsChanged,
          "concurrent other-client change is detected without rollback");

    FakeIo restartedIo;
    restartedIo.flags = 2;
    StockLimitController restarted(restartedIo);
    check(restarted.submit({75, 80}) == SubmitResult::Submitted && restartedIo.writes == 1,
          "fresh service cannot infer old target from enabled bit");
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([&] { restarted.submit({75, 80}); });
    }
    for (auto& thread : threads) thread.join();
    check(restartedIo.writes == 1, "concurrent duplicate requests serialize to no extra writes");
    std::cout << "PASS: " << checks << " stock-limit submission checks\n";
}
