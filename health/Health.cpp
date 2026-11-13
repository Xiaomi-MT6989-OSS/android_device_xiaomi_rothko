/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <charconv>
#include <cstdint>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/binder_interface_utils.h>
#include <health-impl/Health.h>
#include <health/utils.h>

#ifndef CHARGER_FORCE_NO_UI
#define CHARGER_FORCE_NO_UI 0
#endif

#if !CHARGER_FORCE_NO_UI
#include <health-impl/ChargerUtils.h>
#endif

using aidl::android::hardware::health::BatteryStatus;
using aidl::android::hardware::health::HalHealthLoop;
using aidl::android::hardware::health::Health;
using aidl::android::hardware::health::HealthInfo;

#if !CHARGER_FORCE_NO_UI
using aidl::android::hardware::health::charger::ChargerCallback;
using aidl::android::hardware::health::charger::ChargerModeMain;
#endif

static constexpr const char* gInstanceName = "default";
static constexpr std::string_view gChargerArg{"--charger"};

namespace aidl::android::hardware::health {

namespace {

constexpr char kUsbRealType[] = "/sys/class/power_supply/usb/real_type";
constexpr char kPdSourcePdo[] = "/sys/devices/platform/pd-adapter/Charging_Adapter/pd_adapter/pdo";
constexpr int kMaxSourcePdos = 7;

bool IsUsbPdConnected() {
    std::string type;
    return ::android::base::ReadFileToString(kUsbRealType, &type) &&
           ::android::base::Trim(type) == "USB_PD";
}

void UpdateUsbPdChargingCapability(HealthInfo& info) {
    if (!(info.chargerAcOnline || info.chargerUsbOnline) ||
        info.batteryStatus != BatteryStatus::CHARGING || !IsUsbPdConnected()) {
        return;
    }

    int bestMillivolts = 0;
    int bestMilliamps = 0;
    for (int index = 1; index <= kMaxSourcePdos; ++index) {
        std::string data;
        if (!::android::base::ReadFileToString(kPdSourcePdo + std::to_string(index), &data)) {
            break;
        }
        data = ::android::base::Trim(data);
        uint32_t pdo = 0;
        const auto [end, error] = std::from_chars(data.data(), data.data() + data.size(), pdo, 16);
        if (error != std::errc() || end != data.data() + data.size() || pdo == 0) {
            break;
        }

        // USB PD Source PDOs: fixed supply or programmable power supply (PPS).
        // Keep voltage and current from the same PDO.
        const uint32_t type = pdo >> 30;
        int millivolts;
        int milliamps;
        if (type == 0) {
            millivolts = ((pdo >> 10) & 0x3ff) * 50;
            milliamps = (pdo & 0x3ff) * 10;
        } else if (type == 3 && ((pdo >> 28) & 0x3) == 0) {
            millivolts = ((pdo >> 17) & 0xff) * 100;
            milliamps = (pdo & 0x7f) * 50;
        } else {
            continue;
        }
        if (millivolts * milliamps > bestMillivolts * bestMilliamps) {
            bestMillivolts = millivolts;
            bestMilliamps = milliamps;
        }
    }

    // The primary_chg supply can retain its BC1.2 limits after PD has connected.
    // Replace those limits only with a better capability from the current source.
    const int64_t reportedPower = static_cast<int64_t>(info.maxChargingVoltageMicrovolts) *
                                  info.maxChargingCurrentMicroamps;
    const int64_t pdPower = static_cast<int64_t>(bestMillivolts) * bestMilliamps * 1000000;
    if (pdPower > 0 && pdPower > reportedPower && IsUsbPdConnected()) {
        info.maxChargingVoltageMicrovolts = bestMillivolts * 1000;
        info.maxChargingCurrentMicroamps = bestMilliamps * 1000;
    }
}

}  // namespace

class RothkoHealth : public Health {
  public:
    using Health::Health;

  protected:
    void UpdateHealthInfo(HealthInfo* health_info) override {
        UpdateUsbPdChargingCapability(*health_info);

        if (health_info->batteryLevel >= 100 ||
            health_info->batteryStatus == BatteryStatus::FULL) {
            health_info->batteryChargeTimeToFullNowSeconds = 0;
            return;
        }

        // mt6375-battery.ko computes TIME_TO_FULL_NOW from
        // GAUGE_PROP_AVERAGE_CURRENT. The rothko gauge reports an invalid
        // average (~6 mA) and an instantaneous value that alternates around
        // zero, so neither source can provide a trustworthy estimate here.
        // Report unsupported and let BatteryStats learn the rate from observed
        // charge-level changes instead of publishing a made-up number.
        health_info->batteryChargeTimeToFullNowSeconds =
                HealthInfo::BATTERY_CHARGE_TIME_TO_FULL_NOW_SECONDS_UNSUPPORTED;
    }
};

#if !CHARGER_FORCE_NO_UI
class ChargerCallbackImpl : public ChargerCallback {
  public:
    using ChargerCallback::ChargerCallback;
    bool ChargerEnableSuspend() override { return true; }
};
#endif

}  // namespace aidl::android::hardware::health

int main(int argc, char** argv) {
#ifdef __ANDROID_RECOVERY__
    android::base::InitLogging(argv, android::base::KernelLogger);
#endif

    auto config = std::make_unique<healthd_config>();
    ::android::hardware::health::InitHealthdConfig(config.get());
    auto binder = ndk::SharedRefBase::make<aidl::android::hardware::health::RothkoHealth>(
            gInstanceName, std::move(config));

    if (argc >= 2 && argv[1] == gChargerArg) {
#if !CHARGER_FORCE_NO_UI
        return ChargerModeMain(
                binder,
                std::make_shared<aidl::android::hardware::health::ChargerCallbackImpl>(binder));
#endif

        LOG(INFO) << "Starting charger mode without UI.";
    } else {
        LOG(INFO) << "Starting health HAL.";
    }

    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, binder);
    return hal_health_loop->StartLoop();
}
