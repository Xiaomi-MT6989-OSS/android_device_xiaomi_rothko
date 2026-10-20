#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Small native host checks, using real source-tree Binder headers and a fake service.

Not an Android build, live Binder/parcel round trip, link against the arm64 blob,
SELinux check or hardware test. No lunch, Soong, Ninja, images or ADB are invoked.
"""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--source-root", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
root = args.source_root.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
tree = Path(__file__).resolve().parents[1]
clang = root / "prebuilts/clang/host/linux-x86/clang-r547379/bin/clang++"
include = root / "frameworks/native/libs/binder/ndk"
flags = [str(clang), "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
         "-D__INTRODUCED_IN(x)=", "-D__INTRODUCED_IN_FUTURE=", "-D__RENAME(x)=",
         "-I" + str(include / "include_cpp"), "-I" + str(include / "include_ndk"),
         "-I" + str(include / "include_platform"),
         "-I" + str(tree / "controller")]
reader = tree / "controller/StockPqReader.cpp"
# A missing platform header used to shift the call to slot 46. Refuse such an
# environment, rather than letting a successful syntax check hide ABI damage.
sdk_only = [flag for flag in flags if flag != "-I" + str(include / "include_platform")]
negative = subprocess.run([*sdk_only, "-fsyntax-only", str(reader)], text=True, capture_output=True)
assert negative.returncode != 0 and "Stock PQ requires the platform" in negative.stderr
# This also checks the full declaration header against the actual platform
# Binder base class. Verify compiled virtual call indices, not just comments.
ir = output / "reader.ll"
subprocess.run([*flags, "-S", "-emit-llvm", str(reader), "-o", str(ir)], check=True)
text = ir.read_text()
slots = {int(n) for n in re.findall(r"getelementptr inbounds(?: nuw)? ptr, ptr %\w+, i64 (\d+)", text)}
assert {47, 194, 195} <= slots, ("getSamples/version/hash slots differ", slots)

header = (tree / "controller/StockPictureQuality.h").read_text()
methods = re.findall(r"virtual ndk::ScopedAStatus (.*) = 0;", header)
assert len(methods) == 190
custom = {"getDispContentSamples", "getInterfaceVersion", "getInterfaceHash"}
overrides = []
for method in methods:
    name = method.split("(", 1)[0]
    if name in custom:
        continue
    overrides.append("ndk::ScopedAStatus " + method +
                     " override { return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }")
fixture = r'''
#include "StockPqReader.h"
#include <cassert>
#include <iostream>
using namespace rothko::color;
// Only these leaf C functions are faked. Interface/base-class definitions
// themselves are the real platform headers, not an invented six-slot fixture.
struct AStatus { binder_exception_t error; };
extern "C" {
AStatus* AStatus_newOk() { return new AStatus{EX_NONE}; }
AStatus* AStatus_fromExceptionCode(binder_exception_t e) { return new AStatus{e}; }
void AStatus_delete(AStatus* s) { delete s; }
bool AStatus_isOk(const AStatus* s) { return s->error == EX_NONE; }
void AIBinder_decStrong(AIBinder*) {}
void AIBinder_incStrong(AIBinder*) {}
}
namespace aidl::vendor::mediatek::hardware::pq_aidl {
IPictureQuality_AIDL::IPictureQuality_AIDL() = default;
IPictureQuality_AIDL::~IPictureQuality_AIDL() = default;
class Fake final : public IPictureQuality_AIDL {
public:
    ndk::SpAIBinder asBinder() override { return ndk::SpAIBinder(); }
    bool isRemote() override { return true; }
    int32_t version = 7;
    std::string digest = "2e981138bf239a3959d7b4acf90c61416856fc26";
    bool failVersion = false, failHash = false;
    int calls = 0;
    std::function<ndk::ScopedAStatus(parcelable_getDispContentSamples*)> operation;
    ndk::ScopedAStatus getInterfaceVersion(int32_t* out) override {
        *out = version; return failVersion ? ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE) : ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus getInterfaceHash(std::string* out) override {
        *out = digest; return failHash ? ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE) : ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus getDispContentSamples(signed char display,
            const std::vector<int32_t>& channels, parcelable_getDispContentSamples* out) override {
        assert(display == 1 && channels == std::vector<int32_t>{0});
        // Also ensure the production reader never reuses its previous out value.
        assert(out->result == 0 && out->frame == 0 && out->channels.empty());
        calls++; return operation(out);
    }
    // UNUSED_METHODS
};
}
int main() {
    auto service = ndk::SharedRefBase::make<pq::Fake>();
    int64_t now = 10;
    StockPqReader reader([&] { return now; });
    int assertions = 0;
    auto check = [&](bool value) { assert(value); assertions++; };
    check(reader.compatible(*service));
    service->version = 4; service->digest = "1f8b1085be183c24269a6aacac46bb666446e3ea";
    check(reader.compatible(*service));
    service->version = 8; check(!reader.compatible(*service));
    service->version = 7; check(!reader.compatible(*service));
    service->digest = "2e981138bf239a3959d7b4acf90c61416856fc26";
    service->failHash = true; check(!reader.compatible(*service)); service->failHash = false;
    service->failVersion = true; check(!reader.compatible(*service)); service->failVersion = false;
    auto good = [](pq::parcelable_getDispContentSamples* out) {
        out->frame = 123;
        pq::Drm_mtk_channel_hist channel;
        channel.channel = 0; channel.format = 9; channel.binCount = 256;
        channel.bins[0] = 500; // A real black image has population in bin 0.
        out->channels.push_back(channel);
    };
    service->operation = [&](auto* out) { good(out); now = 100; return ndk::ScopedAStatus::ok(); };
    auto sample = reader.read(*service);
    check(sample.transportOk && sample.shapeValid);
    check(sample.readStartedAt == 10); // IO finish did not move the start timestamp.
    check(sample.frame == 123 && sample.bins[0] == 500);
    for (int variant = 0; variant < 8; variant++) {
        service->operation = [&, variant](auto* out) {
            good(out);
            switch (variant) {
                case 0: out->result = -11; break;
                case 1: out->frame = 0; break;
                case 2: out->channels.clear(); break;
                case 3: out->channels.push_back(out->channels.front()); break;
                case 4: out->channels[0].channel = 1; break;
                case 5: out->channels[0].format = 8; break;
                case 6: out->channels[0].binCount = 128; break;
                case 7: out->channels[0].bins[255] = -1; break;
            }
            return ndk::ScopedAStatus::ok();
        };
        auto bad = reader.read(*service);
        check(bad.transportOk && !bad.shapeValid);
    }
    service->operation = [&](auto* out) { good(out); out->channels[0].bins.fill(0); return ndk::ScopedAStatus::ok(); };
    check(!reader.read(*service).shapeValid); // Empty is not black.
    service->operation = [](auto*) { return ndk::ScopedAStatus::ok(); };
    sample = reader.read(*service);
    check(sample.transportOk && !sample.shapeValid && sample.frame == 0 && sample.bins[0] == 0);
    service->operation = [&](auto* out) { good(out); return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE); };
    sample = reader.read(*service);
    check(!sample.transportOk && !sample.shapeValid && sample.frame == 0 && sample.bins[0] == 0);
    service->operation = [&](auto* out) { good(out); out->frame = -1; return ndk::ScopedAStatus::ok(); };
    check(reader.read(*service).shapeValid); // Unsigned wrap belongs to the consumer's ordering.
    check(service->calls == 13);
    std::cout << "PASS: " << assertions << " native reader assertions; real Binder base and virtual slots 47/194/195\n";
}
'''.replace("// UNUSED_METHODS", "\n".join(overrides))
test = output / "reader-test.cpp"
test.write_text(fixture)
binary = output / "reader-test"
subprocess.run([*flags, str(reader), str(test), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
