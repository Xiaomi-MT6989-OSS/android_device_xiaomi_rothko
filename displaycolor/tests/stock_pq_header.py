#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Recover/check ABI declarations against the pinned stock V4 library, without a build.

Only declarations are recovered. All Binder proxy code remains the factory binary.
Order comes from ELF relocations in BpPictureQuality_AIDL's vtable, not symbol order.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--library", type=Path, required=True)
parser.add_argument("--llvm-bin", type=Path, required=True)
parser.add_argument("--write", action="store_true")
args = parser.parse_args()
assert hashlib.sha256(args.library.read_bytes()).hexdigest() == (
    "1ebe213e0c5e7c638dff35aa688207e5b5f577bd3aca7693afca3d48c582eab1")
relocs = subprocess.check_output(
    [args.llvm_bin / "llvm-readelf", "-r", args.library], text=True)
methods = []
for row in relocs.splitlines():
    fields = row.split()
    if len(fields) < 5 or not re.fullmatch("[0-9a-f]{16}", fields[0]):
        continue
    address = int(fields[0], 16)
    if 0x5d540 <= address < 0x5db30:
        methods.append((address, fields[4]))
assert len(methods) == 190
assert [a for a, _ in methods] == list(range(0x5d540, 0x5db30, 8))
names = subprocess.check_output(
    [args.llvm_bin / "llvm-cxxfilt", *[name for _, name in methods]], text=True).splitlines()
prefix = "aidl::vendor::mediatek::hardware::pq_aidl::"
assert all(name.startswith(prefix + "BpPictureQuality_AIDL::") for name in names)
assert "::getDispContentSamples(" in names[41]
assert methods[41][0] - 0x5d510 == 0x178
assert "::getInterfaceVersion(" in names[-2]
assert "::getInterfaceHash(" in names[-1]
types = set(re.findall(re.escape(prefix) + r"(\w+)", "\n".join(names)))
types.remove("BpPictureQuality_AIDL")
header = """// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
// ABI declarations recovered from stock OS3.0.307.0; see tests/stock_pq_header.py.
// Do not implement this interface or replace the factory V4 proxy. All 190 virtual
// declarations retain their ELF vtable order, including methods we never call.
#pragma once
#include <android/binder_interface_utils.h>
#ifndef HAS_BINDER_SHELL_COMMAND
#error Stock PQ requires the platform libbinder_ndk headers, including binder_shell.h
#endif
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::common { class NativeHandle; }
namespace aidl::vendor::mediatek::hardware::pq_aidl {
"""
for name in sorted(types):
    if name in ("Result", "PQFeatureID"):
        header += f"enum class {name} : int32_t;\n"
    else:
        header += f"class {name};\n"
header += """
// Names below describe verified offsets, not unverified vendor semantic names.
class Drm_mtk_channel_hist {
public:
    int32_t channel = 0;
    int32_t format = 0;
    std::array<int32_t, 256> bins{};
    int32_t binCount = 0;
    binder_status_t readFromParcel(const AParcel*);
    binder_status_t writeToParcel(AParcel*) const;
};
class parcelable_getDispContentSamples {
public:
    int32_t result = 0;
    int32_t frame = 0;
    std::vector<Drm_mtk_channel_hist> channels;
    binder_status_t readFromParcel(const AParcel*);
    binder_status_t writeToParcel(AParcel*) const;
};
static_assert(sizeof(Drm_mtk_channel_hist) == 0x40c);
static_assert(offsetof(Drm_mtk_channel_hist, bins) == 8);
static_assert(offsetof(Drm_mtk_channel_hist, binCount) == 0x408);
static_assert(offsetof(parcelable_getDispContentSamples, channels) == 8);
static_assert(sizeof(parcelable_getDispContentSamples) == 32);

class IPictureQuality_AIDL : public ndk::ICInterface {
public:
    static const char* descriptor;
    IPictureQuality_AIDL();
    virtual ~IPictureQuality_AIDL();
    static const std::string hash;
    static std::shared_ptr<IPictureQuality_AIDL> fromBinder(const ndk::SpAIBinder&);
"""
for (address, _), name in zip(methods, names):
    method = name.removeprefix(prefix + "BpPictureQuality_AIDL::")
    method = method.replace(prefix, "").replace("std::__1::", "std::")
    header += f"    // vtable +0x{address - 0x5d510:x}\n"
    header += f"    virtual ndk::ScopedAStatus {method} = 0;\n"
header += "};\n} // namespace aidl::vendor::mediatek::hardware::pq_aidl\n"
destination = Path(__file__).resolve().parents[1] / "controller/StockPictureQuality.h"
if args.write:
    destination.write_text(header)
else:
    assert destination.read_text() == header, "StockPictureQuality.h differs from pinned ABI"
print("PASS: stock SHA-256, 190 ordered ABI declarations, getter slot 0x178, result layout")
