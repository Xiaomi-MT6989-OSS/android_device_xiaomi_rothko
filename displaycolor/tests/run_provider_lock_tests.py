#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise production provider method bodies with controlled lock contention.

Only update/enqueuePolicy/loseState are extracted verbatim; this is not an
Android Binder, complete provider, frame latency or hardware test.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import shutil


def body(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1]
               / 'provider/RothkoDisplayColorProvider.cpp')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--scenario', choices=['all', 'state', 'queue'], default='all')
p.add_argument('--sanitize', action='store_true')
args = p.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
shutil.copyfile(args.source.parent / 'StateUpdateQueue.h', args.output / 'StateUpdateQueue.h')
text = args.source.read_text()
methods = '\n'.join([
    body(text, 'void loseState()'),
    'template <typename Mutate>\n' + body(text, 'void update(Mutate mutate)'),
    body(text, 'void enqueuePolicy(const PolicyMessage& message)'),
])
prefix = r'''
#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include "StateUpdateQueue.h"
using namespace std::chrono_literals;
constexpr size_t kPolicyQueueCapacity = 16;
struct PolicyMessage { long connection = 1; long requestId = 1; };
struct State {
    bool connected = true, contentKnown = true, profileKnown = true;
    bool powerKnown = true, profileRejected = false, synchronizedState = true;
    long sequence = 0;
};
struct Provider {
    std::mutex mStateMutex, mQueueMutex;
    State mState;
    rothko::color::StateUpdateQueue<State> mStateUpdates;
    std::atomic<pid_t> mProducerTid{0};
    std::atomic<bool> mPolicyEnabled{true}, mOverflow{false}, mDirty{false};
    std::atomic<long> mLiveConnection{1};
    std::array<PolicyMessage, kPolicyQueueCapacity> mPolicyQueue{};
    size_t mQueueHead = 0, mQueueSize = 0;
'''
suffix = r'''
};
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Work>
void contend(std::mutex& mutex, Work work) {
    std::unique_lock owner(mutex);
    std::promise<void> entered;
    auto ready = entered.get_future();
    auto call = std::async(std::launch::async, [&] {
        entered.set_value();
        work();
    });
    ready.wait();
    const bool waited = call.wait_for(100ms) == std::future_status::timeout;
    owner.unlock();
    call.get();
    check(waited, "ordinary contention dropped the operation instead of serializing");
}
int main(int argc, char** argv) {
    try {
        const std::string scenario = argc > 1 ? argv[1] : "all";
        if (scenario != "queue") {
        Provider state;
        std::unique_lock held(state.mStateMutex);
        auto update = std::async(std::launch::async, [&] {
            state.update([](auto& s) { s.contentKnown = true; });
        });
        const bool nonblocking = update.wait_for(1s) == std::future_status::ready;
        held.unlock();
        update.get();
        check(nonblocking, "SF update waited for the worker state lock");
        check(!state.mOverflow && !state.mDirty,
              "producer lost state or mutated the worker snapshot inline");
        check(state.mStateUpdates.pop(&state.mState), "state event was lost");
        check(!state.mOverflow && state.mPolicyEnabled && state.mDirty &&
              state.mState.sequence == 1 && state.mState.synchronizedState,
              "state update invalidated a healthy display generation");
        std::cout << "PASS contended-state-update\n";
        Provider power;
        power.update([](auto& s) { s.powerKnown = false; });
        power.update([](auto& s) { s.powerKnown = true; });
        while (power.mStateUpdates.pop(&power.mState)) {}
        check(power.mState.powerKnown && power.mState.sequence == 2 && !power.mOverflow,
              "power observations were reordered or discarded");
        std::cout << "PASS ordered-power-observations\n";
        Provider discard;
        discard.update([](auto& s) { s.powerKnown = true; });
        check(discard.mStateUpdates.pop(nullptr) && discard.mState.sequence == 0,
              "discarded event promoted stale power state");
        std::cout << "PASS discard-without-applying\n";
        Provider filled;
        for (int n = 0; n < 128; ++n) {
            filled.update([](auto& s) { s.powerKnown = true; });
        }
        check(filled.mOverflow && !filled.mPolicyEnabled,
              "full state queue did not fail closed");
        while (filled.mStateUpdates.pop(nullptr)) {}
        check(filled.mState.sequence == 0, "overflow discard applied stale events");
        std::cout << "PASS full-state-queue\n";
        Provider wrongThread;
        wrongThread.update([](auto&) {});
        std::thread other([&] { wrongThread.update([](auto&) {}); });
        other.join();
        check(wrongThread.mOverflow && !wrongThread.mPolicyEnabled,
              "unexpected producer thread corrupted the SPSC contract");
        std::cout << "PASS unexpected-producer-rejected\n";
        rothko::color::StateUpdateQueue<State, 16> stress;
        constexpr long total = 100000;
        std::thread writer([&] {
            for (long n = 1; n <= total; ++n) {
                auto change = [n](State& s) {
                    if (s.sequence != n - 1) s.profileRejected = true;
                    s.sequence = n;
                };
                while (!stress.push(change)) std::this_thread::yield();
            }
        });
        State consumed;
        while (consumed.sequence < total) {
            if (!stress.pop(&consumed)) std::this_thread::yield();
        }
        writer.join();
        check(!consumed.profileRejected, "concurrent queue wrap reordered events");
        std::cout << "PASS concurrent-wrap-100000\n";
        }
        if (scenario != "state") {
        Provider queue;
        contend(queue.mQueueMutex, [&] { queue.enqueuePolicy({1, 7}); });
        check(!queue.mOverflow && queue.mPolicyEnabled && queue.mQueueSize == 1 &&
              queue.mPolicyQueue[0].requestId == 7,
              "contended policy queue lost its request");
        std::cout << "PASS contended-policy-enqueue\n";
        Provider full;
        for (size_t n = 0; n < kPolicyQueueCapacity; ++n) full.enqueuePolicy({1, long(n+1)});
        full.enqueuePolicy({1, 100});
        check(full.mOverflow && !full.mPolicyEnabled &&
              full.mQueueSize == kPolicyQueueCapacity,
              "real queue overflow no longer fails closed");
        std::cout << "PASS real-queue-overflow\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
'''
source = args.output / 'provider-lock-test.cpp'
source.write_text(prefix + methods + suffix)
(args.output / 'production-source.sha256').write_text(
    hashlib.sha256(args.source.read_bytes()).hexdigest() + '\n')
binary = args.output / 'provider-lock-test'
subprocess.run(['g++', '-std=c++20', '-pthread', '-Wall', '-Wextra', '-Werror',
                *(['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
                  if args.sanitize else []),
                str(source), '-o', str(binary)], check=True)
run = subprocess.run([str(binary), args.scenario], capture_output=True, text=True, timeout=10)
(args.output / 'run.log').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
