// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "RothkoDisplayColor"
#include "controller/Controller.h"
#include "controller/HistogramService.h"
#include <android-base/logging.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <string>

using namespace rothko::color;

namespace {
void addService(const char* name, const std::shared_ptr<ndk::ICInterface>& service) {
    const std::string instance(name);
    const binder_status_t status =
            AServiceManager_addService(service->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        // A missing service would leave Settings with no colour profile source.
        LOG(FATAL) << "Failed to register " << instance << ": " << static_cast<int>(status);
    }
}
} // namespace

int main() {
    // The OEM service pushes its notifications into this process, and the
    // SurfaceFlinger provider calls setListener/reportState, so a pool is
    // required before either interface is published.
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    auto controller = ndk::SharedRefBase::make<Controller>();
    auto modes = ndk::SharedRefBase::make<DisplayModes>(controller);
    auto doze = ndk::SharedRefBase::make<DozeBrightness>(controller);

    addService("vendor.lineage.livedisplay.IDisplayModes/default", modes);
    addService("vendor.xiaomi.rothko.display.color.IRothkoDisplayColor/default", controller);
    const binder_status_t dozeStatus = AServiceManager_addService(doze->asBinder().get(),
            "vendor.xiaomi.rothko.display.brightness.IRothkoDozeBrightness/default");
    if (dozeStatus != STATUS_OK) {
        LOG(ERROR) << "Doze backlight transport registration failed: " << dozeStatus;
    }

    // Optional brightness transport failure must not restart a working colour
    // controller. The consumer fails closed and retries if it is unavailable.
    const binder_status_t brightnessStatus = AServiceManager_addService(
            controller->histogram()->asBinder().get(),
            "vendor.xiaomi.rothko.display.brightness.IRothkoBrightness/default");
    if (brightnessStatus != STATUS_OK) {
        LOG(ERROR) << "Brightness transport registration failed: " << brightnessStatus;
    }

    ABinderProcess_joinThreadPool();
    return 0;
}
