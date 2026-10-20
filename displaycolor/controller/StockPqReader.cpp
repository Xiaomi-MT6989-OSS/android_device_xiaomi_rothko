// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#include "StockPqReader.h"

namespace rothko::color {

bool StockPqReader::compatible(pq::IPictureQuality_AIDL& service) const {
    int32_t version = -1;
    std::string hash;
    if (!service.getInterfaceVersion(&version).isOk() ||
        !service.getInterfaceHash(&hash).isOk()) return false;
    // The packaged client is V4; stock DisplayFeature itself uses this client
    // against the packaged V7 service. Selected request/result wire fields have
    // been checked in both binaries. Do not accept arbitrary later versions.
    return (version == 4 && hash == "1f8b1085be183c24269a6aacac46bb666446e3ea") ||
           (version == 7 && hash == "2e981138bf239a3959d7b4acf90c61416856fc26");
}

PqReadback StockPqReader::read(pq::IPictureQuality_AIDL& service) const {
    PqReadback sample;
    // New, value-initialized result each time. The stock server can leave out
    // fields untouched on errors and ignores some kernel ioctl return values.
    pq::parcelable_getDispContentSamples result{};
    const std::vector<int32_t> channels{0};
    sample.readStartedAt = mClock();
    const auto status = service.getDispContentSamples(1, channels, &result);
    sample.transportOk = status.isOk();
    if (!sample.transportOk) return sample;
    sample.result = result.result;
    sample.frame = result.frame;
    if (result.channels.size() != 1) return sample;
    const auto& channel = result.channels.front();
    sample.channel = channel.channel;
    sample.format = channel.format;
    sample.binCount = channel.binCount;
    sample.bins = channel.bins;
    // Binder success/result==0 alone are not a measurement. Do not compute a
    // scalar or invent a replacement frame here: the consumer must retain the
    // full tuple and apply its own session, counter and elapsed-time checks.
    if (sample.result != 0 || sample.frame == 0 || sample.channel != 0 ||
        sample.format != 9 || sample.binCount != 256) return sample;
    bool populated = false;
    for (int32_t bin : sample.bins) {
        if (bin < 0) return sample;
        populated |= bin > 0;
    }
    sample.shapeValid = populated;
    return sample;
}
} // namespace rothko::color
