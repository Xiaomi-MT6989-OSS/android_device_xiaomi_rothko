// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.brightness;

// Device-private cached readback. No vendor IPC executes in readSample().
parcelable HistogramSample {
    long request;
    long generation;
    boolean sceneKnown;
    boolean hdr;
    boolean active;
    long readStartedAt = -1;
    boolean transportOk;
    boolean shapeValid;
    int result = -1;
    int frame;
    int channel = -1;
    int format = -1;
    int binCount;
    int[] bins;
}
