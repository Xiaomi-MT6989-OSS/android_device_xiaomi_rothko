# Color controller host regression tests

Pinned stock startup-order transformation (provide the unmodified firmware
executable, not an arbitrary newer version):

```sh
python3 displaycolor/tests/test_startup_fixup.py /path/to/stock/vendor.xiaomi.hardware.displayfeature_aidl-service
```

This checks ELF preservation, instruction relocation and modeled control flow;
it does not execute the ARM code or the real HAL. See
`../docs/STOCK_STARTUP_ORDER.md` for scope and device validation requirements.

Provider lock contention reproducer:

```sh
python3 displaycolor/tests/run_provider_lock_tests.py --output /tmp/rothko-provider-handoff
```

This compiles verbatim production update/enqueuePolicy/loseState method bodies
inside a minimal host fixture and uses the production StateUpdateQueue header.
It covers nonblocking state delivery despite worker lock ownership, ordered
power events, discarded events, full state queue, rejection of another producer
thread, 100000 concurrent updates across ring wrap, Binder policy queue contention
and real policy overflow. `--sanitize` adds ASan/UBSan. The former try-lock
implementation loses state under contention; a blocking-lock replacement also
fails the nonblocking check. The fixture does not model the complete provider,
Binder delivery, SF thread affinity, frame latency or a physical display.

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

The fourteen scenarios cover stale timeout after listener replacement, queued
caller completion on replacement, a request admitted during automatic restore,
stale completion while a newer request owns admission, delayed OEM return after
replacement, replacement reusing external connection identifiers, and persistence
failure reported separately from the accepted current mode, and a failed scene
notification retried before a mode write with sampling eligibility restored, and
a matching late intent after the former 400 ms window (including rejection of a
different intent and exactly one publication), a missing echo timing out without
changing current/default mode, and timely echo waiting for matching SF/HWC
software acceptance before completing the caller, prompt failure when policy
delivery fails, invalidating current mode when gamut-policy delivery fails, and
invalidating current on the caller timeout while OEM IPC remains blocked, without
letting its late return confirm or save the abandoned mode. The last case invokes
the real timeout handler at a controlled boundary rather than sleeping 12 seconds.

These tests execute the real worker/request code. The fake Binder transport does
not model Android IPC, death delivery or transaction queues. The fake OEM backend
normally injects intent/gamut through the actual registered callback, and the
fake policy listener supplies synthetic SF/HWC acceptance reports. Specific
scenarios disable each automatic response to exercise late/missing events and
pending acceptance. The missing-echo case uses the real nine-second deadline.
These cannot
prove LUT application, echo/HWC acknowledgement, panel correctness, successful
persistence/durability, HistogramService behaviour or UI lifecycle correctness.
The persistence case covers failure only. Real device regression remains required.
