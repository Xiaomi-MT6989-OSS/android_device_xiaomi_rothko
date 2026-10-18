/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.fingerprint;

import java.util.ArrayList;
import java.util.List;

/** Host regression scenarios for activation and cleanup; no device or template access. */
public final class FodStateMachineTest {
    static final class Wire implements FodStateMachine.Transport {
        final List<String> events = new ArrayList<>();
        int ui;
        int operation;
        int lowBrightness;
        int failCommand = -1;
        @Override public void send(int command, int value) throws Exception {
            if (command == failCommand) throw new Exception("injected transport failure");
            events.add(command + "/" + value);
            if (command == 1) ui = value;
            if (command == 4) operation = value;
            if (command == 7) lowBrightness = value;
        }
    }
    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    static FodStateMachine ready() {
        FodStateMachine state = new FodStateMachine(0);
        state.display(2, 2, false);
        state.lockout(0, 0);
        return state;
    }
    public static void main(String[] args) throws Exception {
        Wire wire = new Wire();
        FodStateMachine state = ready();
        state.start(1, 0, 1);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 0, "request alone must not enable capture");
        state.overlay(999, true);
        state.apply(wire);
        check(wire.ui == 0, "stale overlay cannot enable another request");
        state.overlay(1, true);
        state.apply(wire);
        check(wire.ui == 1 && wire.operation == 1, "attached enrollment must start");
        int count = wire.events.size();
        state.apply(wire);
        check(count == wire.events.size(), "unchanged state must not repeat OEM calls");
        state.overlay(1, false);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 2, "detach must stop enrollment");
        state.overlay(1, true);
        state.apply(wire);
        state.finish(1);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 2, "enrollment completion must stop capture");

        state.start(2, 0, 2);
        state.overlay(2, true);
        state.apply(wire);
        check(wire.ui == 1 && wire.operation == 3 && wire.lowBrightness == 1,
                "keyguard authentication must receive OEM operation and keyguard state");
        state.display(2, 2, true);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 4 && wire.lowBrightness == 0,
                "fade must stop authentication before blanking");
        state.display(1, 1, false);
        state.apply(wire);
        check(wire.ui == 0 && wire.events.contains("3/0"), "OFF must never masquerade as ON");
        state.display(2, 1, false);
        state.apply(wire);
        check(wire.ui == 0, "requested ON is insufficient before physical ON");
        state.display(2, 2, false);
        state.apply(wire);
        check(wire.ui == 1, "same real framework request can resume after wake");
        state.lockout(0, 1);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 4, "timed lockout must stop capture");
        state.lockout(0, 2);
        state.apply(wire);
        check(wire.ui == 0 && wire.events.contains("5/2"), "permanent lockout is forwarded");
        state.lockout(0, 0);
        state.apply(wire);
        check(wire.ui == 1, "only real lockout reset can re-enable an existing request");

        state.start(3, 0, 3);
        state.overlay(3, true);
        state.finish(2);
        state.overlay(2, false);
        state.apply(wire);
        check(wire.ui == 1 && wire.lowBrightness == 0,
                "old completion must not cancel new app auth or classify it as keyguard");
        state.cancel();
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 4, "controller/HAL cleanup must stop capture");
        state.overlay(3, true);
        state.apply(wire);
        check(wire.ui == 0, "late UI event after cancellation must not reactivate");
        state.start(4, 0, 2);
        state.overlay(4, true);
        state.apply(wire);
        state.user(10);
        state.apply(wire);
        check(wire.ui == 0 && wire.lowBrightness == 0, "user switch must clear active capture");
        state.start(5, 0, 2);
        state.overlay(5, true);
        state.apply(wire);
        check(wire.ui == 0, "old user request must stay disabled");
        state.start(6, 10, 2);
        state.overlay(6, true);
        state.apply(wire);
        check(wire.ui == 0, "unknown new-user lockout state must fail closed");
        state.lockout(10, 0);
        state.apply(wire);
        check(wire.ui == 1, "new user requires its own request and actual lockout state");
        state.cancel();
        state.apply(wire);
        state.forgetTransport();
        state.apply(wire);
        check(wire.ui == 0, "transport reconnect cannot resurrect a canceled operation");

        state = ready();
        wire = new Wire();
        state.start(1, 0, 1);
        state.overlay(1, true);
        wire.failCommand = 4;
        try {
            state.apply(wire);
            throw new AssertionError("transport failure must surface");
        } catch (Exception expected) {
            check(wire.ui == 0, "failed start cannot announce an enabled overlay");
        }
        state.cancel();
        wire.failCommand = -1;
        state.forgetTransport();
        state.apply(wire);
        check(wire.ui == 0, "caller cleanup after failure must remain inactive");
        // Low-power capture is opt-in, keyguard-only and still reports the real power/fade.
        for (int[] display : new int[][] {{1, 0}, {3, 1}, {4, 3}}) {
            state = ready();
            wire = new Wire();
            state.screenOffUnlock(true);
            state.start(10, 0, 2);
            state.overlay(10, true);
            state.display(display[0], display[0], true);
            state.apply(wire);
            check(wire.ui == 1 && wire.operation == 3 && wire.lowBrightness == 1,
                    "opted-in stable low-power keyguard must keep real auth active");
            check(wire.events.contains("3/" + display[1]) && wire.events.contains("2/1"),
                    "low-power auth must retain physical power and persistent fade truth");
            state.screenOffUnlock(false);
            state.apply(wire);
            check(wire.ui == 0 && wire.operation == 4 && wire.lowBrightness == 0,
                    "turning off the preference must stop low-power capture");
            state.screenOffUnlock(true);
            state.lockout(0, 1);
            state.apply(wire);
            check(wire.ui == 0, "low-power opt-in cannot bypass timed lockout");
            state.lockout(0, 0);
            state.apply(wire);
            check(wire.ui == 1, "actual lockout reset may restore the still-valid keyguard request");
            int transitionStart = wire.events.size();
            state.display(2, display[0], true);
            state.apply(wire);
            check(wire.ui == 1, "existing keyguard auth must survive desired/actual wake mismatch");
            state.display(2, 2, true);
            state.apply(wire);
            check(wire.ui == 1, "existing keyguard auth must survive the remaining color fade");
            state.display(2, 2, false);
            state.apply(wire);
            List<String> transition = wire.events.subList(transitionStart, wire.events.size());
            check(!transition.contains("1/0") && !transition.contains("4/4")
                            && !transition.contains("4/3") && !transition.contains("7/0"),
                    "wake must not stop/replay the same auth or clear its keyguard condition");
            check(transition.contains("3/2") && transition.contains("2/0"),
                    "wake must still forward actual ON and actual fade completion");
            state.display(0, 2, false);
            state.apply(wire);
            check(wire.ui == 0, "unknown display state must stop even continuing keyguard auth");
            state.display(2, 2, false);
            state.apply(wire);
            check(wire.ui == 1, "normal ON auth must resume after the transition");
            state.cancel();
            state.display(display[0], display[0], true);
            state.apply(wire);
            check(wire.ui == 0, "low-power setting cannot resurrect canceled auth");
        }
        state = ready();
        wire = new Wire();
        state.screenOffUnlock(true);
        state.start(30, 0, 2);
        state.overlay(30, true);
        state.display(2, 1, true);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 0,
                "a fresh request must not start during an unstable display transition");
        state.display(1, 1, true);
        state.apply(wire);
        check(wire.ui == 1, "fresh keyguard auth may start once low-power state is stable");
        state.display(2, 1, true);
        state.lockout(0, 1);
        state.apply(wire);
        check(wire.ui == 0 && wire.operation == 4,
                "lockout during a display transition must still stop the request immediately");
        for (int operation : new int[] {1, 3, 4}) {
            state = ready();
            wire = new Wire();
            state.screenOffUnlock(true);
            state.start(20, 0, operation);
            state.overlay(20, true);
            state.display(3, 3, true);
            state.apply(wire);
            check(wire.ui == 0, "enrollment and app/other auth must remain ON-only");
        }
        System.out.println("PASS: enrollment, auth, request races, fade, low-power preference, wake, lockout, users, death and transport failure");
    }
}
