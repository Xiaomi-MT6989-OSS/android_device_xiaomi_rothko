// Copyright (C) 2026 The LineageOS Project
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
class DYNAMIC_METADATA_T;
class DispPQIoctlParams;
class DispPQSwitchModules;
class DitherParam;
class Drm_mtk_channel_config;
class IAiBldCallback;
class IDataCallback;
enum class PQFeatureID : int32_t;
enum class Result : int32_t;
class STATIC_METADATA_T;
class parcelable_execIoctl;
class parcelable_getAIBldParams;
class parcelable_getAshmem;
class parcelable_getBlueLightEnabled;
class parcelable_getBlueLightStrength;
class parcelable_getChameleonEnabled;
class parcelable_getChameleonStrength;
class parcelable_getColorIndex;
class parcelable_getColorRegion;
class parcelable_getDMABufferFd;
class parcelable_getDispC3DMode;
class parcelable_getDispContentSamples;
class parcelable_getDispDitherParam;
class parcelable_getDispGammaTable;
class parcelable_getDispSampleAttibutes;
class parcelable_getDispTdshpMode;
class parcelable_getDisplayGamePQFeatures;
class parcelable_getESSLEDMinStep;
class parcelable_getESSOLEDMinStep;
class parcelable_getExternalPanelNits;
class parcelable_getFeatureSwitch;
class parcelable_getGammaIndex;
class parcelable_getGlobalPQIndex;
class parcelable_getGlobalPQStableStatus;
class parcelable_getGlobalPQStrength;
class parcelable_getGlobalPQStrengthRange;
class parcelable_getGlobalPQSwitch;
class parcelable_getMappedColorIndex;
class parcelable_getMappedTDSHPIndex;
class parcelable_getSilkyBrightnessEnabled;
class parcelable_getSilkyBrightnessMode;
class parcelable_getTDSHPFlag;
class parcelable_getTDSHPIndex;
class parcelable_getTuningField;
class parcelable_isHDRVideo;
class parcelable_perform;
class parcelable_readField;
class parcelable_setAIBldBufferWithFence;
class parcelable_setColorModeWithFence;

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
    // vtable +0x30
    virtual ndk::ScopedAStatus getAIBldParams(parcelable_getAIBldParams*) = 0;
    // vtable +0x38
    virtual ndk::ScopedAStatus getAshmem(parcelable_getAshmem*) = 0;
    // vtable +0x40
    virtual ndk::ScopedAStatus getBlueLightEnabled(parcelable_getBlueLightEnabled*) = 0;
    // vtable +0x48
    virtual ndk::ScopedAStatus getBlueLightStrength(parcelable_getBlueLightStrength*) = 0;
    // vtable +0x50
    virtual ndk::ScopedAStatus getChameleonEnabled(parcelable_getChameleonEnabled*) = 0;
    // vtable +0x58
    virtual ndk::ScopedAStatus getChameleonStrength(parcelable_getChameleonStrength*) = 0;
    // vtable +0x60
    virtual ndk::ScopedAStatus getColorRegion(parcelable_getColorRegion*) = 0;
    // vtable +0x68
    virtual ndk::ScopedAStatus getGammaIndex(parcelable_getGammaIndex*) = 0;
    // vtable +0x70
    virtual ndk::ScopedAStatus getGlobalPQIndex(parcelable_getGlobalPQIndex*) = 0;
    // vtable +0x78
    virtual ndk::ScopedAStatus getGlobalPQStableStatus(parcelable_getGlobalPQStableStatus*) = 0;
    // vtable +0x80
    virtual ndk::ScopedAStatus getGlobalPQStrength(parcelable_getGlobalPQStrength*) = 0;
    // vtable +0x88
    virtual ndk::ScopedAStatus getGlobalPQStrengthRange(parcelable_getGlobalPQStrengthRange*) = 0;
    // vtable +0x90
    virtual ndk::ScopedAStatus getGlobalPQSwitch(parcelable_getGlobalPQSwitch*) = 0;
    // vtable +0x98
    virtual ndk::ScopedAStatus getMappedColorIndex(int, int, parcelable_getMappedColorIndex*) = 0;
    // vtable +0xa0
    virtual ndk::ScopedAStatus getSilkyBrightnessEnabled(parcelable_getSilkyBrightnessEnabled*) = 0;
    // vtable +0xa8
    virtual ndk::ScopedAStatus getSilkyBrightnessMode(parcelable_getSilkyBrightnessMode*) = 0;
    // vtable +0xb0
    virtual ndk::ScopedAStatus registerAIBldCb(std::shared_ptr<IAiBldCallback> const&) = 0;
    // vtable +0xb8
    virtual ndk::ScopedAStatus setAIBldBuffer(aidl::android::hardware::common::NativeHandle const&, int, Result*) = 0;
    // vtable +0xc0
    virtual ndk::ScopedAStatus setAIBldBufferWithFence(aidl::android::hardware::common::NativeHandle const&, ndk::ScopedFileDescriptor const&, int, parcelable_setAIBldBufferWithFence*) = 0;
    // vtable +0xc8
    virtual ndk::ScopedAStatus setAIBldEnable(bool, int, Result*) = 0;
    // vtable +0xd0
    virtual ndk::ScopedAStatus setAmbientLightCT(double, double, double, Result*) = 0;
    // vtable +0xd8
    virtual ndk::ScopedAStatus setAmbientLightRGBW(int, int, int, int, Result*) = 0;
    // vtable +0xe0
    virtual ndk::ScopedAStatus setBlueLightStrength(int, int, Result*) = 0;
    // vtable +0xe8
    virtual ndk::ScopedAStatus setChameleonStrength(int, int, Result*) = 0;
    // vtable +0xf0
    virtual ndk::ScopedAStatus setDISPScenario(int, int, Result*) = 0;
    // vtable +0xf8
    virtual ndk::ScopedAStatus setDispC3DMode(signed char, int, int, Result*) = 0;
    // vtable +0x100
    virtual ndk::ScopedAStatus setDispDitherParam(signed char, DitherParam const&, Result*) = 0;
    // vtable +0x108
    virtual ndk::ScopedAStatus setDispFetureType(std::basic_string<char, std::char_traits<char>, std::allocator<char>> const&, int, int, int, int, Result*) = 0;
    // vtable +0x110
    virtual ndk::ScopedAStatus setDispGammaTable(signed char, std::vector<int, std::allocator<int>> const&, std::vector<int, std::allocator<int>> const&, std::vector<int, std::allocator<int>> const&, int, signed char, Result*) = 0;
    // vtable +0x118
    virtual ndk::ScopedAStatus setDispPQBufHandle(signed char, ndk::ScopedFileDescriptor const&, int, ndk::ScopedFileDescriptor const&, int, Result*) = 0;
    // vtable +0x120
    virtual ndk::ScopedAStatus setDispSampleAttibutes(signed char, int, std::vector<Drm_mtk_channel_config, std::allocator<Drm_mtk_channel_config>> const&, Result*) = 0;
    // vtable +0x128
    virtual ndk::ScopedAStatus setDispTdshpMode(signed char, int, Result*) = 0;
    // vtable +0x130
    virtual ndk::ScopedAStatus setDisplayGamePQ(int, int, int, Result*) = 0;
    // vtable +0x138
    virtual ndk::ScopedAStatus setESSLEDMinStep(int, Result*) = 0;
    // vtable +0x140
    virtual ndk::ScopedAStatus setESSOLEDMinStep(int, Result*) = 0;
    // vtable +0x148
    virtual ndk::ScopedAStatus setLowBLReadabilityLevel(int, Result*) = 0;
    // vtable +0x150
    virtual ndk::ScopedAStatus setScreenState(int, int, Result*) = 0;
    // vtable +0x158
    virtual ndk::ScopedAStatus setSilkyBrightnessMode(int, Result*) = 0;
    // vtable +0x160
    virtual ndk::ScopedAStatus setSmartBacklightRange(int, Result*) = 0;
    // vtable +0x168
    virtual ndk::ScopedAStatus setSmartBacklightStrength(int, Result*) = 0;
    // vtable +0x170
    virtual ndk::ScopedAStatus getDispC3DMode(signed char, parcelable_getDispC3DMode*) = 0;
    // vtable +0x178
    virtual ndk::ScopedAStatus getDispContentSamples(signed char, std::vector<int, std::allocator<int>> const&, parcelable_getDispContentSamples*) = 0;
    // vtable +0x180
    virtual ndk::ScopedAStatus getDispDitherParam(signed char, parcelable_getDispDitherParam*) = 0;
    // vtable +0x188
    virtual ndk::ScopedAStatus getDispGammaTable(signed char, signed char, parcelable_getDispGammaTable*) = 0;
    // vtable +0x190
    virtual ndk::ScopedAStatus getDispSampleAttibutes(signed char, parcelable_getDispSampleAttibutes*) = 0;
    // vtable +0x198
    virtual ndk::ScopedAStatus getDispTdshpMode(signed char, parcelable_getDispTdshpMode*) = 0;
    // vtable +0x1a0
    virtual ndk::ScopedAStatus getDisplayGamePQFeatures(parcelable_getDisplayGamePQFeatures*) = 0;
    // vtable +0x1a8
    virtual ndk::ScopedAStatus getESSLEDMinStep(parcelable_getESSLEDMinStep*) = 0;
    // vtable +0x1b0
    virtual ndk::ScopedAStatus getESSOLEDMinStep(parcelable_getESSOLEDMinStep*) = 0;
    // vtable +0x1b8
    virtual ndk::ScopedAStatus setCcorrInfo(std::array<std::array<int, 3ul>, 3ul> const&, bool, Result*) = 0;
    // vtable +0x1c0
    virtual ndk::ScopedAStatus setColorMatrix3x3(std::array<std::array<int, 3ul>, 3ul> const&, int, Result*) = 0;
    // vtable +0x1c8
    virtual ndk::ScopedAStatus setColorModeWithFence(int, ndk::ScopedFileDescriptor const&, int, int, parcelable_setColorModeWithFence*) = 0;
    // vtable +0x1d0
    virtual ndk::ScopedAStatus setColorRegion(int, int, int, int, int, Result*) = 0;
    // vtable +0x1d8
    virtual ndk::ScopedAStatus setColorTransform(std::array<std::array<float, 4ul>, 4ul> const&, int, int, Result*) = 0;
    // vtable +0x1e0
    virtual ndk::ScopedAStatus setFunction(int, bool, Result*) = 0;
    // vtable +0x1e8
    virtual ndk::ScopedAStatus setGamePQHandle(ndk::ScopedFileDescriptor const&, Result*) = 0;
    // vtable +0x1f0
    virtual ndk::ScopedAStatus setGammaIndex(int, int, Result*) = 0;
    // vtable +0x1f8
    virtual ndk::ScopedAStatus setGlobalPQStableStatus(int, Result*) = 0;
    // vtable +0x200
    virtual ndk::ScopedAStatus setGlobalPQStrength(int, Result*) = 0;
    // vtable +0x208
    virtual ndk::ScopedAStatus setGlobalPQSwitch(int, Result*) = 0;
    // vtable +0x210
    virtual ndk::ScopedAStatus setHueOffset(int, int, Result*) = 0;
    // vtable +0x218
    virtual ndk::ScopedAStatus setRGBGain(int, int, int, int, Result*) = 0;
    // vtable +0x220
    virtual ndk::ScopedAStatus setReadabilityLevel(int, Result*) = 0;
    // vtable +0x228
    virtual ndk::ScopedAStatus writeField(int, int, Result*) = 0;
    // vtable +0x230
    virtual ndk::ScopedAStatus readField(int, parcelable_readField*) = 0;
    // vtable +0x238
    virtual ndk::ScopedAStatus custInvoke(int, long, Result*) = 0;
    // vtable +0x240
    virtual ndk::ScopedAStatus enableBlueLight(bool, int, Result*) = 0;
    // vtable +0x248
    virtual ndk::ScopedAStatus enableChameleon(bool, int, Result*) = 0;
    // vtable +0x250
    virtual ndk::ScopedAStatus enableSilkyBrightness(bool, Result*) = 0;
    // vtable +0x258
    virtual ndk::ScopedAStatus getDMABufferFd(parcelable_getDMABufferFd*) = 0;
    // vtable +0x260
    virtual ndk::ScopedAStatus getExternalPanelNits(parcelable_getExternalPanelNits*) = 0;
    // vtable +0x268
    virtual ndk::ScopedAStatus hintSecure(bool) = 0;
    // vtable +0x270
    virtual ndk::ScopedAStatus isHDRVideo(STATIC_METADATA_T const&, DYNAMIC_METADATA_T const&, parcelable_isHDRVideo*) = 0;
    // vtable +0x278
    virtual ndk::ScopedAStatus perform(long, int, std::optional<std::vector<unsigned char, std::allocator<unsigned char>>> const&, std::optional<std::vector<ndk::ScopedFileDescriptor, std::allocator<ndk::ScopedFileDescriptor>>> const&, parcelable_perform*) = 0;
    // vtable +0x280
    virtual ndk::ScopedAStatus setAIModel(ndk::ScopedFileDescriptor const&, std::basic_string<char, std::char_traits<char>, std::allocator<char>> const&, Result*) = 0;
    // vtable +0x288
    virtual ndk::ScopedAStatus setExternalPanelNits(int, Result*) = 0;
    // vtable +0x290
    virtual ndk::ScopedAStatus setFeatureSwitch(PQFeatureID, int, Result*) = 0;
    // vtable +0x298
    virtual ndk::ScopedAStatus setPQFramework(ndk::ScopedFileDescriptor const&, long, std::vector<int, std::allocator<int>> const&, Result*) = 0;
    // vtable +0x2a0
    virtual ndk::ScopedAStatus setPQFrameworkWithAppHint(ndk::ScopedFileDescriptor const&, long, std::vector<int, std::allocator<int>> const&, int, Result*) = 0;
    // vtable +0x2a8
    virtual ndk::ScopedAStatus getMappedTDSHPIndex(int, int, parcelable_getMappedTDSHPIndex*) = 0;
    // vtable +0x2b0
    virtual ndk::ScopedAStatus getColorIndex(int, int, parcelable_getColorIndex*) = 0;
    // vtable +0x2b8
    virtual ndk::ScopedAStatus getFeatureSwitch(PQFeatureID, parcelable_getFeatureSwitch*) = 0;
    // vtable +0x2c0
    virtual ndk::ScopedAStatus getTDSHPFlag(parcelable_getTDSHPFlag*) = 0;
    // vtable +0x2c8
    virtual ndk::ScopedAStatus setTDSHPFlag(int, Result*) = 0;
    // vtable +0x2d0
    virtual ndk::ScopedAStatus getTDSHPIndex(int, int, parcelable_getTDSHPIndex*) = 0;
    // vtable +0x2d8
    virtual ndk::ScopedAStatus execIoctl(DispPQIoctlParams const&, parcelable_execIoctl*) = 0;
    // vtable +0x2e0
    virtual ndk::ScopedAStatus setPQIndex(int, int, int, int, int, Result*) = 0;
    // vtable +0x2e8
    virtual ndk::ScopedAStatus setPQMode(int, int, Result*) = 0;
    // vtable +0x2f0
    virtual ndk::ScopedAStatus setTuningField(int, int, int, Result*) = 0;
    // vtable +0x2f8
    virtual ndk::ScopedAStatus getTuningField(int, int, parcelable_getTuningField*) = 0;
    // vtable +0x300
    virtual ndk::ScopedAStatus triggerALIReport(Result*) = 0;
    // vtable +0x308
    virtual ndk::ScopedAStatus getAIBldParamsMPq(int, int, parcelable_getAIBldParams*) = 0;
    // vtable +0x310
    virtual ndk::ScopedAStatus getAshmemMPq(int, int, parcelable_getAshmem*) = 0;
    // vtable +0x318
    virtual ndk::ScopedAStatus getBlueLightEnabledMPq(int, int, parcelable_getBlueLightEnabled*) = 0;
    // vtable +0x320
    virtual ndk::ScopedAStatus getBlueLightStrengthMPq(int, int, parcelable_getBlueLightStrength*) = 0;
    // vtable +0x328
    virtual ndk::ScopedAStatus getChameleonEnabledMPq(int, int, parcelable_getChameleonEnabled*) = 0;
    // vtable +0x330
    virtual ndk::ScopedAStatus getChameleonStrengthMPq(int, int, parcelable_getChameleonStrength*) = 0;
    // vtable +0x338
    virtual ndk::ScopedAStatus getColorRegionMPq(int, int, parcelable_getColorRegion*) = 0;
    // vtable +0x340
    virtual ndk::ScopedAStatus getGammaIndexMPq(int, int, parcelable_getGammaIndex*) = 0;
    // vtable +0x348
    virtual ndk::ScopedAStatus getGlobalPQIndexMPq(int, int, parcelable_getGlobalPQIndex*) = 0;
    // vtable +0x350
    virtual ndk::ScopedAStatus getGlobalPQStableStatusMPq(int, int, parcelable_getGlobalPQStableStatus*) = 0;
    // vtable +0x358
    virtual ndk::ScopedAStatus getGlobalPQStrengthMPq(int, int, parcelable_getGlobalPQStrength*) = 0;
    // vtable +0x360
    virtual ndk::ScopedAStatus getGlobalPQStrengthRangeMPq(int, int, parcelable_getGlobalPQStrengthRange*) = 0;
    // vtable +0x368
    virtual ndk::ScopedAStatus getGlobalPQSwitchMPq(int, int, parcelable_getGlobalPQSwitch*) = 0;
    // vtable +0x370
    virtual ndk::ScopedAStatus getMappedColorIndexMPq(int, int, int, int, parcelable_getMappedColorIndex*) = 0;
    // vtable +0x378
    virtual ndk::ScopedAStatus getSilkyBrightnessEnabledMPq(int, int, parcelable_getSilkyBrightnessEnabled*) = 0;
    // vtable +0x380
    virtual ndk::ScopedAStatus getSilkyBrightnessModeMPq(int, int, parcelable_getSilkyBrightnessMode*) = 0;
    // vtable +0x388
    virtual ndk::ScopedAStatus registerAIBldCbMPq(int, int, std::shared_ptr<IAiBldCallback> const&) = 0;
    // vtable +0x390
    virtual ndk::ScopedAStatus setAIBldBufferMPq(int, int, aidl::android::hardware::common::NativeHandle const&, int, Result*) = 0;
    // vtable +0x398
    virtual ndk::ScopedAStatus setAIBldBufferWithFenceMPq(int, int, aidl::android::hardware::common::NativeHandle const&, ndk::ScopedFileDescriptor const&, int, parcelable_setAIBldBufferWithFence*) = 0;
    // vtable +0x3a0
    virtual ndk::ScopedAStatus setAIBldEnableMPq(int, int, bool, int, Result*) = 0;
    // vtable +0x3a8
    virtual ndk::ScopedAStatus setAmbientLightCTMPq(int, int, double, double, double, Result*) = 0;
    // vtable +0x3b0
    virtual ndk::ScopedAStatus setAmbientLightRGBWMPq(int, int, int, int, int, int, Result*) = 0;
    // vtable +0x3b8
    virtual ndk::ScopedAStatus setBlueLightStrengthMPq(int, int, int, int, Result*) = 0;
    // vtable +0x3c0
    virtual ndk::ScopedAStatus setChameleonStrengthMPq(int, int, int, int, Result*) = 0;
    // vtable +0x3c8
    virtual ndk::ScopedAStatus setDISPScenarioMPq(int, int, int, int, Result*) = 0;
    // vtable +0x3d0
    virtual ndk::ScopedAStatus setDispC3DModeMPq(int, int, int, int, Result*) = 0;
    // vtable +0x3d8
    virtual ndk::ScopedAStatus setDispDitherParamMPq(int, int, DitherParam const&, Result*) = 0;
    // vtable +0x3e0
    virtual ndk::ScopedAStatus setDispFeatureTypeMPq(int, int, int, std::basic_string<char, std::char_traits<char>, std::allocator<char>> const&, int, int, int, Result*) = 0;
    // vtable +0x3e8
    virtual ndk::ScopedAStatus setDispGammaTableMPq(int, int, std::vector<int, std::allocator<int>> const&, std::vector<int, std::allocator<int>> const&, std::vector<int, std::allocator<int>> const&, int, signed char, Result*) = 0;
    // vtable +0x3f0
    virtual ndk::ScopedAStatus setDispPQBufHandleMPq(int, int, ndk::ScopedFileDescriptor const&, int, ndk::ScopedFileDescriptor const&, int, Result*) = 0;
    // vtable +0x3f8
    virtual ndk::ScopedAStatus setDispSampleAttibutesMPq(int, int, int, std::vector<Drm_mtk_channel_config, std::allocator<Drm_mtk_channel_config>> const&, Result*) = 0;
    // vtable +0x400
    virtual ndk::ScopedAStatus setDispTdshpModeMPq(int, int, int, Result*) = 0;
    // vtable +0x408
    virtual ndk::ScopedAStatus setDisplayGamePQMPq(int, int, int, int, int, Result*) = 0;
    // vtable +0x410
    virtual ndk::ScopedAStatus setESSLEDMinStepMPq(int, int, int, Result*) = 0;
    // vtable +0x418
    virtual ndk::ScopedAStatus setESSOLEDMinStepMPq(int, int, int, Result*) = 0;
    // vtable +0x420
    virtual ndk::ScopedAStatus setLowBLReadabilityLevelMPq(int, int, int, Result*) = 0;
    // vtable +0x428
    virtual ndk::ScopedAStatus setScreenStateMPq(int, int, int, int, Result*) = 0;
    // vtable +0x430
    virtual ndk::ScopedAStatus setSilkyBrightnessModeMPq(int, int, int, Result*) = 0;
    // vtable +0x438
    virtual ndk::ScopedAStatus setSmartBacklightRangeMPq(int, int, int, Result*) = 0;
    // vtable +0x440
    virtual ndk::ScopedAStatus setSmartBacklightStrengthMPq(int, int, int, Result*) = 0;
    // vtable +0x448
    virtual ndk::ScopedAStatus getDispC3DModeMPq(int, int, parcelable_getDispC3DMode*) = 0;
    // vtable +0x450
    virtual ndk::ScopedAStatus getDispContentSamplesMPq(int, int, std::vector<int, std::allocator<int>> const&, parcelable_getDispContentSamples*) = 0;
    // vtable +0x458
    virtual ndk::ScopedAStatus getDispDitherParamMPq(int, int, parcelable_getDispDitherParam*) = 0;
    // vtable +0x460
    virtual ndk::ScopedAStatus getDispGammaTableMPq(int, int, signed char, parcelable_getDispGammaTable*) = 0;
    // vtable +0x468
    virtual ndk::ScopedAStatus getDispSampleAttibutesMPq(int, int, parcelable_getDispSampleAttibutes*) = 0;
    // vtable +0x470
    virtual ndk::ScopedAStatus getDispTdshpModeMPq(int, int, parcelable_getDispTdshpMode*) = 0;
    // vtable +0x478
    virtual ndk::ScopedAStatus getDisplayGamePQFeaturesMPq(int, int, parcelable_getDisplayGamePQFeatures*) = 0;
    // vtable +0x480
    virtual ndk::ScopedAStatus getESSLEDMinStepMPq(int, int, parcelable_getESSLEDMinStep*) = 0;
    // vtable +0x488
    virtual ndk::ScopedAStatus getESSOLEDMinStepMPq(int, int, parcelable_getESSOLEDMinStep*) = 0;
    // vtable +0x490
    virtual ndk::ScopedAStatus setCcorrInfoMPq(int, int, std::array<std::array<int, 3ul>, 3ul> const&, bool, Result*) = 0;
    // vtable +0x498
    virtual ndk::ScopedAStatus setColorMatrix3x3MPq(int, int, std::array<std::array<int, 3ul>, 3ul> const&, int, Result*) = 0;
    // vtable +0x4a0
    virtual ndk::ScopedAStatus setColorModeWithFenceMPq(int, int, int, int, ndk::ScopedFileDescriptor const&, parcelable_setColorModeWithFence*) = 0;
    // vtable +0x4a8
    virtual ndk::ScopedAStatus setColorRegionMPq(int, int, int, int, int, int, int, Result*) = 0;
    // vtable +0x4b0
    virtual ndk::ScopedAStatus setColorTransformMPq(int, int, std::array<std::array<float, 4ul>, 4ul> const&, int, int, Result*) = 0;
    // vtable +0x4b8
    virtual ndk::ScopedAStatus setFunctionMPq(int, int, int, bool, Result*) = 0;
    // vtable +0x4c0
    virtual ndk::ScopedAStatus setGamePQHandleMPq(int, int, ndk::ScopedFileDescriptor const&, Result*) = 0;
    // vtable +0x4c8
    virtual ndk::ScopedAStatus setGammaIndexMPq(int, int, int, int, Result*) = 0;
    // vtable +0x4d0
    virtual ndk::ScopedAStatus setGlobalPQStableStatusMPq(int, int, int, Result*) = 0;
    // vtable +0x4d8
    virtual ndk::ScopedAStatus setGlobalPQStrengthMPq(int, int, int, Result*) = 0;
    // vtable +0x4e0
    virtual ndk::ScopedAStatus setGlobalPQSwitchMPq(int, int, int, Result*) = 0;
    // vtable +0x4e8
    virtual ndk::ScopedAStatus setHueOffsetMPq(int, int, int, int, Result*) = 0;
    // vtable +0x4f0
    virtual ndk::ScopedAStatus setRGBGainMPq(int, int, int, int, int, int, Result*) = 0;
    // vtable +0x4f8
    virtual ndk::ScopedAStatus setReadabilityLevelMPq(int, int, int, Result*) = 0;
    // vtable +0x500
    virtual ndk::ScopedAStatus writeFieldMPq(int, int, int, int, Result*) = 0;
    // vtable +0x508
    virtual ndk::ScopedAStatus readFieldMPq(int, int, int, parcelable_readField*) = 0;
    // vtable +0x510
    virtual ndk::ScopedAStatus custInvokeMPq(int, int, int, long, Result*) = 0;
    // vtable +0x518
    virtual ndk::ScopedAStatus enableBlueLightMPq(int, int, bool, int, Result*) = 0;
    // vtable +0x520
    virtual ndk::ScopedAStatus enableChameleonMPq(int, int, bool, int, Result*) = 0;
    // vtable +0x528
    virtual ndk::ScopedAStatus enableSilkyBrightnessMPq(int, int, bool, Result*) = 0;
    // vtable +0x530
    virtual ndk::ScopedAStatus getDMABufferFdMPq(int, int, parcelable_getDMABufferFd*) = 0;
    // vtable +0x538
    virtual ndk::ScopedAStatus getExternalPanelNitsMPq(int, int, parcelable_getExternalPanelNits*) = 0;
    // vtable +0x540
    virtual ndk::ScopedAStatus hintSecureMPq(int, int, bool) = 0;
    // vtable +0x548
    virtual ndk::ScopedAStatus isHDRVideoMPq(int, int, STATIC_METADATA_T const&, DYNAMIC_METADATA_T const&, parcelable_isHDRVideo*) = 0;
    // vtable +0x550
    virtual ndk::ScopedAStatus performMPq(int, int, long, int, std::optional<std::vector<unsigned char, std::allocator<unsigned char>>> const&, std::optional<std::vector<ndk::ScopedFileDescriptor, std::allocator<ndk::ScopedFileDescriptor>>> const&, parcelable_perform*) = 0;
    // vtable +0x558
    virtual ndk::ScopedAStatus setAIModelMPq(int, int, ndk::ScopedFileDescriptor const&, std::basic_string<char, std::char_traits<char>, std::allocator<char>> const&, Result*) = 0;
    // vtable +0x560
    virtual ndk::ScopedAStatus setExternalPanelNitsMPq(int, int, int, Result*) = 0;
    // vtable +0x568
    virtual ndk::ScopedAStatus setFeatureSwitchMPq(int, int, PQFeatureID, int, Result*) = 0;
    // vtable +0x570
    virtual ndk::ScopedAStatus setPQFrameworkMPq(int, int, ndk::ScopedFileDescriptor const&, long, std::vector<int, std::allocator<int>> const&, Result*) = 0;
    // vtable +0x578
    virtual ndk::ScopedAStatus setPQFrameworkWithAppHintMPq(int, int, ndk::ScopedFileDescriptor const&, long, std::vector<int, std::allocator<int>> const&, int, Result*) = 0;
    // vtable +0x580
    virtual ndk::ScopedAStatus getMappedTDSHPIndexMPq(int, int, int, int, parcelable_getMappedTDSHPIndex*) = 0;
    // vtable +0x588
    virtual ndk::ScopedAStatus getColorIndexMPq(int, int, int, int, parcelable_getColorIndex*) = 0;
    // vtable +0x590
    virtual ndk::ScopedAStatus getFeatureSwitchMPq(int, int, PQFeatureID, parcelable_getFeatureSwitch*) = 0;
    // vtable +0x598
    virtual ndk::ScopedAStatus getTDSHPFlagMPq(int, int, parcelable_getTDSHPFlag*) = 0;
    // vtable +0x5a0
    virtual ndk::ScopedAStatus setTDSHPFlagMPq(int, int, int, Result*) = 0;
    // vtable +0x5a8
    virtual ndk::ScopedAStatus getTDSHPIndexMPq(int, int, int, int, parcelable_getTDSHPIndex*) = 0;
    // vtable +0x5b0
    virtual ndk::ScopedAStatus execIoctlMPq(int, int, DispPQIoctlParams const&, parcelable_execIoctl*) = 0;
    // vtable +0x5b8
    virtual ndk::ScopedAStatus setPQIndexMPq(int, int, int, int, int, int, int, Result*) = 0;
    // vtable +0x5c0
    virtual ndk::ScopedAStatus setPQModeMPq(int, int, int, int, Result*) = 0;
    // vtable +0x5c8
    virtual ndk::ScopedAStatus setTuningFieldMPq(int, int, int, int, int, Result*) = 0;
    // vtable +0x5d0
    virtual ndk::ScopedAStatus getTuningFieldMPq(int, int, int, int, parcelable_getTuningField*) = 0;
    // vtable +0x5d8
    virtual ndk::ScopedAStatus triggerALIReportMPq(int, int, Result*) = 0;
    // vtable +0x5e0
    virtual ndk::ScopedAStatus registerDataCbMPq(int, int, int, std::shared_ptr<IDataCallback> const&) = 0;
    // vtable +0x5e8
    virtual ndk::ScopedAStatus unregisterDataCbMPq(int, int, int, Result*) = 0;
    // vtable +0x5f0
    virtual ndk::ScopedAStatus setDispFeatureDynIndexMPq(int, int, int, std::basic_string<char, std::char_traits<char>, std::allocator<char>> const&, int, Result*) = 0;
    // vtable +0x5f8
    virtual ndk::ScopedAStatus setDispPQBufHandleV2MPq(int, int, bool, ndk::ScopedFileDescriptor const&, int, ndk::ScopedFileDescriptor const&, int, int, Result*) = 0;
    // vtable +0x600
    virtual ndk::ScopedAStatus setDisplayGamePQV2MPq(int, int, int, int, int, int, int, Result*) = 0;
    // vtable +0x608
    virtual ndk::ScopedAStatus setDispPQSwitchMPq(int, int, int, int, int, DispPQSwitchModules const&, bool, std::shared_ptr<IDataCallback> const&, Result*) = 0;
    // vtable +0x610
    virtual ndk::ScopedAStatus getInterfaceVersion(int*) = 0;
    // vtable +0x618
    virtual ndk::ScopedAStatus getInterfaceHash(std::basic_string<char, std::char_traits<char>, std::allocator<char>>*) = 0;
};
} // namespace aidl::vendor::mediatek::hardware::pq_aidl
