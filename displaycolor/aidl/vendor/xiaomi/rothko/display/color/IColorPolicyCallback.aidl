// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.color;

// All requests are local to one SF/controller connection. requestId is owned
// by the controller; it is NOT a token supplied by the OEM callback protocol.
oneway interface IColorPolicyCallback {
    // intent=-1 clears policy. Other supported intents are 256..258.
    // Reusing a requestId for different policy contents is a protocol error.
    void onPolicy(long connection, long requestId, int intent, boolean wideColorGamut);
}
