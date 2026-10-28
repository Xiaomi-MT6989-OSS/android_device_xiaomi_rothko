# Color controller host regression tests

Run from the device repository with Python 3.9+ and a C++20-capable g++:

```sh
python3 displaycolor/tests/run_controller_host_tests.py --output /tmp/rothko-color-host-test
```

The output directory must not exist and must be outside the device repository.
It receives source copies, their SHA-256 hashes, the executable, run.log and
result.json. The runner compiles byte-identical production Controller.cpp,
Controller.h and StockDisplayFeature.h against small host boundary fakes.
`-fno-access-control` is confined to this host executable: it permits controlled
scheduling and invoking the actual caller timeout handler without a 12-second
wall-clock delay. Android build files and production class visibility are unchanged.
Host open/openat always fail, preventing access to production /data paths.

The nine scenarios cover stale timeout after listener replacement, queued
caller completion on replacement, a request admitted during automatic restore,
stale completion while a newer request owns admission, delayed OEM return after
replacement, replacement reusing external connection identifiers, and persistence
failure reported separately from the accepted current mode, and a failed scene
notification retried before a mode write with sampling eligibility restored, and
the echo observation window starting after a delayed synchronous OEM return.

These tests execute the real worker/request code. The fake Binder transport does
not model Android IPC, death delivery or transaction queues. The fake OEM backend
returns success without echo, exercising the existing 400 ms fallback; it cannot
prove LUT application, echo/HWC acknowledgement, panel correctness, successful
persistence/durability, HistogramService behaviour or UI lifecycle correctness.
The persistence case covers failure only. Real device regression remains required.
