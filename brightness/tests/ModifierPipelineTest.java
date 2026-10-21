/* SPDX-License-Identifier: Apache-2.0 */
package com.android.server.display.brightness.clamper;

import android.content.Context;
import android.hardware.display.BrightnessInfo;
import android.hardware.display.DisplayManagerInternal.DisplayPowerRequest;
import android.os.Handler;

import com.android.server.display.DisplayBrightnessState;
import com.android.server.display.DisplayDeviceConfig;
import com.android.server.display.brightness.DeviceBrightnessPolicy;
import com.android.server.display.brightness.BrightnessReason;

import java.io.PrintWriter;

/** Tests the actual generic modifier. DDC fixtures test the bound contract, not calibration. */
public final class ModifierPipelineTest {
    private static int assertions;
    private static void equal(float expected, float actual, String what) {
        assertions++;
        if (Math.abs(expected - actual) > 0.00001f) {
            throw new AssertionError(what + ": " + expected + " != " + actual);
        }
    }
    public static final class Policy extends DeviceBrightnessPolicy {
        static float cap = 160;
        static int stopped;
        static boolean hdrKnown, hdrPresent;
        static boolean separateSdr;
        static float sdrCap;
        static boolean explicitReason;
        static int reason = BrightnessInfo.BRIGHTNESS_MAX_REASON_THERMAL;
        public Policy(Context c, Handler h, Runnable changed) { }
        public void setDisplayState(int state) { }
        public void setAutomaticBrightness(boolean enabled) { }
        public void setAmbientLux(float lux) { }
        public void setHdrContent(boolean known, boolean present) {
            hdrKnown = known; hdrPresent = present;
        }
        public float getMaxBrightnessNits() { return cap; }
        public float getMaxSdrBrightnessNits(float common) {
            return separateSdr ? sdrCap : super.getMaxSdrBrightnessNits(common);
        }
        public BrightnessCap getBrightnessCap() {
            return explicitReason ? new BrightnessCap(cap, getMaxSdrBrightnessNits(cap), reason)
                    : super.getBrightnessCap();
        }
        public void stop() { stopped++; }
        public void dump(PrintWriter p) { }
    }
    public static void main(String[] args) {
        Handler handler = new Handler();
        DisplayDeviceConfig ddc = new DisplayDeviceConfig();
        var data = new BrightnessClamperController.DisplayDeviceData(ddc, 0);
        int[] changes = {0};
        DeviceBrightnessModifier modifier = new DeviceBrightnessModifier(
                new Context(Policy.class.getName()), handler, () -> changes[0]++, data);
        handler.runDue();
        equal(2, changes[0], "initial changed cap and deferred aggregate publication");
        DisplayPowerRequest request = new DisplayPowerRequest();
        var state = new DisplayBrightnessState.Builder();
        state.setBrightness(0.9f);
        modifier.apply(request, state);
        float logical160 = (160f - 2) / 498;
        equal(logical160, state.getMaxBrightness(), "nits-to-DDC common max");
        equal(logical160, state.getBrightness(), "SDR obeys cap");
        equal(1, state.getBrightnessMaxReason(), "thermal max reason");
        equal(BrightnessReason.MODIFIER_THROTTLED, state.getBrightnessReason().getModifier(),
                "default thermal policy keeps its throttled marker");
        equal(0, state.slow ? 1 : 0, "tightening does not use a slow ramp");
        var aggregate = new BrightnessClamperController.ModifiersAggregatedState();
        modifier.applyStateChange(aggregate);
        equal(logical160, aggregate.mMaxBrightness, "aggregate cap used before HDR");
        state = new DisplayBrightnessState.Builder();
        state.setBrightness(0.1f); state.setMaxBrightness(0.2f);
        modifier.apply(request, state);
        equal(0.2f, state.getMaxBrightness(), "other stricter max preserved");
        equal(0.1f, state.getBrightness(), "dimmer target preserved");
        Policy.cap = 1000;
        state = new DisplayBrightnessState.Builder();
        modifier.apply(request, state);
        equal(1, state.getMaxBrightness(), "nits above calibration clipped to its top");
        Policy.cap = Float.NaN;
        state = new DisplayBrightnessState.Builder();
        modifier.apply(request, state);
        equal(0, state.getMaxBrightness(), "invalid provider output fails closed");
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY, state.getBrightnessMaxReason(),
                "invalid provider output is not an observed thermal event");
        Policy.cap = Float.POSITIVE_INFINITY;
        state = new DisplayBrightnessState.Builder();
        modifier.apply(request, state);
        equal(1, state.getMaxBrightness(), "explicit unlimited policy");
        // Deliberately simple 2..4500 calibration fixture: tests bound propagation, not the
        // physical panel spline. A tighter SDR bound must not erase HDR's common headroom.
        ddc.maxNits = 4500; Policy.cap = 4500; Policy.separateSdr = true; Policy.sdrCap = 1600;
        state = new DisplayBrightnessState.Builder(); state.setBrightness(1);
        modifier.apply(request, state);
        equal(1, state.getMaxBrightness(), "common maximum still permits eligible HDR");
        equal(1598f / 4498, state.getBrightness(), "SDR target keeps its independent normal bound");
        aggregate = new BrightnessClamperController.ModifiersAggregatedState();
        modifier.applyStateChange(aggregate);
        equal(1, aggregate.mMaxBrightness, "SDR white point does not cap the HDR aggregate");
        state = new DisplayBrightnessState.Builder(); state.setBrightness(1); state.setMaxBrightness(0.2f);
        modifier.apply(request, state);
        equal(0.2f, state.getMaxBrightness(), "stricter external bound preserved for HDR");
        equal(0.2f, state.getBrightness(), "stricter external bound also wins over SDR target");
        Policy.sdrCap = Float.NaN;
        state = new DisplayBrightnessState.Builder(); state.setBrightness(1);
        modifier.apply(request, state);
        equal(0, state.getBrightness(), "invalid SDR output cannot grant a higher SDR white point");
        Policy.separateSdr = false; ddc.maxNits = 500;
        modifier.setHdrContent(true, true);
        equal(1, Policy.hdrKnown && Policy.hdrPresent ? 1 : 0, "actual HDR report reaches provider");
        modifier.setHdrContent(false, false);
        equal(0, Policy.hdrKnown || Policy.hdrPresent ? 1 : 0, "display removal propagates unknown");

        Policy.cap = 160; Policy.explicitReason = true;
        Policy.reason = BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY;
        state = new DisplayBrightnessState.Builder(); state.setBrightness(1);
        modifier.apply(request, state);
        equal(logical160, state.getMaxBrightness(), "content policy still constrains common maximum");
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY, state.getBrightnessMaxReason(),
                "content/fallback constraint does not masquerade as overheating");
        equal(0, state.getBrightnessReason().getModifier(), "content constraint has no thermal marker");
        aggregate = new BrightnessClamperController.ModifiersAggregatedState();
        modifier.applyStateChange(aggregate);
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY, aggregate.mMaxBrightnessReason,
                "aggregate carries the same content cause");
        int beforeReasonChange = changes[0];
        Policy.reason = BrightnessInfo.BRIGHTNESS_MAX_REASON_THERMAL;
        aggregate = new BrightnessClamperController.ModifiersAggregatedState();
        modifier.applyStateChange(aggregate);
        equal(beforeReasonChange + 1, changes[0], "cause-only change notifies without a numeric change");
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_THERMAL, aggregate.mMaxBrightnessReason,
                "thermal cause replaces content cause at identical nits");
        Policy.reason = BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY;
        state = new DisplayBrightnessState.Builder(); state.setMaxBrightness(0.2f);
        state.setBrightnessMaxReason(BrightnessInfo.BRIGHTNESS_MAX_REASON_POWER_IC);
        state.getBrightnessReason().addModifier(BrightnessReason.MODIFIER_THROTTLED);
        modifier.apply(request, state);
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_POWER_IC, state.getBrightnessMaxReason(),
                "stricter preceding power constraint keeps its cause");
        equal(BrightnessReason.MODIFIER_THROTTLED, state.getBrightnessReason().getModifier(),
                "device policy cannot erase another protection's throttling marker");
        aggregate = new BrightnessClamperController.ModifiersAggregatedState();
        aggregate.mMaxBrightness = 0.2f;
        aggregate.mMaxBrightnessReason = BrightnessInfo.BRIGHTNESS_MAX_REASON_POWER_IC;
        modifier.applyStateChange(aggregate);
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_POWER_IC, aggregate.mMaxBrightnessReason,
                "stricter aggregate keeps original cause");
        state = new DisplayBrightnessState.Builder(); state.setMaxBrightness(logical160);
        state.setBrightnessMaxReason(BrightnessInfo.BRIGHTNESS_MAX_REASON_THERMAL);
        modifier.apply(request, state);
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_THERMAL, state.getBrightnessMaxReason(),
                "equal preceding thermal constraint is preserved");
        modifier.stop();
        modifier.setHdrContent(true, true);
        equal(0, Policy.hdrKnown || Policy.hdrPresent ? 1 : 0, "stopped provider receives no report");
        equal(1, Policy.stopped, "provider is stopped");
        DeviceBrightnessModifier absent = new DeviceBrightnessModifier(new Context(), handler,
                () -> {}, data);
        state = new DisplayBrightnessState.Builder();
        absent.apply(request, state);
        equal(1, state.getMaxBrightness(), "empty config has no effect");
        Policy.cap = 160;
        DeviceBrightnessModifier external = new DeviceBrightnessModifier(
                new Context(Policy.class.getName()), handler, () -> {},
                new BrightnessClamperController.DisplayDeviceData(ddc, 1));
        state = new DisplayBrightnessState.Builder();
        external.apply(request, state);
        equal(1, state.getMaxBrightness(), "built-in policy not applied to external display");
        DeviceBrightnessModifier missing = new DeviceBrightnessModifier(new Context("no.such.Policy"),
                handler, () -> {}, data);
        state = new DisplayBrightnessState.Builder();
        missing.apply(request, state);
        equal(0, state.getMaxBrightness(), "missing configured class does not crash or uncap");
        equal(BrightnessInfo.BRIGHTNESS_MAX_REASON_DEVICE_POLICY, state.getBrightnessMaxReason(),
                "class-load failure is not reported as overheating");
        System.out.println("PASS: " + assertions + " common-bound/factory/stop assertions");
    }
}
