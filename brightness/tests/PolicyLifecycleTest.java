/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import android.os.Handler;
import android.os.SystemClock;
import android.view.Display;

import java.io.FileInputStream;
import java.io.IOException;

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
        IO(String file) { this.file = file; }
        @Override StockBrightnessConfig loadConfig() throws Exception {
            if (configFail) throw new IOException("test config unavailable");
            try (FileInputStream input = new FileInputStream(file)) {
                return StockBrightnessConfig.read(input);
            }
        }
        @Override boolean ntcEnabled() { return ntc; }
        @Override String readNode(String node) throws IOException {
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
        SystemClock.now = 2500;
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
        policy.stop();
        SystemClock.now += 10000;
        worker.runDue(); display.runDue();
        equal(160, policy.getMaxBrightnessNits(), "stopped policy cannot grant permission");
        equal(0, worker.pending(), "no worker poll after stop");
        equal(0, display.pending(), "no expiry callback after stop");

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
