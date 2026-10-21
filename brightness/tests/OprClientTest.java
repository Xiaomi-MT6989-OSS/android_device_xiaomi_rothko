/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;
import android.os.Handler;
import android.os.SystemClock;
import vendor.xiaomi.rothko.display.brightness.HistogramSample;

public final class OprClientTest {
    private static int assertions;
    private static void equal(float expected, float actual, String label) {
        assertions++;
        if (Float.compare(expected, actual) != 0) throw new AssertionError(label
                + ": expected=" + expected + " actual=" + actual);
    }
    private static final class Source extends OprBrightnessClient.Source {
        long epoch = 1, generation = 10;
        int frame = 1, releases, reads;
        boolean shape = true, active = true, known = true, hdr, enabled;
        boolean mismatch, empty;
        Runnable duringRead;
        @Override long epoch() { return epoch; }
        @Override OprBrightnessClient.Response read(long request, boolean enable) {
            enabled = enable; reads++;
            long capturedEpoch = epoch;
            HistogramSample sample = new HistogramSample();
            sample.request = mismatch ? request + 1 : request;
            sample.generation = generation;
            sample.readStartedAt = SystemClock.now;
            sample.active = active; sample.sceneKnown = known; sample.hdr = hdr;
            sample.transportOk = true; sample.shapeValid = shape;
            sample.result = 0; sample.frame = frame; sample.channel = 0;
            sample.format = 9; sample.binCount = 256; sample.bins = new int[256];
            sample.bins[0] = empty ? 0 : 100;
            if (duringRead != null) {
                Runnable hook = duringRead; duringRead = null; hook.run();
            }
            return new OprBrightnessClient.Response(capturedEpoch, sample);
        }
        @Override void release(long request) { releases++; epoch++; }
    }
    public static void main(String[] args) {
        SystemClock.now = 0;
        Handler display = new Handler(), worker = new Handler();
        Source source = new Source();
        int[] events = {0};
        OprBrightnessClient client = new OprBrightnessClient(display, worker, () -> events[0]++, source);
        client.update(true, true);
        worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "first frame only baseline");
        SystemClock.now = 200; source.frame = 2;
        worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "next black frame grants stock band");
        for (int time = 400; time <= 2600; time += 200) {
            SystemClock.now = time; worker.runDue(); display.runDue();
            equal(1600, client.cap(true, 30000, false, false), "duplicate still within original deadline");
        }
        SystemClock.now = 2700;
        int before = events[0]; display.runDue();
        equal(1, events[0] > before ? 1 : 0, "duplicate did not defer display expiry notification");
        equal(1000, client.cap(true, 30000, false, false), "duplicates expire without renewal");
        SystemClock.now = 2800; source.frame = 3; worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "new frame restores permission");
        source.epoch++;
        equal(1000, client.cap(true, 30000, false, false), "Binder death invalidates before any handler callback");
        SystemClock.now = 3000; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "new service epoch rebuilds baseline");
        SystemClock.now = 3200; source.frame = 4; worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "new connection advances");
        source.generation++; source.frame = 5;
        SystemClock.now = 3400; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "new native generation rebuilds baseline");
        source.frame = 6; SystemClock.now = 3600; worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "new native generation advances");
        source.shape = false; SystemClock.now = 3800; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "native validation failure revokes permission");
        source.shape = true; source.frame = 7;
        SystemClock.now = 4000; worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "read recovers on later frame");
        source.duringRead = () -> source.epoch++;
        SystemClock.now = 4200; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "death during IPC rejects queued response");
        source.frame = 8; source.duringRead = () -> client.update(false, false);
        SystemClock.now = 4400; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "off transition drops blocked old reply");
        equal(1, source.releases, "off releases native request");
        client.update(true, true); worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "new on session needs baseline");
        source.frame = 9; SystemClock.now = 4600; worker.runDue(); display.runDue();
        equal(1600, client.cap(true, 30000, false, false), "new on session advances");
        source.known = true; source.hdr = true; source.active = false; source.generation++;
        SystemClock.now = 4800; worker.runDue(); display.runDue();
        equal(Float.POSITIVE_INFINITY, client.cap(true, 30000, false, false), "known HDR bypasses SDR table only");
        SystemClock.now = 7300;
        equal(1000, client.cap(true, 30000, false, false), "HDR identity expires without fresh cached response");
        source.hdr = false; source.known = false; source.generation++;
        worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "unknown composition cannot bypass SDR fallback");
        equal(Float.POSITIVE_INFINITY, client.cap(false, 30000, false, false), "manual mode bypasses SDR OPR");
        client.update(true, false); worker.runDue(); display.runDue();
        equal(0, source.enabled ? 1 : 0, "low-lux request does not enable hardware statistics");
        source.mismatch = true; SystemClock.now = 7500; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "wrong request reply rejected");
        source.mismatch = false; source.known = true; source.active = true;
        source.duringRead = () -> SystemClock.now += 3000;
        SystemClock.now = 7700; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "blocked request cannot refresh permission at completion");
        source.empty = true; SystemClock.now += 200; worker.runDue(); display.runDue();
        equal(1000, client.cap(true, 30000, false, false), "consumer independently rejects empty histogram");
        client.stop(); worker.runDue(); display.runDue();
        equal(2, source.releases, "stop releases request");
        int reads = source.reads; SystemClock.now += 10000; worker.runDue(); display.runDue();
        equal(reads, source.reads, "no work after stop");
        equal(1000, client.cap(true, 30000, false, false), "stopped client cannot grant high band");

        Source rgb = new Source(); rgb.active = false; rgb.hdr = false;
        OprBrightnessClient rgbClient = new OprBrightnessClient(display, worker, () -> {}, rgb);
        rgbClient.update(true, false); worker.runDue(); display.runDue();
        equal(Float.POSITIVE_INFINITY, rgbClient.cap(true, 30000, true, true),
                "actual RGB HDR complements unknown PQ/HLG candidate");
        equal(1000, rgbClient.cap(true, 30000, false, false), "RGB bypass requires framework HDR signal");
        rgb.hdr = true;
        SystemClock.now += 200; worker.runDue(); display.runDue();
        equal(1000, rgbClient.cap(true, 30000, true, false),
                "known upstream absence overrides a lingering PQ candidate");
        equal(Float.POSITIVE_INFINITY, rgbClient.cap(true, 30000, false, false),
                "explicit PQ candidate remains fallback before upstream report");
        rgb.hdr = false;
        rgb.known = false;
        SystemClock.now += 200; worker.runDue(); display.runDue();
        equal(1000, rgbClient.cap(true, 30000, true, true), "RGB signal cannot bypass native unknown scene");
        rgb.known = true;
        SystemClock.now += 200; worker.runDue(); display.runDue();
        equal(Float.POSITIVE_INFINITY, rgbClient.cap(true, 30000, true, true), "fresh known scene recovers");
        rgb.epoch++;
        equal(1000, rgbClient.cap(true, 30000, true, true), "RGB signal cannot bypass Binder death");
        worker.runDue(); display.runDue();
        SystemClock.now += 2500;
        equal(1000, rgbClient.cap(true, 30000, true, true), "RGB signal cannot bypass stale response");
        rgbClient.stop(); worker.runDue(); display.runDue();

        Source peak = new Source();
        OprBrightnessClient peakClient = new OprBrightnessClient(display, worker, () -> {}, peak);
        peakClient.update(true, true); worker.runDue(); display.runDue();
        equal(1600, peakClient.peakCap(true, 100001, true, true), "peak baseline only");
        SystemClock.now += 200; peak.frame++; worker.runDue(); display.runDue();
        equal(4500, peakClient.peakCap(true, 100001, true, true), "actual RGB HDR has valid dark gray");
        equal(1600, peakClient.peakCap(true, 100001, false, true), "unknown upstream HDR cannot grant peak");
        equal(1600, peakClient.peakCap(true, 100001, true, false), "known SDR cannot grant peak");
        peakClient.update(true, true, true); // Same enable bit, different actual content.
        equal(1600, peakClient.peakCap(true, 100001, true, true), "same-enable content change revokes peak");
        worker.runDue(); display.runDue();
        equal(1600, peakClient.peakCap(true, 100001, true, true), "same cached frame is a new baseline");
        SystemClock.now += 200; peak.frame++; worker.runDue(); display.runDue();
        equal(4500, peakClient.peakCap(true, 100001, true, true), "new content advances");
        peak.epoch++;
        equal(1600, peakClient.peakCap(true, 100001, true, true), "death revokes peak before queue dispatch");
        SystemClock.now += 200; worker.runDue(); display.runDue();
        equal(1600, peakClient.peakCap(true, 100001, true, true), "new Binder epoch needs baseline");
        peakClient.stop(); worker.runDue(); display.runDue();
        equal(1600, peakClient.peakCap(true, 100001, true, true), "stop revokes peak");
        System.out.println("PASS: " + assertions + " cached OPR transport/lifecycle assertions");
    }
}
