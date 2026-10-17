// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "RothkoDisplayColor"
#include "controller/Controller.h"
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

    addService("vendor.lineage.livedisplay.IDisplayModes/default", modes);
    addService("vendor.xiaomi.rothko.display.color.IRothkoDisplayColor/default", controller);

    ABinderProcess_joinThreadPool();
    return 0;
}
