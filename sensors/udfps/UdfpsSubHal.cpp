/* Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0 */
#define LOG_TAG "sensors.rothko.udfps"
#include <android-base/unique_fd.h>
#include <dirent.h>
#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utils/SystemClock.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "FodInput.h"
#include "V2_1/SubHal.h"

namespace android::hardware::sensors::V2_1::subhal::rothko {
using ::android::base::unique_fd;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::RateLevel;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SharedMemInfo;
using ::android::hardware::sensors::V2_1::implementation::IHalProxyCallback;
using ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal;

// This device's stock Goodix input ABI was captured under real screen-off
// presses. Never guess an event number, rescale another panel, grab input, or
// write the device.
static unique_fd findGoodixInput() {
  std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir("/dev/input"), closedir);
  if (!dir) return {};
  while (const auto* entry = readdir(dir.get())) {
    if (strncmp(entry->d_name, "event", 5) != 0) continue;
    const std::string path = std::string("/dev/input/") + entry->d_name;
    unique_fd fd(open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC));
    char name[128]{};
    std::array<uint8_t, KEY_MAX / 8 + 1> keys{};
    input_absinfo x{}, y{}, slots{};
    if (fd.get() < 0 || ioctl(fd.get(), EVIOCGNAME(sizeof(name)), name) < 0 ||
        strcmp(name, "goodix_ts") != 0 ||
        ioctl(fd.get(), EVIOCGBIT(EV_KEY, sizeof(keys)), keys.data()) < 0 ||
        !(keys[::rothko::FodInput::kFodKey / 8] &
          (1 << (::rothko::FodInput::kFodKey % 8))) ||
        ioctl(fd.get(), EVIOCGABS(ABS_MT_POSITION_X), &x) < 0 ||
        ioctl(fd.get(), EVIOCGABS(ABS_MT_POSITION_Y), &y) < 0 ||
        ioctl(fd.get(), EVIOCGABS(ABS_MT_SLOT), &slots) < 0)
      continue;
    if (x.minimum == 0 && x.maximum == 121999 && y.minimum == 0 &&
        y.maximum == 271199 && slots.minimum == 0 && slots.maximum == 9)
      return fd;
  }
  return {};
}

class UdfpsSubHal final : public ISensorsSubHal {
 public:
  UdfpsSubHal() : mWake(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
    mAvailable = mWake.get() >= 0 && findGoodixInput().get() >= 0;
    if (!mAvailable)
      ALOGE("Stock Goodix FOD input unavailable; no sensor exposed");
    if (mWake.get() >= 0) mThread = std::thread([this] { run(); });
  }
  ~UdfpsSubHal() override {
    mStop = true;
    kick();
    if (mThread.joinable()) mThread.join();
  }

  Return<void> getSensorsList_2_1(ISensors::getSensorsList_2_1_cb cb) override {
    SensorInfo info{};
    info.sensorHandle = kHandle;
    info.name = "Rothko UDFPS touch";
    info.vendor = "The LineageOS Project";
    info.version = 1;
    info.type = static_cast<SensorType>(
        static_cast<int>(SensorType::DEVICE_PRIVATE_BASE) + 3);
    info.typeAsString = "org.lineageos.sensor.udfps";
    info.requiredPermission = "android.permission.MANAGE_BIOMETRIC";
    info.maxRange = 2712;
    info.resolution = 1;
    info.minDelay = -1;
    info.flags = static_cast<uint32_t>(SensorFlagBits::ONE_SHOT_MODE) |
                 static_cast<uint32_t>(SensorFlagBits::WAKE_UP);
    cb(mAvailable ? std::vector<SensorInfo>{info} : std::vector<SensorInfo>{});
    return Void();
  }
  Return<Result> initialize(const sp<IHalProxyCallback>& callback) override {
    std::lock_guard lock(mMutex);
    mInput.reset();
    ++mGeneration;
    mCallback = callback;
    mMode = OperationMode::NORMAL;
    kick();
    return Result::OK;
  }
  Return<Result> activate(int32_t handle, bool enabled) override {
    if (handle != kHandle || !mAvailable) return Result::BAD_VALUE;
    std::lock_guard lock(mMutex);
    // Idempotent activation must not reset an in-progress physical press.
    if (enabled == (mInput.get() >= 0)) return Result::OK;
    if (enabled && (mMode != OperationMode::NORMAL || !mCallback)) {
      return Result::INVALID_OPERATION;
    }
    if (enabled) {
      auto input = findGoodixInput();
      std::array<uint8_t, KEY_MAX / 8 + 1> keys{};
      if (input.get() < 0 ||
          ioctl(input.get(), EVIOCGKEY(sizeof(keys)), keys.data()) < 0) {
        ALOGE("Cannot arm stock Goodix input");
        return Result::INVALID_OPERATION;
      }
      mHeldAtActivation = keys[::rothko::FodInput::kFodKey / 8] &
                          (1 << (::rothko::FodInput::kFodKey % 8));
      mInput = std::move(input);
    } else {
      mInput.reset();
    }
    ++mGeneration;
    kick();
    return Result::OK;
  }
  Return<Result> setOperationMode(OperationMode mode) override {
    if (mode != OperationMode::NORMAL) return Result::INVALID_OPERATION;
    return Result::OK;
  }
  Return<Result> batch(int32_t handle, int64_t, int64_t) override {
    return handle == kHandle ? Result::OK : Result::BAD_VALUE;
  }
  Return<Result> flush(int32_t) override { return Result::BAD_VALUE; }
  Return<Result> injectSensorData_2_1(const Event&) override {
    return Result::INVALID_OPERATION;
  }
  Return<void> registerDirectChannel(
      const SharedMemInfo&, ISensors::registerDirectChannel_cb cb) override {
    cb(Result::INVALID_OPERATION, -1);
    return Void();
  }
  Return<Result> unregisterDirectChannel(int32_t) override {
    return Result::INVALID_OPERATION;
  }
  Return<void> configDirectReport(int32_t, int32_t, RateLevel,
                                  ISensors::configDirectReport_cb cb) override {
    cb(Result::INVALID_OPERATION, 0);
    return Void();
  }
  Return<void> debug(const hidl_handle& fd,
                     const hidl_vec<hidl_string>&) override {
    std::lock_guard lock(mMutex);
    const auto* handle = fd.getNativeHandle();
    if (handle && handle->numFds > 0) {
      dprintf(handle->data[0],
              "rothko FOD: available=%d armed=%d generation=%llu\n", mAvailable,
              mInput.get() >= 0, static_cast<unsigned long long>(mGeneration));
    }
    return Void();
  }
  const std::string getName() override { return "RothkoUdfpsInput"; }

 private:
  void kick() {
    const uint64_t value = 1;
    if (mWake.get() >= 0 && write(mWake.get(), &value, sizeof(value)) < 0 &&
        errno != EAGAIN) {
      ALOGE("Failed to interrupt FOD wait: %s", strerror(errno));
    }
  }
  void run() {
    while (!mStop) {
      unique_fd input;
      uint64_t generation;
      bool held;
      {
        std::lock_guard lock(mMutex);
        if (mInput.get() >= 0) input.reset(dup(mInput.get()));
        generation = mGeneration;
        held = mHeldAtActivation;
      }
      ::rothko::FodInput parser(held);
      while (!mStop) {
        pollfd fds[] = {{mWake.get(), POLLIN, 0}, {input.get(), POLLIN, 0}};
        const int rc = poll(fds, 2, -1);
        if (rc < 0) {
          if (errno == EINTR) continue;
          return;
        }
        if (fds[0].revents) {
          uint64_t value;
          (void)read(mWake.get(), &value, sizeof(value));
          break;
        }
        if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
          std::lock_guard lock(mMutex);
          if (generation == mGeneration) {
            mInput.reset();
            ++mGeneration;
          }
          ALOGE("FOD input lost; waiting for a new activation");
          break;
        }
        if (!(fds[1].revents & POLLIN)) continue;
        input_event events[32];
        const auto size = read(input.get(), events, sizeof(events));
        if (size < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        if (size <= 0 || size % sizeof(input_event)) {
          std::lock_guard lock(mMutex);
          if (generation == mGeneration) {
            mInput.reset();
            ++mGeneration;
          }
          break;
        }
        bool triggered = false;
        for (size_t i = 0; i < static_cast<size_t>(size) / sizeof(input_event);
             ++i) {
          const auto touch = parser.feed(events[i]);
          if (!touch) continue;
          // Serialize cancellation/reinitialization against this one-shot
          // delivery.
          std::lock_guard lock(mMutex);
          if (generation == mGeneration && mInput.get() >= 0 && mCallback) {
            Event event{};
            event.sensorHandle = kHandle;
            event.sensorType = static_cast<SensorType>(
                static_cast<int>(SensorType::DEVICE_PRIVATE_BASE) + 3);
            event.timestamp = ::android::elapsedRealtimeNano();
            event.u.data[0] = touch->x;
            event.u.data[1] = touch->y;
            event.u.data[3] = touch->major;
            event.u.data[4] = touch->minor;
            mInput.reset();
            ++mGeneration;
            auto wakelock = mCallback->createScopedWakelock(true);
            mCallback->postEvents({event}, std::move(wakelock));
          }
          triggered = true;
          break;
        }
        if (triggered) break;
      }
    }
  }
  static constexpr int32_t kHandle = 1;
  unique_fd mWake, mInput;
  bool mAvailable = false, mHeldAtActivation = false;
  uint64_t mGeneration = 0;
  std::atomic_bool mStop{false};
  OperationMode mMode = OperationMode::NORMAL;
  std::mutex mMutex;
  sp<IHalProxyCallback> mCallback;
  std::thread mThread;
};
}  // namespace android::hardware::sensors::V2_1::subhal::rothko

::android::hardware::sensors::V2_1::implementation::ISensorsSubHal*
sensorsHalGetSubHal_2_1(uint32_t* version) {
  static ::android::hardware::sensors::V2_1::subhal::rothko::UdfpsSubHal
      instance;
  *version = SUB_HAL_2_1_VERSION;
  return &instance;
}
