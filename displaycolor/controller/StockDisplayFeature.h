// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <android/binder_interface_utils.h>
#include <cstdint>
#include <memory>
#include <string>

// ABI declarations only. All constructors, proxy/stub code and frozen metadata
// are supplied by the byte-identical stock V2 NDK library. Do not generate a
// replacement OEM binding or copy the diagnostic notfrozen implementation.
namespace aidl::vendor::xiaomi::hardware::displayfeature_aidl {
class IDisplayFeatureCallback : public ndk::ICInterface {
public:
    static const char* descriptor;
    IDisplayFeatureCallback();
    virtual ~IDisplayFeatureCallback();
    static inline const int32_t version = 2;
    static const std::string hash;
    static std::shared_ptr<IDisplayFeatureCallback> fromBinder(const ndk::SpAIBinder&);
    static binder_status_t writeToParcel(AParcel*, const std::shared_ptr<IDisplayFeatureCallback>&);
    static binder_status_t readFromParcel(const AParcel*, std::shared_ptr<IDisplayFeatureCallback>*);
    static bool setDefaultImpl(const std::shared_ptr<IDisplayFeatureCallback>&);
    static const std::shared_ptr<IDisplayFeatureCallback>& getDefaultImpl();
    virtual ndk::ScopedAStatus displayfeatureInfoChanged(int32_t, int32_t, float, float, float) = 0;
    virtual ndk::ScopedAStatus getInterfaceVersion(int32_t*) = 0;
    virtual ndk::ScopedAStatus getInterfaceHash(std::string*) = 0;
private:
    static std::shared_ptr<IDisplayFeatureCallback> default_impl;
};
class BnDisplayFeatureCallback : public ndk::BnCInterface<IDisplayFeatureCallback> {
public:
    BnDisplayFeatureCallback();
    virtual ~BnDisplayFeatureCallback();
    ndk::ScopedAStatus getInterfaceVersion(int32_t*) final;
    ndk::ScopedAStatus getInterfaceHash(std::string*) final;
protected:
    ndk::SpAIBinder createBinder() override;
};
class IDisplayFeature : public ndk::ICInterface {
public:
    static const char* descriptor;
    IDisplayFeature();
    virtual ~IDisplayFeature();
    static inline const int32_t version = 2;
    static const std::string hash;
    static std::shared_ptr<IDisplayFeature> fromBinder(const ndk::SpAIBinder&);
    static binder_status_t writeToParcel(AParcel*, const std::shared_ptr<IDisplayFeature>&);
    static binder_status_t readFromParcel(const AParcel*, std::shared_ptr<IDisplayFeature>*);
    static bool setDefaultImpl(const std::shared_ptr<IDisplayFeature>&);
    static const std::shared_ptr<IDisplayFeature>& getDefaultImpl();
    virtual ndk::ScopedAStatus notifyBrightness(int32_t) = 0;
    virtual ndk::ScopedAStatus registerCallback(int32_t, const std::shared_ptr<IDisplayFeatureCallback>&) = 0;
    virtual ndk::ScopedAStatus sendMessage(int32_t, int32_t, const std::string&) = 0;
    virtual ndk::ScopedAStatus sendPanelCommand(const std::string&) = 0;
    virtual ndk::ScopedAStatus sendPostProcCommand(int32_t, int32_t) = 0;
    virtual ndk::ScopedAStatus sendRefreshCommand() = 0;
    virtual ndk::ScopedAStatus setFeature(int32_t, int32_t, int32_t, int32_t) = 0;
    virtual ndk::ScopedAStatus setFunction(int32_t, int32_t, int32_t, int32_t) = 0;
    virtual ndk::ScopedAStatus sendGamePkgName(int32_t, int32_t, int32_t, const std::string&) = 0;
    virtual ndk::ScopedAStatus getInterfaceVersion(int32_t*) = 0;
    virtual ndk::ScopedAStatus getInterfaceHash(std::string*) = 0;
private:
    static std::shared_ptr<IDisplayFeature> default_impl;
};
} // namespace aidl::vendor::xiaomi::hardware::displayfeature_aidl
