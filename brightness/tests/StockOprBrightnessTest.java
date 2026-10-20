/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import static org.lineageos.rothko.brightness.StockOprBrightnessState.Reception.*;

/** Factory boundary vectors and protocol/lifecycle counterexamples; no phone or Android build. */
public final class StockOprBrightnessTest {
    private static int assertions;
    private static void equal(Object expected, Object actual, String what) {
        assertions++;
        if (!expected.equals(actual)) throw new AssertionError(what + ": " + expected + " != " + actual);
    }
    private static void cap(float expected, StockOprBrightnessState state, float lux, long now) {
        equal(expected, state.cap(true, lux, false, now), "cap");
    }
    private static int[] histogram(int gray) {
        int[] bins = new int[256];
        bins[gray] = 1220 * 2712;
        return bins;
    }
    private static StockOprBrightnessState.Reception sample(StockOprBrightnessState s, long g,
            int frame, int gray, long now) {
        return s.accept(g, now, now, true, 0, frame, 0, 9, 256, histogram(gray));
    }
    public static void main(String[] args) {
        StockOprBrightnessState s = new StockOprBrightnessState();
        long g = s.beginSession();
        cap(1000, s, 30000, 0);
        equal(BASELINE, sample(s, g, 100, 0, 0), "first cached frame cannot grant boost");
        cap(1000, s, 30000, 0);
        equal(VALID, sample(s, g, 101, 0, 1), "valid black differs from empty histogram");
        cap(1600, s, 30000, 1);
        equal(0, s.gray(), "black gray");
        equal(3308640L, s.pixels(), "black has nonzero pixels");
        cap(1000, s, 20000, 1);
        cap(1600, s, Math.nextUp(20000f), 1);
        cap(1000, s, Float.NaN, 1);
        cap(1000, s, Float.POSITIVE_INFINITY, 1);
        equal(Float.POSITIVE_INFINITY, s.cap(false, 30000, false, 1), "manual outside SDR OPR");
        equal(Float.POSITIVE_INFINITY, s.cap(true, 30000, true, 1), "HDR outside SDR OPR");
        // Explicit factory boundary pairs, including equality and the thirteenth/final bucket.
        int[][] cases = {
            {0,1600},{119,1600},{120,1600},{121,1550},{130,1550},{131,1500},
            {140,1500},{141,1450},{150,1450},{151,1400},{160,1400},{161,1350},
            {170,1350},{171,1300},{180,1300},{181,1250},{190,1250},{191,1200},
            {200,1200},{201,1150},{210,1150},{211,1100},{220,1100},{221,1050},
            {230,1050},{231,1000},{255,1000}
        };
        int frame = 101;
        long time = 1;
        for (int[] c : cases) {
            equal(VALID, sample(s, g, ++frame, c[0], ++time), "boundary valid");
            cap(c[1], s, 30000, time);
        }
        equal(VALID, sample(s, g, ++frame, 0, ++time), "fresh low gray");
        long acceptedAt = time;
        equal(DUPLICATE, sample(s, g, frame, 255, acceptedAt + 2000), "same frame cannot change gray");
        cap(1600, s, 30000, acceptedAt + 2499);
        cap(1000, s, 30000, acceptedAt + 2500);
        equal(VALID, sample(s, g, ++frame, 120, acceptedAt + 2501), "advanced frame recovers");
        equal(OLD_REPLY, s.accept(g, acceptedAt, acceptedAt + 2501, false, 1,
                0, 1, 1, 0, null), "late failed reply cannot revoke newer good sample");
        cap(1600, s, 30000, acceptedAt + 2501);
        time = acceptedAt + 2502;
        equal(INVALID, s.accept(g, time, time, true, 0, ++frame, 0, 9, 256, new int[256]),
                "zeroed ioctl failure is not a black frame");
        cap(1000, s, 30000, time);
        equal(VALID, sample(s, g, ++frame, 0, ++time), "recovery after empty sample");
        int[][] badMetadata = {{0,9,255},{1,9,256},{0,8,256}};
        for (int[] m : badMetadata) {
            equal(INVALID, s.accept(g, ++time, time, true, 0, ++frame,
                    m[0],m[1],m[2],histogram(0)), "wrong metadata");
            cap(1000, s, 30000, time);
        }
        equal(INVALID, s.accept(g, ++time, time, false, 0, ++frame,0,9,256,histogram(0)),
                "Binder error with nonzero bins");
        equal(INVALID, s.accept(g, ++time, time, true, 3, ++frame,0,9,256,histogram(0)),
                "service error with nonzero bins");
        equal(INVALID, s.accept(g, ++time, time, true, 0, 0,0,9,256,histogram(0)),
                "not-ready frame zero");
        equal(INVALID, s.accept(g, ++time, time, true, 0, ++frame,0,9,256,null), "null bins");
        equal(INVALID, s.accept(g, ++time, time, true, 0, ++frame,0,9,256,new int[255]),
                "truncated bins");
        equal(INVALID, s.accept(g, ++time, time, true, 0, ++frame,0,9,256,new int[257]),
                "wrong bins length");
        int[] negative = histogram(0); negative[255] = -1;
        equal(INVALID, s.accept(g, ++time, time, true, 0, ++frame,0,9,256,negative),
                "negative count");
        equal(INVALID, s.accept(g, ++time, time + 2500, true, 0, ++frame,0,9,256,histogram(0)),
                "blocked read already expired");
        equal(INVALID, s.accept(g, ++time, time - 1, true, 0, ++frame,0,9,256,histogram(0)),
                "future timestamp");
        equal(VALID, sample(s, g, ++frame, 0, ++time), "good sample before generation change");
        long old = g;
        g = s.beginSession();
        equal(OLD_SESSION, sample(s, old, ++frame, 0, ++time), "old connection sample");
        cap(1000, s, 30000, time);
        equal(BASELINE, sample(s, g, frame, 0, ++time), "new session discards cache baseline");
        equal(VALID, sample(s, g, ++frame, 0, ++time), "new session fresh frame");
        s.endSession(old);
        cap(1600, s, 30000, time);
        s.endSession(g);
        equal(INACTIVE, sample(s, g, ++frame, 0, ++time), "ended statistics subscription");
        cap(1000, s, 30000, time);

        g = s.beginSession();
        equal(BASELINE, sample(s, g, -2, 0, ++time), "unsigned counter baseline");
        equal(VALID, sample(s, g, -1, 0, ++time), "unsigned counter above signed max");
        equal(VALID, sample(s, g, 1, 0, ++time), "unsigned wrap advances");
        equal(RESET, sample(s, g, -5, 0, ++time), "backwards counter establishes new baseline");
        cap(1000, s, 30000, time);
        equal(VALID, sample(s, g, -4, 0, ++time), "counter progresses after reset");

        int[] split = new int[256]; split[0] = Integer.MAX_VALUE; split[255] = Integer.MAX_VALUE;
        equal(VALID, s.accept(g, ++time, time, true, 0,-3,0,9,256,split), "wide accumulation");
        equal(127, s.gray(), "integer weighted mean with 64-bit intermediate");
        equal(4294967294L, s.pixels(), "64-bit total");
        cap(1550, s, 30000, time);
        cap(1000, s, 30000, time - 1);
        int[] mixed = new int[256]; mixed[120] = 1; mixed[121] = 1;
        equal(VALID, s.accept(g, ++time, time, true, 0,-2,0,9,256,mixed),
                "mixed threshold bins");
        equal(120, s.gray(), "stock mean truncates rather than rounds at boundary");
        cap(1600, s, 30000, time);
        mixed[120] = 0; mixed[121] = 2;
        equal(VALID, s.accept(g, ++time, time, true, 0,-1,0,9,256,mixed),
                "next integer threshold bin");
        equal(121, s.gray(), "integer mean crosses bucket");
        cap(1550, s, 30000, time);
        System.out.println("PASS: " + assertions + " stock OPR/protocol/lifecycle assertions");
    }
}
