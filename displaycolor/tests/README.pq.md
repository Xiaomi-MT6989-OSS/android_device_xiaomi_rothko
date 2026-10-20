# Stock PQ readback preparation

`StockPictureQuality.h` contains declarations for the **packaged V4** client.
Constructors, destructors, `fromBinder`, metadata RPCs, transaction handling and
parcel readers/writers remain the byte-identical OEM shared library. We do not
construct a replacement proxy, make an opaque-object/vtable cast, or handwrite a
transaction. The header retains all 190 method declarations because removing
unused methods changes the virtual slot of the getter.

`stock_pq_header.py` reconstructs/checks their signatures and order from ELF
relocations and exported symbols of the pinned stock file, not nm output order.
The selected getter is method 41, vtable byte offset `0x178` from the object's
address point. The metadata methods are slots 194/195. The real platform Binder
base includes `handleShellCommand`; SDK-only headers omit it and would shift
these slots. A compile-time guard refuses that incompatible include environment.

`StockPqReader` calls only metadata and `getDispContentSamples(1, {0}, result)`.
It initializes a **new result** on every read, preserves the start-of-IO
`CLOCK_BOOTTIME` timestamp supplied by its owner, and copies the entire selected
tuple. It validates one channel, result 0, nonzero cached frame, channel 0,
format 9, 256 bins and nonnegative populated counts. A populated bin 0 is valid
black; an all-zero histogram is invalid. `transportOk` alone is insufficient.
V4 and V7 service version/hash pairs are pinned to the two inspected factory
bindings; selected request/result fields were checked in both. That comparison
does not establish compatibility with arbitrary other firmware or versions.

The adapter is an **unconsumed static module**, not yet linked into the installed
controller. There is no sampling thread, histogram enable, new Binder service,
system-server connection, SELinux permission or production cap change in this
commit. `shapeValid` is not session/freshness validity. The eventual owner must:

* Use a dedicated IO worker and `CLOCK_BOOTTIME` milliseconds. Never do vendor
  RPC on SurfaceFlinger, a display handler, OEM callback or private Binder pool.
* Issue all histogram enable/disable operations via the existing OEM serial
  worker, with display/HDR/request/connection generations and cleanup on death.
* Reject late-generation results, preserve both `transportOk` and `shapeValid`,
  and forward the complete frame/channel/bin tuple to `StockOprBrightnessState`.
  Do not cache/replay bins after failure or replace missing samples with zero.
* Keep the existing colour listener separate from the brightness subscriber.
  Blocked IO cannot extend sample permission or prevent display-state cleanup.
* Test the physical frame counter/idle behavior before enabling higher brightness;
  the current raw maximum remains 0.499938 and thermal limits stay authoritative.

Offline commands (no lunch, Soong, Ninja, ROM artifacts, device or IPC activity):

```sh
python3 displaycolor/tests/stock_pq_header.py \
  --library /path/to/stock/vendor.mediatek.hardware.pq_aidl-V4-ndk.so \
  --llvm-bin /path/to/LineageOS/prebuilts/clang/host/linux-x86/clang-r547379/bin
python3 displaycolor/tests/run_pq_reader_tests.py \
  --source-root /path/to/LineageOS \
  --output /path/to/output/pq-reader-host-suite
```

The second command uses real source-tree Binder headers, checks the compiled
virtual call indices, and exercises the actual reader with a fake service.
It is a small native host test, **not** an Android arm64 link, real Binder parcel
round trip, service-manager/SELinux check or device validation. The frozen OEM
binary itself is not executed by these host tests.
