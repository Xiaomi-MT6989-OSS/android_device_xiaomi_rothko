// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#include "Controller.h"
#include "StockDisplayFeature.h"
#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace rothko::color;
namespace df = aidl::vendor::xiaomi::hardware::displayfeature_aidl;
using ndk::ScopedAStatus;

// Production path names never access host /data; persistence is unavailable.
extern "C" int hostOpen(const char*,int,...){return -1;}
extern "C" int hostOpenAt(int,const char*,int,...){return -1;}
void hostOemFeature(int32_t,int32_t);
std::atomic<bool> failSceneOnce{false}, sceneNeedsRetry{false}, modeBeforeSceneRetry{false};
std::atomic<int> sceneAttempts{0};
std::atomic<bool> histogramSceneKnown{false};
void hostHistogramScene(bool known){histogramSceneKnown.store(known);}
// Boundary fakes only; Controller.cpp and its request/worker logic are unchanged.
namespace aidl::vendor::xiaomi::hardware::displayfeature_aidl {
const char* IDisplayFeature::descriptor="host.oem";
const std::string IDisplayFeature::hash="e57e364757bc7b63a3a7e586585c5827115966d7";
const char* IDisplayFeatureCallback::descriptor="host.callback";
const std::string IDisplayFeatureCallback::hash=IDisplayFeature::hash;
IDisplayFeature::IDisplayFeature()=default;
IDisplayFeature::~IDisplayFeature()=default;
IDisplayFeatureCallback::IDisplayFeatureCallback()=default;
IDisplayFeatureCallback::~IDisplayFeatureCallback()=default;
BnDisplayFeatureCallback::BnDisplayFeatureCallback()=default;
BnDisplayFeatureCallback::~BnDisplayFeatureCallback()=default;
ndk::SpAIBinder BnDisplayFeatureCallback::createBinder(){return asBinder();}
ScopedAStatus BnDisplayFeatureCallback::getInterfaceVersion(int32_t* v){*v=2;return ScopedAStatus::ok();}
ScopedAStatus BnDisplayFeatureCallback::getInterfaceHash(std::string* v){*v=hash;return ScopedAStatus::ok();}
std::shared_ptr<IDisplayFeature> IDisplayFeature::fromBinder(const ndk::SpAIBinder& b){
    return std::shared_ptr<IDisplayFeature>(static_cast<IDisplayFeature*>(b.get()->object),[](auto*){});
}
}
struct Oem final:df::IDisplayFeature {
    ScopedAStatus notifyBrightness(int32_t)override{return ScopedAStatus::ok();}
    ScopedAStatus registerCallback(int32_t,const std::shared_ptr<df::IDisplayFeatureCallback>&)override{return ScopedAStatus::ok();}
    ScopedAStatus sendMessage(int32_t,int32_t,const std::string&)override{return ScopedAStatus::ok();}
    ScopedAStatus sendPanelCommand(const std::string&)override{return ScopedAStatus::ok();}
    ScopedAStatus sendPostProcCommand(int32_t,int32_t)override{return ScopedAStatus::ok();}
    ScopedAStatus sendRefreshCommand()override{return ScopedAStatus::ok();}
    ScopedAStatus setFeature(int32_t,int32_t feature,int32_t,int32_t cookie)override{
        hostOemFeature(feature,cookie);
        if(feature==10&&cookie==0){
            ++sceneAttempts;
            if(failSceneOnce.exchange(false)){
                sceneNeedsRetry.store(true);
                return ScopedAStatus::fromServiceSpecificError(1);
            }
            sceneNeedsRetry.store(false);
        }
        if(cookie==255&&feature>=0&&feature<=2&&sceneNeedsRetry.load())
            modeBeforeSceneRetry.store(true);
        return ScopedAStatus::ok();
    }
    ScopedAStatus setFunction(int32_t,int32_t,int32_t,int32_t)override{return ScopedAStatus::ok();}
    ScopedAStatus sendGamePkgName(int32_t,int32_t,int32_t,const std::string&)override{return ScopedAStatus::ok();}
    ScopedAStatus getInterfaceVersion(int32_t* v)override{*v=2;return ScopedAStatus::ok();}
    ScopedAStatus getInterfaceHash(std::string* v)override{*v=hash;return ScopedAStatus::ok();}
};
Oem* oem=nullptr;
AIBinder* AServiceManager_checkService(const char*){return oem?oem->asBinder().get():nullptr;}

struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool armed=false,entered=false,released=false;
    void arm(){std::lock_guard l(mutex);armed=true;entered=false;released=false;}
    void pass(){std::unique_lock l(mutex);if(!armed)return;armed=false;entered=true;cv.notify_all();cv.wait(l,[&]{return released;});}
    void wait(){std::unique_lock l(mutex);if(!cv.wait_for(l,2s,[&]{return entered;}))throw std::runtime_error("worker gate not reached");}
    void release(){std::lock_guard l(mutex);released=true;cv.notify_all();}
} gate, oemGate;
void hostHistogramDrive(){gate.pass();}
void hostOemFeature(int32_t feature,int32_t cookie){if(feature==0&&cookie==255)oemGate.pass();}
struct Listener:bridge::IColorPolicyCallback {
    std::mutex mutex;
    int clears=0;
    ScopedAStatus onPolicy(int64_t,int64_t,int32_t intent,bool)override{
        std::lock_guard l(mutex);if(intent==-1)++clears;return ScopedAStatus::ok();
    }
    int count(){std::lock_guard l(mutex);return clears;}
};
template<class F> void until(F f,const char* message){
    auto deadline=std::chrono::steady_clock::now()+2s;
    while(!f()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error(message);std::this_thread::sleep_for(1ms);}
}
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
bridge::DisplayState scene(int64_t session,int64_t conn,int64_t seq,int output=1){
    bridge::DisplayState s;
    s.sfSession=session;s.connection=conn;s.sequence=seq;s.physicalId=4627039422300187648LL;
    s.connected=s.synchronizedState=s.contentKnown=s.enabled=s.managed=s.powerKnown=s.accepted=true;
    s.lastAttempted=2;s.outputCandidate=s.sdrCandidate=output;return s;
}
struct Fixture {
    std::shared_ptr<Oem> backend=std::make_shared<Oem>();
    std::shared_ptr<Controller> c;
    std::shared_ptr<Listener> listener=std::make_shared<Listener>();
    Fixture(){oem=backend.get();c=std::make_shared<Controller>();c->setListener(10,1,listener);c->reportState(scene(10,1,1));until([&]{return c->mReady.load();},"initial restore failed");}
    ~Fixture(){gate.release();oemGate.release();c.reset();oem=nullptr;}
    std::shared_ptr<Controller::Request> queued(){
        std::shared_ptr<Controller::Request> r;
        until([&]{std::lock_guard l(c->mMutex);r=c->mRequest;return r!=nullptr;},"request not admitted");return r;
    }
};

void staleTimeout(){
    Fixture f;
    gate.arm();f.c->reportState(scene(10,1,2,2));gate.wait();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    auto request=f.queued();
    auto replacement=std::make_shared<Listener>();
    f.c->setListener(11,1,replacement);f.c->reportState(scene(11,1,1));
    gate.release();
    until([&]{return f.c->mReady.load();},"replacement did not restore");
    int before=replacement->count();
    f.c->abandonFromCaller(request,1); // Real timeout handler, explicit scheduler boundary.
    int after=replacement->count();
    f.c->finish(request,false,2); // Teardown of the blocked public caller on old code.
    check(!call.get().isOk(),"old request unexpectedly succeeded");
    check(before==after,"stale timeout cleared replacement policy");
}
void reconnectCompletesQueued(){
    Fixture f;
    gate.arm();f.c->reportState(scene(10,1,2,2));gate.wait();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    auto request=f.queued();
    f.c->setListener(11,1,std::make_shared<Listener>());
    bool completed=call.wait_for(100ms)==std::future_status::ready;
    if(!completed)f.c->finish(request,false,1);
    gate.release();
    check(!call.get().isOk(),"replaced request succeeded");
    check(completed,"listener replacement silently dropped waiting request");
}
void requestDuringRestore(){
    Fixture f;
    gate.arm();f.c->reportState(scene(10,1,2,2));gate.wait();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    auto request=f.queued();gate.release();
    bool completed=call.wait_for(2s)==std::future_status::ready;
    if(!completed)f.c->finish(request,false,1);
    bool ok=call.get().isOk();
    check(completed&&ok,"request admitted during automatic restore was lost");
    check(f.c->currentMode()==1,"requested mode not retained");
}
void oldCompletionCannotReleaseNew(){
    Fixture f;
    gate.arm();f.c->reportState(scene(10,1,2,2));gate.wait();
    auto first=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    auto old=f.queued();
    f.c->setListener(11,1,std::make_shared<Listener>());f.c->reportState(scene(11,1,1));
    check(!first.get().isOk(),"old queued request not cancelled");gate.release();
    until([&]{return f.c->mReady.load();},"new restore unavailable");
    gate.arm();f.c->reportState(scene(11,1,2,2));gate.wait();
    auto second=std::async(std::launch::async,[&]{return f.c->setMode(2,false);});
    auto current=f.queued();
    f.c->finish(old,true,0);
    bool owns;
    {std::lock_guard l(f.c->mMutex);owns=f.c->mActiveRequest==current;}
    bool extra=f.c->setMode(1,false).isOk();
    gate.release();
    check(second.get().isOk(),"new request did not complete");
    check(owns&&!extra&&!old->ok,"stale completion released new admission or rewrote old failure");
}
void lateOemReturn(){
    Fixture f;
    oemGate.arm();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    oemGate.wait();
    f.c->setListener(11,1,std::make_shared<Listener>());f.c->reportState(scene(11,1,1));
    bool completed=call.wait_for(100ms)==std::future_status::ready;
    oemGate.release();
    check(!call.get().isOk()&&completed,"blocked old OEM request kept its caller waiting");
    until([&]{return f.c->mReady.load();},"new generation did not restore after old OEM return");
    check(f.c->currentMode()==3,"old OEM return published old current mode");
}
void repeatedIdentityCancelsQueued(){
    Fixture f;
    gate.arm();f.c->reportState(scene(10,1,2,2));gate.wait();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    auto request=f.queued();
    f.c->setListener(10,1,std::make_shared<Listener>());f.c->reportState(scene(10,1,1));
    bool completed=call.wait_for(100ms)==std::future_status::ready;
    gate.release();
    check(!call.get().isOk()&&completed,"reused public identity retained old request");
    until([&]{return f.c->mReady.load();},"same-identity registration failed to restore");
    check(f.c->currentMode()==3,"same-identity registration published stale user mode");
    f.c->abandonFromCaller(request,1);
    check(f.c->mReady.load(),"same-identity old timeout affected new generation");
}
void persistenceFailureKeepsActualSeparate(){
    Fixture f;
    check(!f.c->setMode(1,true).isOk(),"unavailable host persistence falsely succeeded");
    check(f.c->currentMode()==1&&f.c->defaultMode()==3,"persistence failure hid accepted mode or changed default");
}
void failedSceneMustBeRetried(){
    Fixture f;
    sceneAttempts.store(0);sceneNeedsRetry.store(false);modeBeforeSceneRetry.store(false);
    failSceneOnce.store(true);
    f.c->reportState(scene(10,1,2,2));
    until([&]{return !failSceneOnce.load();},"scene failure not injected");
    until([&]{return f.c->mReady.load();},"scene failure did not recover");
    check(sceneAttempts.load()>=2&&!modeBeforeSceneRetry.load(),
          "failed scene batch was cached as delivered; mode applied before retry");
    check(histogramSceneKnown.load(),"scene recovery left histogram sampling disabled");
}
void echoWindowStartsAfterReturn(){
    Fixture f;
    oemGate.arm();
    auto call=std::async(std::launch::async,[&]{return f.c->setMode(1,false);});
    oemGate.wait();
    std::this_thread::sleep_for(450ms); // Longer than the production echo window.
    oemGate.release();
    const bool early=call.wait_for(100ms)==std::future_status::ready;
    const bool ok=call.get().isOk();
    check(ok&&!early,"OEM IPC consumed the post-return echo observation window");
}
int main(int argc,char** argv){
    std::string which=argc>1?argv[1]:"all";
    int failed=0,ran=0;
    for(auto [name,test]:std::vector<std::pair<const char*,void(*)()>>{
            {"stale-timeout",staleTimeout},{"reconnect-queued",reconnectCompletesQueued},
            {"restore-queue",requestDuringRestore},
            {"stale-completion",oldCompletionCannotReleaseNew},{"late-oem",lateOemReturn},
            {"same-identity",repeatedIdentityCancelsQueued},{"persistence-failure",persistenceFailureKeepsActualSeparate},
            {"failed-scene-retry",failedSceneMustBeRetried},
            {"post-return-echo-window",echoWindowStartsAfterReturn}}){
        if(which!="all"&&which!=name)continue;
        ++ran;
        try{test();std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){++failed;gate.release();oemGate.release();std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
    }
    return failed||!ran?1:0;
}
