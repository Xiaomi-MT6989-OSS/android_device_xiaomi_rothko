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
        System.out.println("PASS: enrollment, auth, request races, fade, wake, lockout, users, death and transport failure");
    }
}
