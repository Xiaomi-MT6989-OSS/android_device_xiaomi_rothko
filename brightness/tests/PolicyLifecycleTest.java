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
        int reads;
        Runnable duringNextRead;
        final List<FileObserver> observers = new ArrayList<>();
        IO(String file) { this.file = file; }
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
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "valid cold sample");
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
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "event recovery");
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
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "valid recovery clears failure");
        SystemClock.now += 1000;
        worker.runDue(); // Old sample is queued on display handler, not applied yet.
        policy.setDisplayState(Display.STATE_OFF);
        policy.setDisplayState(Display.STATE_ON);
        display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "old generation cannot permit boost after wake");
        worker.runDue(); display.runDue();
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "new generation accepted");
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
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "new watch epoch recovers");
        io.safety = "10";
        io.observers.get(io.observers.size() - 1).onEvent(FileObserver.MODIFY, null);
        worker.runDue(); display.runDue();
        equal(10, policy.getMaxBrightnessNits(), "fresh cap below factory fallback");
        io.observers.get(io.observers.size() - 1).onEvent(FileObserver.MOVE_SELF, null);
        equal(10, policy.getMaxBrightnessNits(),
                "undispatched watch invalidation preserves tighter prior cap");
        io.safety = "0";
        display.runDue(); worker.runDue(); display.runDue();
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(),
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
        equal(Float.POSITIVE_INFINITY, policy.getMaxBrightnessNits(), "config load retry");
        brokenConfig.board = "not-a-number"; SystemClock.now += 1000;
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "malformed board temperature");
        policy.stop();
        System.out.println("PASS: " + assertions + " reader expiry/generation/recovery assertions");
    }
}
