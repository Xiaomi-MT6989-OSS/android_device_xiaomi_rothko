/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.rothko.brightness;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.InputSource;

import java.io.IOException;
import java.io.InputStream;
import java.io.StringReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import javax.xml.parsers.DocumentBuilderFactory;

/** Reads the unmodified packaged stock lux/temperature/nits table. No fitted curves. */
final class StockBrightnessConfig {
    private static final int MAX_XML_BYTES = 256 * 1024;
    private final Map<Integer, List<LuxBand>> mConditions;
    final float minimumCapNits;
    final float maximumBoardTemperature;
    final float maximumNtcTemperature;

    private StockBrightnessConfig(Map<Integer, List<LuxBand>> conditions, float minimum,
            float maximumBoardTemperature, float maximumNtcTemperature) {
        mConditions = conditions;
        minimumCapNits = minimum;
        this.maximumBoardTemperature = maximumBoardTemperature;
        this.maximumNtcTemperature = maximumNtcTemperature;
    }

    static StockBrightnessConfig read(InputStream stream) throws Exception {
        byte[] bytes = stream.readNBytes(MAX_XML_BYTES + 1);
        if (bytes.length > MAX_XML_BYTES) throw new IOException("Brightness XML is too large");
        // The shipped table needs neither DTD nor entities. Reject them before any parser IO.
        String xml = new String(bytes, StandardCharsets.UTF_8);
        String upper = xml.toUpperCase(Locale.ROOT);
        if (upper.contains("<!DOCTYPE") || upper.contains("<!ENTITY")) {
            throw new IOException("DTD/entities are not allowed in brightness policy");
        }
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        factory.setExpandEntityReferences(false);
        Element root = factory.newDocumentBuilder()
                .parse(new InputSource(new StringReader(xml))).getDocumentElement();
        require(root.getTagName().equals("thermal-brightness-config"), "Unexpected XML root");
        Map<Integer, List<LuxBand>> conditions = new HashMap<>();
        float minimum = Float.POSITIVE_INFINITY;
        float maximumBoardTemperature = Float.NEGATIVE_INFINITY;
        float maximumNtcTemperature = Float.NEGATIVE_INFINITY;
        for (Element condition : children(root, "thermal-condition-item")) {
            int id = Integer.parseInt(text(condition, "identifier"));
            List<LuxBand> luxBands = new ArrayList<>();
            float previousLuxEnd = Float.NEGATIVE_INFINITY;
            for (Element lux : children(condition, "lux-temperature-pair")) {
                float min = number(lux, "min-inclusive");
                float max = number(lux, "max-exclusive");
                require(min >= 0 && max > min && min >= previousLuxEnd, "Invalid lux interval");
                previousLuxEnd = max;
                List<TemperatureBand> temperatures = new ArrayList<>();
                float previousTempEnd = Float.NEGATIVE_INFINITY;
                for (Element band : children(lux, "temperature-brightness-pair")) {
                    float low = number(band, "min-inclusive");
                    float high = number(band, "max-exclusive");
                    float nits = number(band, "nit");
                    require(high > low && low >= previousTempEnd && nits > 0,
                            "Invalid temperature/nits interval");
                    previousTempEnd = high;
                    minimum = Math.min(minimum, nits);
                    if (id != -3) maximumBoardTemperature = Math.max(maximumBoardTemperature, high);
                    else maximumNtcTemperature = Math.max(maximumNtcTemperature, high);
                    temperatures.add(new TemperatureBand(low, high, nits));
                }
                require(!temperatures.isEmpty(), "Empty temperature table");
                luxBands.add(new LuxBand(min, max, temperatures));
            }
            require(!luxBands.isEmpty() && !conditions.containsKey(id), "Empty/duplicate condition");
            conditions.put(id, luxBands);
        }
        require(conditions.containsKey(0), "Missing default condition");
        require(Float.isFinite(minimum) && Float.isFinite(maximumBoardTemperature), "Empty table");
        return new StockBrightnessConfig(conditions, minimum, maximumBoardTemperature,
                maximumNtcTemperature);
    }

    /** Infinity means no table row applies; unknown condition falls back to stock default 0. */
    float cap(int condition, float lux, float temperature) {
        return cap(mConditions.getOrDefault(condition, mConditions.get(0)), lux, temperature);
    }

    /** Auto mode without a valid ALS sample uses the tightest applicable factory lux band. */
    float capWithoutLux(int condition, float temperature) {
        float cap = Float.POSITIVE_INFINITY;
        for (LuxBand band : mConditions.getOrDefault(condition, mConditions.get(0))) {
            cap = Math.min(cap, band.at(temperature));
        }
        return cap;
    }

    float ntcCap(float lux, float temperature) {
        List<LuxBand> bands = mConditions.get(-3);
        if (bands == null) return Float.POSITIVE_INFINITY;
        return cap(bands, lux, temperature);
    }

    private static float cap(List<LuxBand> bands, float lux, float temperature) {
        for (LuxBand band : bands) {
            if (lux >= band.min && lux < band.max) return band.at(temperature);
        }
        return Float.POSITIVE_INFINITY;
    }

    private static List<Element> children(Element parent, String tag) {
        List<Element> result = new ArrayList<>();
        NodeList nodes = parent.getChildNodes();
        for (int i = 0; i < nodes.getLength(); i++) {
            Node node = nodes.item(i);
            if (node instanceof Element && ((Element) node).getTagName().equals(tag)) {
                result.add((Element) node);
            }
        }
        return result;
    }

    private static String text(Element parent, String tag) throws IOException {
        List<Element> values = children(parent, tag);
        require(values.size() == 1, "Missing/duplicate " + tag);
        return values.get(0).getTextContent().trim();
    }

    private static float number(Element parent, String tag) throws IOException {
        float result = Float.parseFloat(text(parent, tag));
        require(Float.isFinite(result), "Non-finite " + tag);
        return result;
    }

    private static void require(boolean condition, String message) throws IOException {
        if (!condition) throw new IOException(message);
    }

    private static final class LuxBand {
        final float min;
        final float max;
        final List<TemperatureBand> temperatures;
        LuxBand(float min, float max, List<TemperatureBand> temperatures) {
            this.min = min;
            this.max = max;
            this.temperatures = temperatures;
        }
        float at(float temperature) {
            for (TemperatureBand band : temperatures) {
                if (temperature >= band.min && temperature < band.max) return band.nits;
            }
            return Float.POSITIVE_INFINITY;
        }
    }

    private static final class TemperatureBand {
        final float min;
        final float max;
        final float nits;
        TemperatureBand(float min, float max, float nits) {
            this.min = min;
            this.max = max;
            this.nits = nits;
        }
    }
}
