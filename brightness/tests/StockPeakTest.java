/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

/** Factory peak branch boundaries; no Android IO and no physical brightness writes. */
public final class StockPeakTest {
    private static int assertions;
    private static void equal(float expected, float actual, String label) {
        assertions++;
        if (Float.compare(expected, actual) != 0) throw new AssertionError(label
                + ": expected=" + expected + " actual=" + actual);
    }
    public static void main(String[] args) {
        var state = new StockOprBrightnessState();
        long session = state.beginSession();
        int[] bins = new int[256]; bins[210] = 100;
        equal(1600, state.peakCap(true, 100001, true, 0), "missing gray cannot grant peak");
        state.accept(session, 0, 0, true, 0, 1, 0, 9, 256, bins);
        equal(1600, state.peakCap(true, 100001, true, 0), "first frame is baseline only");
        state.accept(session, 200, 200, true, 0, 2, 0, 9, 256, bins);
        equal(4500, state.peakCap(true, 100001, true, 200), "stock gray 210 is inclusive");
        equal(1600, state.peakCap(true, 100000, true, 200), "stock lux must be strictly greater");
        equal(4500, state.peakCap(true, Math.nextUp(100000f), true, 200), "next lux above boundary");
        equal(1600, state.peakCap(false, 100001, true, 200), "manual cannot grant special peak");
        equal(1600, state.peakCap(true, 100001, false, 200), "SDR cannot grant special peak");
        for (float lux : new float[]{Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, -1}) {
            equal(1600, state.peakCap(true, lux, true, 200), "invalid lux");
        }
        bins[210] = 0; bins[211] = 100;
        state.accept(session, 400, 400, true, 0, 3, 0, 9, 256, bins);
        equal(1600, state.peakCap(true, 100001, true, 400), "gray 211 rejects peak");
        bins[211] = 0; bins[0] = 100;
        state.accept(session, 600, 600, true, 0, 4, 0, 9, 256, bins);
        equal(4500, state.peakCap(true, 100001, true, 600), "valid black frame is legal gray zero");
        equal(4500, state.peakCap(true, 100001, true, 3099), "valid immediately before deadline");
        equal(1600, state.peakCap(true, 100001, true, 3100), "expiry without queue dispatch");
        state.accept(session, 3000, 3000, true, 0, 4, 0, 9, 256, bins);
        equal(1600, state.peakCap(true, 100001, true, 3100), "duplicate cannot renew peak");
        state.accept(session, 3200, 3200, true, 0, 5, 0, 9, 256, bins);
        equal(4500, state.peakCap(true, 100001, true, 3200), "new frame restores peak input");
        state.accept(session, 3400, 3400, false, 0, 6, 0, 9, 256, bins);
        equal(1600, state.peakCap(true, 100001, true, 3400), "transport failure revokes peak");
        state.endSession(session);
        equal(1600, state.peakCap(true, 100001, true, 3400), "inactive cannot grant peak");
        session = state.beginSession();
        state.accept(session, 3600, 3600, true, 0, 9, 0, 9, 256, bins);
        equal(1600, state.peakCap(true, 100001, true, 3600), "new content needs baseline again");
        System.out.println("PASS: " + assertions + " stock peak/lux/gray/freshness assertions");
    }
}
