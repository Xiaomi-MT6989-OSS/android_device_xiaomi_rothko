/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
#include "TouchFeature.h"

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

namespace rothko::touch {
namespace {
constexpr char kService[] = "vendor.xiaomi.hw.touchfeature.ITouchFeature/default";
constexpr char kDescriptor[] = "vendor.xiaomi.hw.touchfeature.ITouchFeature";
// Stock V1 BpTouchFeature::setModeValue(int, int, int, int*).
constexpr transaction_code_t kSetModeValue = 9;
constexpr int kPrimaryTouch = 0;
constexpr int kDoubleTapMode = 14;

void* onCreate(void*) { return nullptr; }
void onDestroy(void*) {}
binder_status_t onTransact(AIBinder*, transaction_code_t, const AParcel*, AParcel*) {
    return STATUS_UNKNOWN_TRANSACTION;
}

AIBinder_Class* touchFeatureClass() {
    static AIBinder_Class* clazz = AIBinder_Class_define(
            kDescriptor, onCreate, onDestroy, onTransact);
    return clazz;
}
}  // namespace

bool setDoubleTapToWake(bool enabled) {
    // Re-resolve instead of retaining a stale binder across a HAL restart. Do
    // not block the power HAL waiting indefinitely for an unavailable service.
    AIBinder* binder = AServiceManager_checkService(kService);
    AIBinder_Class* clazz = touchFeatureClass();
    if (!binder || !clazz) {
        if (binder) AIBinder_decStrong(binder);
        return false;
    }
    // Remote binders require the stock interface class for the parcel token.
    if (!AIBinder_associateClass(binder, clazz)) {
        AIBinder_decStrong(binder);
        return false;
    }

    AParcel* input = nullptr;
    AParcel* output = nullptr;
    binder_status_t status = AIBinder_prepareTransaction(binder, &input);
    if (status == STATUS_OK) status = AParcel_writeInt32(input, kPrimaryTouch);
    if (status == STATUS_OK) status = AParcel_writeInt32(input, kDoubleTapMode);
    if (status == STATUS_OK) status = AParcel_writeInt32(input, enabled ? 1 : 0);
    if (status != STATUS_OK) {
        if (input) AParcel_delete(input);
        AIBinder_decStrong(binder);
        return false;
    }
    status = AIBinder_transact(binder, kSetModeValue, &input, &output, 0);
    AStatus* replyStatus = nullptr;
    int32_t result = -1;
    if (status == STATUS_OK) status = AParcel_readStatusHeader(output, &replyStatus);
    if (status == STATUS_OK && replyStatus && AStatus_isOk(replyStatus)) {
        status = AParcel_readInt32(output, &result);
    }
    const bool ok = status == STATUS_OK && replyStatus && AStatus_isOk(replyStatus)
            && result == 0;
    if (replyStatus) AStatus_delete(replyStatus);
    if (output) AParcel_delete(output);
    AIBinder_decStrong(binder);
    return ok;
}
}  // namespace rothko::touch
