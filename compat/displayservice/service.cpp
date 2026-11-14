// SPDX-License-Identifier: Apache-2.0
#include <android-base/logging.h>
#include <displayservice/DisplayService.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Errors.h>

int main() {
    android::hardware::configureRpcThreadpool(2, true);
    auto service = android::sp<android::frameworks::displayservice::V1_0::implementation::DisplayService>::make();
    if (service->registerAsService() != android::OK) {
        LOG(ERROR) << "Cannot register rothko display event compatibility service";
        return 1;
    }
    android::hardware::joinRpcThreadpool();
    return 1;
}
