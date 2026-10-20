#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""AIDL generation, native host syntax/unit checks only; never invokes Android build/ADB."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--source-root", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--aidl", type=Path, required=True)
parser.add_argument("--intermediates", type=Path,
                    help="optional existing outputs for full controller and platform Java API checks")
args = parser.parse_args()
root = args.source_root.resolve()
tree = Path(__file__).resolve().parents[1]
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
includes, generated = output / "include", output / "src"
sources = sorted((tree / "aidl/vendor/xiaomi/rothko/display/brightness").glob("*.aidl"))
common = [str(args.aidl), "--structured", "--min_sdk_version=35", "-I" + str(tree / "aidl")]
subprocess.run([*common, "--lang=ndk", "-h", str(includes), "-o", str(generated),
                *map(str, sources)], check=True)
subprocess.run([*common, "--lang=java", "-o", str(output / "java"),
                *map(str, sources)], check=True)
binder = root / "frameworks/native/libs/binder/ndk"
flags = [str(root / "prebuilts/clang/host/linux-x86/clang-r547379/bin/clang++"),
         "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
         "-D__INTRODUCED_IN(x)=", "-D__INTRODUCED_IN_FUTURE=", "-D__RENAME(x)=",
         "-DBINDER_STABILITY_SUPPORT", "-I" + str(tree / "controller"), "-I" + str(includes),
         *["-I" + str(binder / part) for part in ("include_cpp", "include_ndk", "include_platform")]]
subprocess.run([*flags, "-fsyntax-only", str(tree / "controller/HistogramService.cpp"),
                *map(str, generated.rglob("*.cpp"))], check=True)
test = output / "histogram-session-test"
subprocess.run([*flags, str(tree / "tests/HistogramSessionTest.cpp"), "-o", str(test)], check=True)
subprocess.run([str(test)], check=True)
print("PASS: private AIDL Java/NDK generation and native service/stub syntax (host, no ROM build)")

if args.intermediates:
    intermediate = args.intermediates.resolve()
    # Reuse existing, unchanged frozen platform headers; never regenerate their
    # build graph or invoke a module build to obtain them.
    deps = [root / path for path in (
        "system/libbase/include", "external/libbase/include",
        "frameworks/native/libs/ui/include", "system/core/libsystem/include",
        "system/libhidl/base/include", "system/libhidl/transport/include",
        "system/libhwbinder/include", "system/core/libutils/include",
        "system/core/libutils/binder/include", "system/core/libcutils/include",
        "system/logging/liblog/include", "system/libfmq/include", "system/libfmq/base")
        if (root / path).is_dir()]
    deps += [intermediate / f"hardware/interfaces/graphics/common/{v}/android.hardware.graphics.common@{v}_genc++_headers/gen"
             for v in ("1.0", "1.1", "1.2")]
    deps += [intermediate / path for path in (
        "hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include",
        "hardware/lineage/interfaces/livedisplay/aidl/vendor.lineage.livedisplay-V1-ndk-source/gen/include",
        "device/xiaomi/rothko/displaycolor/vendor.xiaomi.rothko.display.color-ndk-source/gen/include")]
    full = [*flags, "-stdlib=libc++", "-Wno-unused-parameter"]
    for dep in deps:
        full += ["-isystem", str(dep)]
    controller = str(tree / "controller/Controller.cpp")
    subprocess.run([*full, "-fsyntax-only", controller], check=True)
    negative = subprocess.run([*full, "-D__ANDROID_VENDOR__", "-fsyntax-only", controller],
                              capture_output=True, text=True)
    assert negative.returncode and "require the platform static" in negative.stderr
    subprocess.run([*full, "-D__ANDROID_VENDOR__", "-fsyntax-only", str(tree / "service.cpp")], check=True)
    print("PASS: real controller/platform and executable/vendor header checks; wrong variant rejected")
    framework = intermediate / "frameworks/base/framework-minus-apex/android_common/combined/framework.jar"
    base = tree.parent / "brightness/src/org/lineageos/rothko/brightness"
    subprocess.run([str(root / "prebuilts/jdk/jdk21/linux-x86/bin/javac"), "-proc:none",
                    "-cp", str(framework), "-d", str(output / "platform-api-java"),
                    str(base / "OprBrightnessClient.java"), str(base / "StockOprBrightnessState.java"),
                    *map(str, (output / "java").rglob("*.java"))], check=True)
    print("PASS: actual generated Java AIDL/client checked against existing platform framework.jar")
