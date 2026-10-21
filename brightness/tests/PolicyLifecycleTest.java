/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import android.os.Handler;
import android.os.FileObserver;
import android.os.SystemClock;
import android.view.Display;

import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.function.IntConsumer;
import vendor.xiaomi.rothko.display.brightness.HistogramSample;

/** Exercises the real reader policy with deterministic handler queues and injected read-only IO. */
public final class PolicyLifecycleTest {
    private static int assertions;
    private static void equal(float expected, float actual, String what) {
        assertions++;
        if (Float.compare(expected, actual) != 0) {
            throw new AssertionError(what + ": expected=" + expected + " actual=" + actual);
        }
    }
    private static final class IO extends RothkoBrightnessPolicy.Injector {
        final String file;
        String board = "31000";
        String safety = "0";
        boolean fail;
        boolean configFail;
        boolean ntc;
        OprBrightnessClient.Source source = new OprBrightnessClient.Source();
        int reads;
        Runnable duringNextRead;
        final List<FileObserver> observers = new ArrayList<>();
        IO(String file) { this.file = file; }
        @Override OprBrightnessClient.Source oprSource() { return source; }
        @Override StockBrightnessConfig loadConfig() throws Exception {
            if (configFail) throw new IOException("test config unavailable");
            try (FileInputStream input = new FileInputStream(file)) {
                return StockBrightnessConfig.read(input);
            }
        }
        @Override boolean ntcEnabled() { return ntc; }
        @Override FileObserver newObserver(String name, IntConsumer callback) {
            FileObserver observer = super.newObserver(name, callback);
            observers.add(observer);
            return observer;
        }
        @Override String readNode(String node) throws IOException {
            if (observers.isEmpty() || observers.stream().noneMatch(o -> o.active)) {
                throw new AssertionError("sample read before observer registration");
            }
            reads++;
            if (duringNextRead != null) {
                Runnable event = duringNextRead;
                duringNextRead = null;
                event.run();
            }
            if (fail) throw new IOException("test read failed");
            switch (node) {
                case "board_sensor_temp": return board;
                case "sconfig": return "-1";
                case "thermal_max_brightness": return safety;
                default: throw new IOException("test missing " + node);
            }
        }
    }
    public static void main(String[] args) throws Exception {
        SystemClock.now = 0;
        Handler display = new Handler();
        Handler worker = new Handler();
        IO io = new IO(args[0]);
        int[] events = {0};
        RothkoBrightnessPolicy policy = new RothkoBrightnessPolicy(display, worker,
                () -> events[0]++, io);
        equal(160, policy.getMaxBrightnessNits(), "initial conservative cap");
        policy.setDisplayState(Display.STATE_ON);
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "valid cold sample");
        equal(3, io.observers.size(), "three stock mandatory node watches");
        SystemClock.now = 100;
        io.safety = "100";
        int before = io.reads;
        io.observers.get(2).onEvent(FileObserver.MODIFY, null);
        io.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "inotify refresh before one-second poll");
        equal(3, io.reads - before, "modify burst coalesced to one complete sample");
        io.safety = "0";
        io.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "event recovery");
        SystemClock.now = 2600;
        equal(160, policy.getMaxBrightnessNits(), "elapsed-realtime expiry before queue resumes");
        display.runDue();
        io.board = "40000"; io.safety = "100";
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "fresh tight safety bound");
        SystemClock.now += 1000; io.fail = true;
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "read failure cannot raise prior tight cap");
        SystemClock.now += 1000; io.fail = false; io.board = "31000"; io.safety = "0";
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "valid recovery clears failure");
        SystemClock.now += 1000;
        worker.runDue(); // Old sample is queued on display handler, not applied yet.
        policy.setDisplayState(Display.STATE_OFF);
        policy.setDisplayState(Display.STATE_ON);
        display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "old generation cannot permit boost after wake");
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "new generation accepted");
        before = io.reads;
        io.observers.get(0).onEvent(FileObserver.MODIFY, null); // old generation's delayed event
        worker.runDue(); display.runDue();
        equal(before, io.reads, "old observer event cannot trigger a new generation read");
        FileObserver current = io.observers.get(io.observers.size() - 1);
        current.onEvent(FileObserver.MODIFY, null);
        current.onEvent(FileObserver.DELETE_SELF, null);
        equal(160, policy.getMaxBrightnessNits(),
                "node deletion revokes snapshot before display queue dispatch");
        display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "node deletion immediately revokes old snapshot");
        before = io.observers.size();
        int readsBeforeReplacement = io.reads;
        worker.runDue(); display.runDue();
        equal(before + 3, io.observers.size(), "lost node triggers watch replacement");
        equal(0, current.active ? 1 : 0, "old watch released");
        equal(3, io.reads - readsBeforeReplacement,
                "coalesced modify plus delete does one complete replacement sample");
        FileObserver replacement = io.observers.get(io.observers.size() - 1);
        io.duringNextRead = () -> replacement.onEvent(FileObserver.DELETE_SELF, null);
        SystemClock.now += 1000;
        worker.runOneDue(); // Stop before the follow-up read so an old snapshot cannot hide it.
        display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "read spanning node deletion cannot grant boost");
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "new watch epoch recovers");
        io.safety = "10";
        io.observers.get(io.observers.size() - 1).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(10, policy.getMaxBrightnessNits(), "fresh cap below factory fallback");
        io.observers.get(io.observers.size() - 1).onEvent(FileObserver.MOVE_SELF, null);
        equal(10, policy.getMaxBrightnessNits(),
                "undispatched watch invalidation preserves tighter prior cap");
        io.safety = "0";
        display.runDue(); worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(),
                "fresh replacement snapshot restores normal cap");
        policy.stop();
        SystemClock.now += 10000;
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "stopped policy cannot grant permission");
        equal(0, worker.pending(), "no worker poll after stop");
        equal(0, display.pending(), "no expiry callback after stop");
        equal(0, io.observers.stream().anyMatch(o -> o.active) ? 1 : 0, "all watches stopped");

        IO ntc = new IO(args[0]); ntc.ntc = true;
        policy = new RothkoBrightnessPolicy(display, worker, () -> {}, ntc);
        policy.setDisplayState(Display.STATE_ON);
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "enabled but missing NTC fails closed");
        policy.stop();

        IO brokenConfig = new IO(args[0]); brokenConfig.configFail = true;
        policy = new RothkoBrightnessPolicy(display, worker, () -> {}, brokenConfig);
        policy.setDisplayState(Display.STATE_ON);
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "config failure");
        brokenConfig.configFail = false; SystemClock.now += 1000;
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "config load retry");
        brokenConfig.board = "not-a-number"; SystemClock.now += 1000;
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "malformed board temperature");
        policy.stop();

        IO combined = new IO(args[0]); combined.safety = "2000";
        policy = new RothkoBrightnessPolicy(display, worker, () -> {}, combined);
        policy.setDisplayState(Display.STATE_ON);
        policy.setAutomaticBrightness(true);
        policy.setAmbientLux(30000);
        worker.runDue(); display.runDue();
        equal(1000, policy.getMaxBrightnessNits(), "missing OPR transport bounds looser thermal cap");
        combined.safety = "100";
        combined.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "OPR cannot override tighter stock thermal cap");
        combined.safety = "2000";
        combined.observers.get(2).onEvent(FileObserver.MODIFY, null);
        policy.setAutomaticBrightness(false);
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "manual bypasses OPR but retains stock normal peak bound");
        policy.stop(); worker.runDue(); display.runDue();

        // The native color candidate is SDR even though SF reports actual RGB HDR layers.
        boolean[] requested = {true};
        int[] frame = {1};
        IO rgb = new IO(args[0]); rgb.safety = "2000";
        rgb.source = new OprBrightnessClient.Source() {
            @Override OprBrightnessClient.Response read(long request, boolean enable) {
                requested[0] = enable;
                HistogramSample sample = new HistogramSample();
                sample.request = request; sample.generation = 42;
                sample.sceneKnown = true; sample.hdr = false; sample.active = enable;
                sample.readStartedAt = SystemClock.now;
                sample.transportOk = true; sample.shapeValid = true; sample.result = 0;
                sample.frame = frame[0]; sample.channel = 0; sample.format = 9;
                sample.binCount = 256; sample.bins = new int[256]; sample.bins[0] = 100;
                return new OprBrightnessClient.Response(epoch(), sample);
            }
        };
        policy = new RothkoBrightnessPolicy(display, worker, () -> {}, rgb);
        policy.setDisplayState(Display.STATE_ON);
        policy.setAutomaticBrightness(true); policy.setAmbientLux(30000);
        worker.runDue(); display.runDue();
        equal(1000, policy.getMaxBrightnessNits(), "SDR baseline still conservative");
        SystemClock.now += 200; frame[0]++; worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "SDR uses original OPR band");
        policy.setHdrContent(true, true); worker.runDue(); display.runDue();
        equal(0, requested[0] ? 1 : 0, "RGB HDR releases ordinary histogram request");
        equal(1600, policy.getMaxBrightnessNits(), "RGB HDR removes SDR OPR but keeps stock normal peak bound");
        rgb.safety = "100"; rgb.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "RGB HDR cannot override stock safety limit");
        rgb.safety = "2000"; rgb.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        policy.setHdrContent(false, true); worker.runDue(); display.runDue();
        equal(1, requested[0] ? 1 : 0, "unknown cannot assert HDR despite a true present flag");
        equal(1000, policy.getMaxBrightnessNits(), "unknown/new SDR request needs fresh baseline");
        SystemClock.now += 200; frame[0]++; worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "fresh SDR frame recovers after HDR");
        // Above the distinct peak lux threshold, entering RGB HDR keeps statistics requested.
        // It must nevertheless invalidate the old SDR gray and create a new baseline.
        rgb.safety = "0"; rgb.observers.get(2).onEvent(FileObserver.MODIFY, null);
        policy.setAmbientLux(100001);
        worker.runDue(); display.runDue();
        equal(1600, policy.getMaxBrightnessNits(), "SDR remains in normal segment even in bright sun");
        policy.setHdrContent(true, true);
        worker.runDue(); display.runDue();
        equal(1, requested[0] ? 1 : 0, "HDR peak requests its own valid gray statistics");
        equal(1600, policy.getMaxBrightnessNits(), "pre-HDR gray cannot grant peak on new baseline");
        SystemClock.now += 200; frame[0]++; worker.runDue(); display.runDue();
        equal(4500, policy.getMaxBrightnessNits(), "fresh dark RGB HDR plus high lux permits stock peak segment");
        rgb.safety = "100"; rgb.observers.get(2).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(100, policy.getMaxBrightnessNits(), "stock safety bound wins over eligible HDR peak");
        rgb.safety = "0"; rgb.observers.get(2).onEvent(FileObserver.MODIFY, null);
        policy.setAmbientLux(100000);
        worker.runDue(); display.runDue();
        equal(0, requested[0] ? 1 : 0, "exact peak lux boundary releases HDR statistics");
        equal(1600, policy.getMaxBrightnessNits(), "exiting peak conditions restores normal bound");
        policy.stop(); worker.runDue(); display.runDue();
        System.out.println("PASS: " + assertions + " reader expiry/generation/recovery assertions");
    }
}
