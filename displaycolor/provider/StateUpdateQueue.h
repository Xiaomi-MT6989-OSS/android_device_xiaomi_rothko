// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace rothko::color {

// Single SF producer, single provider-worker consumer. Fixed inline storage:
// no allocations, locks, spinning or callbacks into SF in the producer.
// The provider checks the producer thread identity before calling push().
template <typename State, size_t Capacity = 128, size_t Bytes = 128>
class StateUpdateQueue {
    static_assert(Capacity > 1);
    static_assert(std::atomic<size_t>::is_always_lock_free);
    struct Slot {
        alignas(std::max_align_t) std::array<std::byte, Bytes> storage;
        void (*apply)(void*, State*);
    };
public:
    static constexpr size_t maxPending = Capacity - 1;
    template <typename Mutate>
    bool push(const Mutate& mutate) {
        static_assert(sizeof(Mutate) <= Bytes);
        static_assert(alignof(Mutate) <= alignof(std::max_align_t));
        static_assert(std::is_trivially_copyable_v<Mutate>);
        static_assert(std::is_nothrow_copy_constructible_v<Mutate>);
        const size_t write = mWrite.load(std::memory_order_relaxed);
        const size_t next = (write + 1) % Capacity;
        if (next == mRead.load(std::memory_order_acquire)) return false;
        auto& slot = mSlots[write];
        new (slot.storage.data()) Mutate(mutate);
        slot.apply = [](void* raw, State* state) {
            auto* value = std::launder(static_cast<Mutate*>(raw));
            if (state) (*value)(*state);
            value->~Mutate();
        };
        mWrite.store(next, std::memory_order_release);
        return true;
    }

    // nullptr destroys a discarded update without applying it after data loss.
    bool pop(State* state) {
        const size_t read = mRead.load(std::memory_order_relaxed);
        if (read == mWrite.load(std::memory_order_acquire)) return false;
        auto& slot = mSlots[read];
        slot.apply(slot.storage.data(), state);
        mRead.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }

    ~StateUpdateQueue() {
        // Destruction requires both producer and consumer to have stopped.
        while (pop(nullptr)) {}
    }
private:
    std::array<Slot, Capacity> mSlots;
    std::atomic<size_t> mRead{0};
    std::atomic<size_t> mWrite{0};
};

} // namespace rothko::color
