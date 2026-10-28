#!/usr/bin/env python3
# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
"""Compile byte-identical production Controller.cpp with host IPC/histogram fakes.

No Android build or phone access. Private access is enabled only in this host
test executable, for deterministic scheduling and invoking the real timeout
handler without waiting twelve seconds. This does not validate Binder, panel,
filesystem persistence or the real HistogramService.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--compiler', default='g++')
p.add_argument('--scenario', default='all')
args = p.parse_args()
tests = Path(__file__).resolve().parent
source = tests.parent / 'controller'
out = args.output.resolve()
if out.exists() or out.is_relative_to(tests.parent.parent):
    p.error('output must be new and outside the device source tree')
out.mkdir(parents=True)
for name in ('Controller.cpp', 'Controller.h', 'StockDisplayFeature.h'):
    shutil.copyfile(source / name, out / name)
    assert (out / name).read_bytes() == (source / name).read_bytes()

runtime = r'''#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <unistd.h>
using binder_status_t = int;
constexpr int STATUS_OK=0, EX_ILLEGAL_ARGUMENT=-3, EX_SECURITY=-1;
struct AParcel {};
struct AIBinder { void* object; };
struct AIBinder_DeathRecipient {
    void (*unlinked)(void*) = nullptr;
    std::vector<void*> cookies;
    ~AIBinder_DeathRecipient() { if (unlinked) for (void* c:cookies) unlinked(c); }
};
inline AIBinder_DeathRecipient* AIBinder_DeathRecipient_new(void (*)(void*)) {
    return new AIBinder_DeathRecipient;
}
inline void AIBinder_DeathRecipient_setOnUnlinked(AIBinder_DeathRecipient* r,void (*f)(void*)) {r->unlinked=f;}
inline int AIBinder_linkToDeath(AIBinder*, AIBinder_DeathRecipient* r,void* c) {r->cookies.push_back(c);return 0;}
inline int AIBinder_getCallingUid() {return 1000;}
namespace ndk {
class SpAIBinder {
    std::shared_ptr<AIBinder> p;
public:
    SpAIBinder() = default;
    explicit SpAIBinder(AIBinder* b):p(b,[](AIBinder*){}) {}
    explicit SpAIBinder(std::shared_ptr<AIBinder> b):p(std::move(b)) {}
    AIBinder* get() const {return p.get();}
};
using ScopedAIBinder_DeathRecipient=std::unique_ptr<AIBinder_DeathRecipient>;
class ScopedAStatus {
    int error=0;
public:
    static ScopedAStatus ok(){return {};}
    static ScopedAStatus fromServiceSpecificError(int e){ScopedAStatus s;s.error=e;return s;}
    static ScopedAStatus fromExceptionCode(int e){return fromServiceSpecificError(e);}
    bool isOk() const{return error==0;}
};
class ICInterface {
    std::shared_ptr<AIBinder> binder=std::make_shared<AIBinder>(AIBinder{this});
public:
    virtual ~ICInterface()=default;
    SpAIBinder asBinder(){return SpAIBinder(binder);}
};
class SharedRefBase {
public:
    template<class T,class... A> static std::shared_ptr<T> make(A&&... a){return std::make_shared<T>(std::forward<A>(a)...);}
};
template<class T> class BnCInterface: public T {
protected:
    virtual SpAIBinder createBinder(){return this->asBinder();}
};
}
struct HostLog {template<class T> HostLog& operator<<(const T&){return *this;}};
#define LOG(level) HostLog()
namespace android::base {
class unique_fd {
    int fd;
public:
    explicit unique_fd(int f):fd(f){} ~unique_fd(){if(fd>=0)::close(fd);}
    bool ok()const{return fd>=0;} int get()const{return fd;}
};
inline bool WriteFully(int fd,const void* p,size_t n){return ::write(fd,p,n)==static_cast<ssize_t>(n);}
}
namespace android::ui {
enum class Dataspace:int {UNKNOWN=0,V0_SRGB=1,DISPLAY_P3=2,BT2020_HLG=3,BT2020_ITU_HLG=4};
}
namespace aidl::vendor::lineage::livedisplay {
struct DisplayMode {int32_t id=0;std::string name;};
class BnDisplayModes:public ndk::ICInterface {
public:
    virtual ndk::ScopedAStatus getDisplayModes(std::vector<DisplayMode>*)=0;
    virtual ndk::ScopedAStatus getCurrentDisplayMode(DisplayMode*)=0;
    virtual ndk::ScopedAStatus getDefaultDisplayMode(DisplayMode*)=0;
    virtual ndk::ScopedAStatus setDisplayMode(int32_t,bool)=0;
};
}
namespace aidl::vendor::xiaomi::rothko::display::color {
struct DisplayState {
    int64_t sfSession=0,connection=0,sequence=0,physicalId=0;
    bool connected=false,synchronizedState=false,contentKnown=false,enabled=false,managed=false;
    int32_t sdrCandidate=0,hdrCandidate=0,outputCandidate=0;
    bool forceHdrClientComposition=false,profileKnown=false;
    int64_t policyRequestId=0,policyRevision=0;
    int32_t colorMode=0,dataspace=0,renderIntent=0,hwcResult=0;
    bool profileRejected=false,powerKnown=false;
    int32_t requested=0,lastAttempted=0;
    bool accepted=false;
};
class IColorPolicyCallback:public ndk::ICInterface {
public:virtual ndk::ScopedAStatus onPolicy(int64_t,int64_t,int32_t,bool)=0;
};
class BnRothkoDisplayColor:public ndk::ICInterface {
public:
    virtual ndk::ScopedAStatus setListener(int64_t,int64_t,const std::shared_ptr<IColorPolicyCallback>&)=0;
    virtual ndk::ScopedAStatus reportState(const DisplayState&)=0;
};
}
namespace aidl::vendor::xiaomi::rothko::display::brightness {
class BnRothkoDozeBrightness:public ndk::ICInterface {
public:virtual ndk::ScopedAStatus apply(int64_t,int32_t,bool*)=0;
};
}
AIBinder* AServiceManager_checkService(const char*);
void hostHistogramDrive();
void hostHistogramScene(bool);
'''
(out / 'HostRuntime.h').write_text(runtime)
for name in (
    'android-base/file.h', 'android-base/logging.h', 'android-base/unique_fd.h',
    'android/binder_interface_utils.h', 'android/binder_manager.h',
    'ui/GraphicTypes.h', 'aidl/vendor/lineage/livedisplay/BnDisplayModes.h',
    'aidl/vendor/xiaomi/rothko/display/color/BnRothkoDisplayColor.h',
    'aidl/vendor/xiaomi/rothko/display/brightness/BnRothkoDozeBrightness.h',
):
    f = out / name
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_text('#include "HostRuntime.h"\n')
(out / 'HistogramService.h').write_text('''#pragma once
#include "HostRuntime.h"
#include "StockDisplayFeature.h"
namespace rothko::color {
class HistogramService {
public:
    void newSf(int64_t,int64_t){}
    void scene(bool known,bool,int64_t){hostHistogramScene(known);}
    void sfGuard(std::shared_ptr<std::atomic<bool>>,int64_t,int64_t){}
    void backend(std::shared_ptr<std::atomic<bool>>){}
    void drive(aidl::vendor::xiaomi::hardware::displayfeature_aidl::IDisplayFeature&,int64_t){hostHistogramDrive();}
};
}
''')
binary = out / 'controller-host-test'
subprocess.run([args.compiler, '-std=c++20', '-pthread', '-O0', '-g',
                '-Wall', '-Wextra', '-Werror', '-fno-access-control',
                '-Dopen=hostOpen', '-Dopenat=hostOpenAt',
                '-I', str(out), str(out / 'Controller.cpp'),
                str(tests / 'ControllerRequestHostTest.cpp'), '-o', str(binary)], check=True)
(out / 'source.sha256').write_text(''.join(
    hashlib.sha256((source / n).read_bytes()).hexdigest() + '  ' + n + '\n'
    for n in ('Controller.cpp', 'Controller.h', 'StockDisplayFeature.h')))
result = subprocess.run([str(binary), args.scenario], capture_output=True, text=True, timeout=60)
(out / 'run.log').write_text(result.stdout + result.stderr)
(out / 'result.json').write_text(json.dumps({
    'scenario': args.scenario, 'exit_code': result.returncode,
    'passed': [line[5:] for line in result.stdout.splitlines() if line.startswith('PASS ')],
    'scope': 'production Controller with host boundary fakes; no Android build or phone',
}, indent=2) + '\n')
print(result.stdout, end='')
print(result.stderr, end='')
result.check_returncode()
