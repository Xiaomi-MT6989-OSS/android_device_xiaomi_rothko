// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.brightness;

// Device-private ordered backlight transport, separate from read-only histogram leases.
interface IRothkoDozeBrightness {
    // System-server only. Returns after the OEM command completes; never queues
    // a timed-out command for later replay. Modes are the stock 0/1/2 enum.
    boolean apply(long physicalDisplayId, int mode);
}
