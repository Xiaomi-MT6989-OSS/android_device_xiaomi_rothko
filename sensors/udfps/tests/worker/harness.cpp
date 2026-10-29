// SPDX-License-Identifier: Apache-2.0
// Host fixtures only. This program never opens device input or calls Android
// Binder.
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "FodInput.h"
#define ALOGE(...) ((void)0)
#define ALOGW(...) ((void)0)
namespace android {
int64_t elapsedRealtimeNano() {
  return std::chrono::steady_clock::now().time_since_epoch().count();
}
}  // namespace android
namespace harness {
using android::base::unique_fd;
template <class T>
using sp = std::shared_ptr<T>;
template <class T>
using Return = T;
enum class Result { OK, BAD_VALUE, INVALID_OPERATION };
enum class OperationMode { NORMAL };
enum class SensorType { DEVICE_PRIVATE_BASE = 65536 };
struct Event {
  int sensorHandle;
  SensorType sensorType;
  int64_t timestamp;
  struct {
    float data[16]{};
  } u;
};
struct IHalProxyCallback {
  std::atomic<int> delivered{0};
  int createScopedWakelock(bool) { return 1; }
  void postEvents(std::vector<Event> events, int) {
    for (const auto& e : events) {
      if (e.sensorHandle != 1 || e.u.data[0] != 603 || e.u.data[1] != 2344)
        throw std::runtime_error("payload mismatch");
      ++delivered;
    }
  }
};
struct ISensorsSubHal {
  virtual ~ISensorsSubHal() = default;
  virtual Return<Result> initialize(const sp<IHalProxyCallback>&) = 0;
  virtual Return<Result> activate(int32_t, bool) = 0;
};
unique_fd pipeRead, pipeWrite;
std::atomic<int> injectError{0}, injected{0}, offCalls{0}, onCalls{0},
    wrongModes{0}, disableFailures{0}, appliedMode{0}, timedPolls{0};
std::atomic<bool> persistentError{false};
std::mutex failureTimesMutex;
std::vector<std::chrono::steady_clock::time_point> failureTimes;
#include "production-constants.inc"
bool setTouchMode(int mode, int value) {
  if (mode != 16) ++wrongModes;
  if (value)
    ++onCalls;
  else {
    ++offCalls;
    int failures = disableFailures.load();
    if (failures > 0) {
      --disableFailures;
      return false;
    }
  }
  appliedMode = value;
  return true;
}
#include "production-mode.inc"
unique_fd findGoodixInput() { return unique_fd(dup(pipeRead.get())); }
int ioctl(int, unsigned long code, void* value) {
  std::memset(value, 0, _IOC_SIZE(code));
  return 0;
}
int injectedPoll(pollfd* fds, nfds_t n, int timeout) {
  if (timeout >= 0) ++timedPolls;
  if (n == 2 && (fds[1].fd >= 0 || persistentError.load())) {
    int error =
        persistentError.load() ? injectError.load() : injectError.exchange(0);
    if (error) {
      {
        std::lock_guard lock(failureTimesMutex);
        failureTimes.push_back(std::chrono::steady_clock::now());
      }
      errno = error;
      ++injected;
      return -1;
    }
  }
  return ::poll(fds, n, timeout);
}
#define poll injectedPoll
#include "production-class.inc"
#undef poll
int checks = 0, failures = 0;
void check(bool ok, const char* name) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cout << "FAIL " << name << '\n';
  }
}
template <class F>
bool until(F done, int milliseconds = 1800) {
  auto limit = std::chrono::steady_clock::now() +
               std::chrono::milliseconds(milliseconds);
  while (!done() && std::chrono::steady_clock::now() < limit)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  return done();
}
void setup() {
  int fds[2];
  if (pipe2(fds, O_NONBLOCK | O_CLOEXEC)) throw std::runtime_error("pipe");
  pipeRead.reset(fds[0]);
  pipeWrite.reset(fds[1]);
  injected = 0;
  offCalls = 0;
  onCalls = 0;
  wrongModes = 0;
  disableFailures = 0;
  appliedMode = 0;
  timedPolls = 0;
  persistentError = false;
  failureTimes.clear();
}
void press() {
  std::vector<input_event> events;
  for (auto triple :
       std::vector<std::array<int, 3>>{{EV_KEY, 0x152, 1},
                                       {EV_ABS, ABS_MT_SLOT, 0},
                                       {EV_ABS, ABS_MT_TRACKING_ID, 1},
                                       {EV_ABS, ABS_MT_POSITION_X, 60300},
                                       {EV_ABS, ABS_MT_POSITION_Y, 234400},
                                       {EV_SYN, SYN_REPORT, 0}}) {
    input_event event{};
    event.type = triple[0];
    event.code = triple[1];
    event.value = triple[2];
    events.push_back(event);
  }
  if (write(pipeWrite.get(), events.data(),
            events.size() * sizeof(input_event)) !=
      static_cast<ssize_t>(events.size() * sizeof(input_event)))
    throw std::runtime_error("write");
}
int run() {
  for (int error : {0, EINTR, ENOMEM}) {
    setup();
    injectError = error;
    auto callback = std::make_shared<IHalProxyCallback>();
    auto start = std::chrono::steady_clock::now();
    {
      UdfpsSubHal hal;
      check(hal.initialize(callback) == Result::OK, "initialize");
      check(hal.activate(1, true) == Result::OK, "activation accepted");
      if (error) {
        check(until([] { return injected == 1; }),
              "requested poll failure injected");
      }
      if (error == EINTR)
        check(offCalls == 0, "EINTR does not disarm a valid subscription");
      if (error == ENOMEM) {
        check(until([] { return offCalls >= 1; }),
              "fatal poll error cleans mode16 before worker recovery");
        check(hal.activate(1, true) == Result::OK,
              "new activation accepted after fatal poll");
      }
      press();
      check(until([&] { return callback->delivered == 1; }),
            "real worker resumes and delivers one fresh press");
      const int expected = error == ENOMEM ? 2 : 1;
      check(until([&] { return offCalls >= expected; }),
            "trigger closes mode16");
      check(callback->delivered == 1 && wrongModes == 0,
            "one-shot and exclusive mode16 ownership");
      check(timedPolls == 0, "normal input waiting has no timed poll");
    }
    check(std::chrono::steady_clock::now() - start < std::chrono::seconds(15),
          "bounded fixture teardown");
  }
  setup();
  injectError = ENOMEM;
  persistentError = true;
  {
    auto callback = std::make_shared<IHalProxyCallback>();
    UdfpsSubHal hal;
    hal.initialize(callback);
    hal.activate(1, true);
    check(until([] { return injected >= 2; }, 3000),
          "persistent poll failure reaches a bounded second retry");
    {
      std::lock_guard lock(failureTimesMutex);
      check(failureTimes.size() >= 2 &&
                failureTimes[1] - failureTimes[0] >=
                    std::chrono::milliseconds(500) &&
                callback->delivered == 0 && appliedMode == 0,
            "persistent failure does not spin or emit a touch");
    }
    injectError = 0;
    persistentError = false;
    check(hal.activate(1, true) == Result::OK, "rearm after persistent error");
    press();
    check(until([&] { return callback->delivered == 1; }),
          "persistent failure recovery delivers a new contact");
  }
  setup();
  injectError = ENOMEM;
  disableFailures = 100000;
  {
    auto callback = std::make_shared<IHalProxyCallback>();
    UdfpsSubHal hal;
    hal.initialize(callback);
    hal.activate(1, true);
    check(until([] { return offCalls >= 1; }),
          "poll-loss cleanup failure injected");
    check(appliedMode == 1, "failed disable is not treated as confirmed");
    check(hal.activate(1, true) == Result::OK,
          "new generation can rearm after unconfirmed old cleanup");
    const int beforeDisable = offCalls;
    disableFailures = 0;
    press();
    check(until([&] { return callback->delivered == 1; }),
          "old cleanup does not disable new armed generation");
    check(until([&] { return appliedMode == 0 && offCalls > beforeDisable; }),
          "fresh trigger confirms final mode16 disable");
  }
  setup();
  injectError = ENOMEM;
  {
    auto callback = std::make_shared<IHalProxyCallback>();
    auto hal = std::make_unique<UdfpsSubHal>();
    hal->initialize(callback);
    hal->activate(1, true);
    check(until([] { return offCalls == 1; }), "error cleanup before unload");
    auto start = std::chrono::steady_clock::now();
    hal.reset();
    check(std::chrono::steady_clock::now() - start < std::chrono::seconds(2),
          "unload can stop error recovery wait");
    check(callback->delivered == 0 && appliedMode == 0,
          "unload does not fabricate a press or retain acknowledged mode16");
  }
  std::cout
      << "RESULT " << checks - failures << " passed, " << failures
      << " failed; real worker/thread/poll with input and Binder fixtures\n";
  return failures ? 1 : 0;
}
}  // namespace harness
int main() { return harness::run(); }
