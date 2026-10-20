#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host unit checks only: no lunch/Soong/Ninja, images, ADB, or device writes.

Minimal Android scheduling fixtures exercise the actual reader code; they do not constitute
Android API, SELinux, class-loader, DDC conversion, or device integration verification.
"""
import argparse
import itertools
from pathlib import Path
import struct
import subprocess
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument("--source-root", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
root = args.source_root.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
tree = root / "device/xiaomi/rothko"
config = tree / "configs/displayconfig/common_multi_factor_thermal_brightness_control.xml"
f32 = lambda value: struct.unpack("f", struct.pack("f", value))[0]
# Independent oracle comes from the stock XML, not calls to the Java implementation.
conditions = {}
for condition in ET.parse(config).getroot().findall("thermal-condition-item"):
    bands = []
    for lux in condition.findall("lux-temperature-pair"):
        bands.append((float(lux.findtext("min-inclusive")),
                      float(lux.findtext("max-exclusive")),
                      [(float(t.findtext("min-inclusive")),
                        float(t.findtext("max-exclusive")), float(t.findtext("nit")))
                       for t in lux.findall("temperature-brightness-pair")]))
    conditions[int(condition.findtext("identifier"))] = bands
rows = []
for identifier, bands in conditions.items():
    lux_values = {f32(value + delta) for lo, hi, _ in bands
                  for value in (lo, hi) for delta in (-0.125, 0, 0.125)}
    temp_values = {f32(value + delta) for _, _, temps in bands for lo, hi, _ in temps
                   for value in (lo, hi) for delta in (-0.125, 0, 0.125)}
    for lux, temp in itertools.product(sorted(lux_values), sorted(temp_values)):
        expected = "Infinity"
        for lo, hi, temps in bands:
            if lo <= lux < hi:
                for low, high, cap in temps:
                    if low <= temp < high:
                        expected = str(cap)
                        break
                break
        rows.append(f"{identifier},{lux},{temp},{expected}")
rows.extend("99999," + row.split(",", 1)[1] for row in rows.copy() if row.startswith("0,"))
vectors = output / "boundary-cases.csv"
vectors.write_text("\n".join(rows) + "\n")

fixtures = {
    "android/os/SystemClock.java": """package android.os;
public class SystemClock { public static long now; public static long elapsedRealtime() { return now; } }
""",
    "android/os/Handler.java": """package android.os;
import java.util.*;
public class Handler {
 private record Task(long when, long order, Runnable run) {}
 private final PriorityQueue<Task> q = new PriorityQueue<>(Comparator.comparingLong(Task::when).thenComparingLong(Task::order));
 private long order;
 public Handler() {} public Handler(Object looper) {}
 public boolean post(Runnable r) { return postDelayed(r, 0); }
 public boolean postDelayed(Runnable r, long delay) { q.add(new Task(SystemClock.now + delay, order++, r)); return true; }
 public void removeCallbacks(Runnable r) { q.removeIf(t -> t.run() == r); }
 public void runDue() { int limit = 1000; while (!q.isEmpty() && q.peek().when() <= SystemClock.now) {
  if (--limit == 0) throw new AssertionError("unbounded handler loop"); q.remove().run().run(); } }
 public int pending() { return q.size(); }
}
""",
    "android/os/HandlerThread.java": """package android.os;
public class HandlerThread { public HandlerThread(String n) {} public void start() {}
 public Object getLooper() { return null; } public boolean quitSafely() { return true; } }
""",
    "android/os/SystemProperties.java": """package android.os;
public class SystemProperties { public static boolean getBoolean(String k, boolean fallback) { return fallback; } }
""",
    "android/view/Display.java": """package android.view;
public class Display { public static final int STATE_OFF=1, STATE_ON=2, DEFAULT_DISPLAY=0; }
""",
    "android/content/res/Resources.java": """package android.content.res;
public class Resources { private final String name; public Resources(String n) { name=n; }
 public String getString(int id) { return name; } }
""",
    "android/content/Context.java": """package android.content;
public class Context { private String name=""; public Context() {} public Context(String n) { name=n; }
 public android.content.res.Resources getResources() { return new android.content.res.Resources(name); } }
""",
    "android/util/Slog.java": """package android.util;
public class Slog { public static int e(String t,String m,Throwable e) { return 0; }
 public static int w(String t,String m) { return 0; } }
""",
    "com/android/internal/R.java": """package com.android.internal;
public class R { public static class string { public static int config_deviceSpecificBrightnessPolicy=1; } }
""",
    "android/os/PowerManager.java": """package android.os;
public class PowerManager { public static final float BRIGHTNESS_MIN=0, BRIGHTNESS_MAX=1; }
""",
    "android/hardware/display/BrightnessInfo.java": """package android.hardware.display;
public class BrightnessInfo { public static final int BRIGHTNESS_MAX_REASON_THERMAL=1; }
""",
    "android/hardware/display/DisplayManagerInternal.java": """package android.hardware.display;
public class DisplayManagerInternal { public static class DisplayPowerRequest { public boolean useAutoBrightness; } }
""",
    "com/android/server/display/DisplayDeviceConfig.java": """package com.android.server.display;
public class DisplayDeviceConfig { public float[] getNits() { return new float[]{2,500}; }
 public float getBacklightFromNits(float n) { return (n-2)/498; }
 public float getBrightnessFromBacklight(float b) { return b; } }
""",
    "com/android/server/display/brightness/BrightnessReason.java": """package com.android.server.display.brightness;
public class BrightnessReason { public static int MODIFIER_THROTTLED=1; public void addModifier(int m) {} }
""",
    "com/android/server/display/DisplayBrightnessState.java": """package com.android.server.display;
import com.android.server.display.brightness.BrightnessReason;
public class DisplayBrightnessState { public static class Builder {
 private float max=1, brightness; private int reason; public boolean slow=true;
 public void setMaxBrightness(float x) { max=x; } public float getMaxBrightness() { return max; }
 public void setBrightness(float x) { brightness=x; } public float getBrightness() { return brightness; }
 public void setBrightnessMaxReason(int x) { reason=x; } public int getBrightnessMaxReason() { return reason; }
 public void setIsSlowChange(boolean b) { slow=b; } public BrightnessReason getBrightnessReason() { return new BrightnessReason(); }
} }
""",
    "com/android/server/display/brightness/clamper/BrightnessClamperController.java": """package com.android.server.display.brightness.clamper;
import com.android.server.display.DisplayDeviceConfig;
public class BrightnessClamperController {
 public interface ClamperChangeListener { void onChanged(); }
 public interface DisplayDeviceDataListener { void onDisplayChanged(DisplayDeviceData d); }
 public interface StatefulModifier { void applyStateChange(ModifiersAggregatedState s); }
 public static class ModifiersAggregatedState { float mMaxBrightness=1; int mMaxBrightnessReason; }
 public static class DisplayDeviceData { DisplayDeviceConfig mDisplayDeviceConfig; int mDisplayId;
  public DisplayDeviceData(DisplayDeviceConfig c,int id) { mDisplayDeviceConfig=c; mDisplayId=id; } }
}
""",
}
sources = []
for relative, text in fixtures.items():
    path = output / "fixtures" / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    sources.append(path)
sources += list((tree / "brightness/src").rglob("*.java"))
sources += list((tree / "brightness/tests").glob("*.java"))
sources.append(root / "frameworks/base/services/core/java/com/android/server/display/brightness/DeviceBrightnessPolicy.java")
for filename in ("DeviceBrightnessModifier.java", "BrightnessStateModifier.java"):
    sources.append(root / "frameworks/base/services/core/java/com/android/server/display/brightness/clamper" / filename)
jdk = root / "prebuilts/jdk/jdk21/linux-x86/bin"
classes = output / "classes"
classes.mkdir(exist_ok=True)
subprocess.run([str(jdk / "javac"), "-d", str(classes), *map(str, sources)], check=True)
for test in ("StockBrightnessTest", "PolicyLifecycleTest"):
    subprocess.run([str(jdk / "java"), "-cp", str(classes),
                    "org.lineageos.rothko.brightness." + test, str(config), str(vectors)], check=True)
subprocess.run([str(jdk / "java"), "-cp", str(classes),
                "com.android.server.display.brightness.clamper.ModifierPipelineTest"], check=True)
