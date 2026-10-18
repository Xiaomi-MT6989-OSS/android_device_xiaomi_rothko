/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
#include "FodInput.h"
#undef NDEBUG
#include <cassert>
#include <cmath>

using rothko::FodInput;
static auto send(FodInput& p, int type, int code, int value) {
  input_event e{};
  e.type = type;
  e.code = code;
  e.value = value;
  return p.feed(e);
}
static auto contact(FodInput& p, int x = 60300, int y = 234400) {
  send(p, EV_ABS, ABS_MT_SLOT, 9);
  send(p, EV_ABS, ABS_MT_TRACKING_ID, 4);
  send(p, EV_ABS, ABS_MT_POSITION_X, x);
  send(p, EV_ABS, ABS_MT_POSITION_Y, y);
  send(p, EV_ABS, ABS_MT_WIDTH_MAJOR, 7);
  send(p, EV_ABS, ABS_MT_WIDTH_MINOR, 7);
  return send(p, EV_SYN, SYN_REPORT, 0);
}
int main() {
  FodInput p;
  send(p, EV_KEY, FodInput::kFodKey, 1);
  assert(
      !send(p, EV_SYN, SYN_REPORT, 0));  // Actual stock key-only first frame.
  auto touch = contact(p);
  assert(touch && std::abs(touch->x - 603.f) < .01f &&
         std::abs(touch->y - 2344.f) < .01f && touch->major == 7);
  assert(!send(p, EV_SYN, SYN_REPORT, 0));  // Exactly one event per activation.

  FodInput ordinary;
  assert(!contact(ordinary));  // No FOD key, even with valid MT coordinates.
  FodInput stale;
  assert(!contact(stale));
  send(stale, EV_ABS, ABS_MT_TRACKING_ID, -1);
  send(stale, EV_KEY, FodInput::kFodKey, 1);
  assert(!send(stale, EV_SYN, SYN_REPORT, 0));  // Never reuse old coordinates.
  FodInput coordinatesFirst;
  assert(!contact(coordinatesFirst));
  send(coordinatesFirst, EV_KEY, FodInput::kFodKey, 1);
  assert(send(coordinatesFirst, EV_SYN, SYN_REPORT, 0));
  FodInput noWidth;
  send(noWidth, EV_ABS, ABS_MT_TRACKING_ID, 4);
  send(noWidth, EV_ABS, ABS_MT_POSITION_X, 60300);
  send(noWidth, EV_ABS, ABS_MT_POSITION_Y, 234400);
  send(noWidth, EV_KEY, FodInput::kFodKey, 1);
  const auto noWidthTouch = send(noWidth, EV_SYN, SYN_REPORT, 0);
  assert(noWidthTouch && noWidthTouch->major == 0 && noWidthTouch->minor == 0);
  FodInput held(true);
  send(held, EV_KEY, FodInput::kFodKey, 1);
  assert(!contact(held));
  send(held, EV_KEY, FodInput::kFodKey, 0);
  send(held, EV_KEY, FodInput::kFodKey, 1);
  assert(contact(held));
  FodInput dropped;
  send(dropped, EV_KEY, FodInput::kFodKey, 1);
  send(dropped, EV_SYN, SYN_DROPPED, 0);
  assert(!contact(dropped));
  send(dropped, EV_KEY, FodInput::kFodKey, 0);
  send(dropped, EV_KEY, FodInput::kFodKey, 1);
  assert(contact(dropped));
  FodInput invalid;
  send(invalid, EV_KEY, FodInput::kFodKey, 1);
  assert(!contact(invalid, 122000));
}
