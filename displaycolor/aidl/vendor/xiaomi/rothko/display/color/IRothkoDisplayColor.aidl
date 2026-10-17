// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.color;

import vendor.xiaomi.rothko.display.color.DisplayState;
import vendor.xiaomi.rothko.display.color.IColorPolicyCallback;

interface IRothkoDisplayColor {
    // SF assigns a new positive connection for each registration attempt.
    // Controller replaces its previous listener, links its death and returns
    // promptly. No OEM IPC or waiting for a frame in this method.
    void setListener(long sfSession, long connection, IColorPolicyCallback listener);
    oneway void reportState(in DisplayState state);
}
