# UDFPS worker host regression

Run on Linux with Python 3, a C++17 host compiler and a LineageOS/AOSP source tree:

```sh
python3 device/xiaomi/rothko/sensors/udfps/tests/worker/run.py \
    --source-root . --output /tmp/rothko-fod-worker-test-01
```

The output directory must be new. The script writes generated includes, source
digests, compile/test logs and the host executable there. It does not run lunch,
Soong, adb, a device executable or an Android service.

Selected constructor/destructor, initialize/activate, worker and cleanup method
bodies and the mode constant are extracted unchanged from UdfpsSubHal.cpp.
FodInput.h and libbase unique_fd.h are real source headers. Threads, eventfd,
pipes, descriptor duplication and non-injected poll calls use Linux directly.
HIDL types/callbacks, Goodix discovery/key ioctl and touchfeature transactions
are fixtures. Input frames are generated locally, not captured from a phone.

Checks cover normal and EINTR handling, a fatal poll error, persistent errors,
reactivation and one-shot delivery, failed mode16 cleanup across a new generation,
absence of timed polling during normal waiting, and unload during recovery.
Test wait limits bound the harness; they are not new device timeouts or promises
about unlock latency. The persistent-failure timing check only detects busy-spin.

This is not a complete HIDL ABI, driver, Binder-blocking, hardware-illumination or
Android shutdown test. A touchfeature call that never returns cannot be interrupted
by the recovery condition variable. A disable whose reply fails remains unconfirmed;
the fixture permits transport recovery before checking successful final disable.
