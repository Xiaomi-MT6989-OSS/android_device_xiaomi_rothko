// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
package vendor.xiaomi.rothko.display.color;

parcelable DisplayState {
    long sfSession;
    long connection;
    long sequence;
    long physicalId;
    boolean connected;
    // False after a dropped local update until fresh content, profile and
    // actual power observations have all arrived. Never infer missing state.
    boolean synchronizedState;

    boolean contentKnown;
    boolean enabled;
    boolean managed;
    int sdrCandidate;
    int hdrCandidate;
    int outputCandidate;
    boolean forceHdrClientComposition;

    boolean profileKnown;
    long policyRequestId;
    long policyRevision;
    int colorMode;
    int dataspace;
    int renderIntent;
    int hwcResult;
    boolean profileRejected;

    boolean powerKnown;
    // DisplayColorPowerMode: Unknown=0, Off=1, On=2, Doze=3,
    // DozeSuspend=4. These are NOT the OEM/native power integers.
    int requested;
    int lastAttempted;
    boolean accepted;
}
