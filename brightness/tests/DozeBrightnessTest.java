/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import android.view.Display;
import java.util.ArrayList;
import java.util.List;

/** Host protocol/lifecycle checks; no claim of panel or Binder runtime coverage. */
public final class DozeBrightnessTest {
    private static int assertions;
    private static void check(boolean value, String label) {
        assertions++;
        if (!value) throw new AssertionError(label);
    }
    private static final class Sink implements RothkoDozeBrightnessController.Sink {
        final List<Integer> modes = new ArrayList<>();
        boolean ok = true;
        @Override public boolean apply(long id, int mode) {
            check(id == RothkoDozeBrightnessController.PANEL_ID, "only pinned physical display");
            modes.add(mode);
            return ok;
        }
    }
    public static void main(String[] args) {
        Sink sink = new Sink();
        RothkoDozeBrightnessController c = new RothkoDozeBrightnessController(
                RothkoDozeBrightnessController.PANEL_ID, sink);
        check(c.handlesDozeBrightness(Display.STATE_DOZE), "DOZE route");
        check(c.handlesDozeBrightness(Display.STATE_DOZE_SUSPEND), "DOZE_SUSPEND route");
        for (int state : new int[] {0, 1, 2, 5, 6}) {
            check(!c.handlesDozeBrightness(state), "ordinary/unknown route " + state);
        }
        c.onNormalBrightnessApplied();
        check(sink.modes.isEmpty(), "normal boot sends no unsolicited panel command");
        c.applyDozeBrightness(0.01f, 0.01f);
        c.applyDozeBrightness(0.2f, 0.01f);
        c.applyDozeBrightness(0f, 0.01f);
        c.applyDozeBrightness(0.0101f, 0.01f);
        check(sink.modes.equals(List.of(2, 1, 2, 2)), "stock modes and rounded low target");
        c.onNormalBrightnessApplied();
        check(sink.modes.get(4) == 0, "exit resets dedicated route");
        c.onNormalBrightnessApplied();
        check(sink.modes.size() == 5, "no repeated successful reset");
        c.applyDozeBrightness(0.2f, 0.01f);
        c.onNormalBrightnessApplied();
        c.applyDozeBrightness(0.01f, 0.01f);
        check(sink.modes.subList(5, 8).equals(List.of(1, 0, 2)), "AOD/pulse/AOD mode ordering");
        sink.ok = false;
        c.applyDozeBrightness(0.2f, 0.01f);
        c.onNormalBrightnessApplied();
        check(sink.modes.get(9) == 0, "uncertain apply still requires cleanup");
        sink.ok = true;
        c.onNormalBrightnessApplied();
        check(sink.modes.get(10) == 0, "failed reset retried on actual next normal request");
        c.onNormalBrightnessApplied();
        check(sink.modes.size() == 11, "cleanup acknowledgement clears pending state");
        Sink other = new Sink();
        RothkoDozeBrightnessController foreign = new RothkoDozeBrightnessController(42, other);
        check(!foreign.handlesDozeBrightness(Display.STATE_DOZE), "secondary display excluded");
        foreign.applyDozeBrightness(0.2f, 0.01f);
        foreign.onNormalBrightnessApplied();
        check(other.modes.isEmpty(), "secondary display cannot command OEM display zero");
        System.out.println("DozeBrightnessTest passed: " + assertions + " assertions");
    }
}
