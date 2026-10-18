/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <linux/input.h>

#include <array>
#include <optional>

namespace rothko {

// Stock goodix_ts reports FOD down as KEY 0x152, before or after its MT frame.
// Coordinates are pixels * 100 (ABS maxima 121999, 271199); width is unscaled.
struct FodTouch {
  float x, y, major, minor;
};

class FodInput {
 public:
  explicit FodInput(bool heldAtActivation = false)
      : mRequireRelease(heldAtActivation) {}

  std::optional<FodTouch> feed(const input_event& e) {
    if (e.type == EV_SYN && e.code == SYN_DROPPED) {
      mSlots = {};
      mDown = false;
      mRequireRelease = true;
      return {};
    }
    if (e.type == EV_KEY && e.code == kFodKey) {
      if (e.value == 0) {
        mDown = false;
        mRequireRelease = false;
      } else if (e.value == 1 && !mRequireRelease && !mDown) {
        mDown = true;
      }
    }
    if (e.type == EV_ABS && e.code == ABS_MT_SLOT) {
      mSlot = e.value;
    } else if (e.type == EV_ABS && mSlot >= 0 && mSlot < int(mSlots.size())) {
      auto& s = mSlots[mSlot];
      switch (e.code) {
        case ABS_MT_TRACKING_ID:
          s = {};
          s.active = e.value >= 0;
          break;
        case ABS_MT_POSITION_X:
          s.x = e.value;
          s.xFresh = true;
          break;
        case ABS_MT_POSITION_Y:
          s.y = e.value;
          s.yFresh = true;
          break;
        case ABS_MT_WIDTH_MAJOR:
          s.major = e.value;
          break;
        case ABS_MT_WIDTH_MINOR:
          s.minor = e.value;
          break;
      }
    }
    if (e.type == EV_SYN && e.code == SYN_REPORT && mDown && !mConsumed &&
        mSlot >= 0 && mSlot < int(mSlots.size())) {
      const auto& s = mSlots[mSlot];
      int contacts = 0;
      for (const auto& slot : mSlots) contacts += slot.active;
      // Only a live contact observed since activation is usable. Reject
      // ambiguous multi-touch; missing width remains zero, as in the upstream
      // UDFPS sub-HAL.
      if (contacts == 1 && s.active && s.xFresh && s.yFresh && s.x >= 0 &&
          s.x <= 121999 && s.y >= 0 && s.y <= 271199) {
        mConsumed = true;
        return FodTouch{s.x / 100.f, s.y / 100.f, float(s.major),
                        float(s.minor)};
      }
    }
    return {};
  }

  static constexpr int kFodKey = 0x152;

 private:
  struct Slot {
    int x = 0, y = 0, major = 0, minor = 0;
    bool active = false, xFresh = false, yFresh = false;
  };
  std::array<Slot, 10> mSlots{};
  int mSlot = 0;
  bool mDown = false, mConsumed = false, mRequireRelease;
};
}  // namespace rothko
