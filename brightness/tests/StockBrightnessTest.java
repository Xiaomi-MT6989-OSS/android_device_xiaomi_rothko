/* SPDX-License-Identifier: Apache-2.0 */
package org.lineageos.rothko.brightness;

import java.io.ByteArrayInputStream;
import java.io.FileInputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/** Runs on a host JDK; no phone, Android build graph or fabricated sysfs temperatures. */
public final class StockBrightnessTest {
    private static int assertions;
    private static void equal(float expected, float actual, String what) {
        assertions++;
        if (Float.compare(expected, actual) != 0) {
            throw new AssertionError(what + ": expected=" + expected + " actual=" + actual);
        }
    }
    private static void check(boolean condition, String what) {
        assertions++;
        if (!condition) throw new AssertionError(what);
    }
    private static void reject(String xml) throws Exception {
        try {
            StockBrightnessConfig.read(new ByteArrayInputStream(xml.getBytes(StandardCharsets.UTF_8)));
        } catch (Exception expected) { assertions++; return; }
        throw new AssertionError("Accepted malformed table");
    }
    public static void main(String[] args) throws Exception {
        StockBrightnessConfig config;
        try (FileInputStream in = new FileInputStream(args[0])) {
            config = StockBrightnessConfig.read(in);
        }
        equal(160, config.minimumCapNits, "factory minimum cap");
        equal(100, config.maximumBoardTemperature, "table upper temperature");
        for (String line : Files.readAllLines(Path.of(args[1]))) {
            String[] f = line.split(",");
            equal(Float.parseFloat(f[3]), config.cap(Integer.parseInt(f[0]),
                    Float.parseFloat(f[1]), Float.parseFloat(f[2])), line);
        }
        StockBrightnessState state = new StockBrightnessState(config);
        equal(160, state.cap(true, 100, 0, 0, false), "missing temperature fails closed");
        state.update(39, Float.NaN);
        equal(500, state.cap(true, 100, 0, 0, false), "auto indoor band");
        equal(600, state.cap(false, 100, 0, 0, false), "manual always uses 6000 lux");
        equal(500, state.cap(true, Float.NaN, 0, 0, false), "missing ALS tightest band");
        equal(100, state.cap(true, 100, 0, 100, false), "independent safety cap");
        equal(160, state.cap(true, 100, 0, -1, false), "negative safety invalid");
        equal(160, state.cap(true, 100, 0, Float.NaN, false), "NaN safety invalid");
        equal(160, state.cap(true, 100, 0, 0, true), "required NTC missing");
        state.update(39, 52);
        equal(1000, config.ntcCap(100, 52), "stock NTC table");
        equal(500, state.cap(true, 100, 0, 0, true), "NTC cannot raise board cap");
        state.update(39, 101);
        equal(160, state.cap(true, 100, 0, 0, true), "NTC above table fails closed");
        state.update(100, 52);
        equal(160, state.cap(true, 100, 0, 0, true), "above table cannot mean unlimited");
        equal(10, state.cap(true, 100, 0, 10, true), "invalid temperature preserves tighter safety");
        state.invalidate();
        equal(160, state.cap(true, 100, 0, 0, false), "invalidated snapshot");
        state.update(39.9f, Float.NaN);
        state.update(40.1f, Float.NaN);
        equal(39.9f, state.appliedBoardTemperature(), "stock half-degree magnitude gap");
        state.update(40.5f, Float.NaN);
        equal(40.5f, state.appliedBoardTemperature(), "gap crossing accepted");
        state.update(40.99f, Float.NaN);
        equal(40.5f, state.appliedBoardTemperature(), "same integer stock rule");
        check(StockBrightnessState.sameTemperature(49.1f, 50f, true), "NTC 1 degree gap");
        check(!StockBrightnessState.sameTemperature(49.1f, 50.2f, true), "NTC gap crossed");
        check(!StockBrightnessState.sameTemperature(Float.NaN, 40, false), "first valid sample");
        String xml = Files.readString(Path.of(args[0]));
        reject(xml.replaceFirst("<nit>600</nit>", "<nit>NaN</nit>"));
        reject(xml.replaceFirst("<identifier>0</identifier>", "<identifier>20</identifier>"));
        reject(xml.replaceFirst("<max-exclusive>38</max-exclusive>",
                "<max-exclusive>35</max-exclusive>"));
        reject("<!DOCTYPE a [<!ENTITY b SYSTEM 'file:///etc/passwd'>]>" + xml);
        System.out.println("PASS: " + assertions + " factory boundary/state/parser assertions");
    }
}
