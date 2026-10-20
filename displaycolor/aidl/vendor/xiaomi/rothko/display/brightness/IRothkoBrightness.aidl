// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.brightness;
import vendor.xiaomi.rothko.display.brightness.HistogramSample;

interface IRothkoBrightness {
    // Renews a short request lease, then returns local cached state. A process-
    // local Binder token separates restarted consumers even if request ids recur.
    HistogramSample readSample(IBinder client, long request, boolean enable);
}
