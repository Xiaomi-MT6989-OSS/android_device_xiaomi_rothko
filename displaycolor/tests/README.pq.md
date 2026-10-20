# Stock PQ readback and brightness transport

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

The adapter is now consumed by `HistogramService` in the existing controller
process. The production source path is:

`SF scene -> OEM serial worker / feature 56 -> dedicated PQ IO worker -> cached
IRothkoBrightness -> separate system-server IO worker -> StockOprBrightnessState
-> min(factory thermal cap, OPR cap) -> existing common display modifier`.

`IRothkoBrightness` is independent of the SF colour listener, permits only the
system-server client in policy, and never runs vendor method IPC in its pool.
Consumer Binder identity plus request id separate restarted consumers. A 1200 ms
lease forces eventual disable if the consumer stops renewing; atomic death flags
also invalidate old cached permission before queued work runs. The reader sleeps
when unrequested/OFF/DOZE and reads at most every 200 ms while active. All enabling
and disabling uses the existing OEM serial worker, not OEM callbacks. A captured
scene revision prevents an old worker scene from authorizing a new scene. A
late successful enable is still followed by disable before the next generation.
Physical disable may wait for the serial OEM call already in flight; validity
invalidation does not wait for it.

The private stubs deliberately retain the original controller's **platform static
compilation** even though the executable is installed on vendor. Changing them
to a vendor compilation unit would change Binder stability and break platform
callers. The source rejects that variant; OEM transport remains the stock frozen
proxy. No VINTF ABI is being added or substituted.

`shapeValid` is not session/freshness validity. The Java consumer retains the full
tuple, uses fresh session baselines, rejects delayed replies and repeated frames
that have expired, and takes the minimum with thermal/system limits. Known HDR
composition bypasses only the stock SDR OPR table; unknown/stale identity cannot
authorize that bypass. Leases/2500 ms freshness limits are adaptation watchdogs,
not OEM waiting times or extra smoothing. No scalar-95000 fallback is present.

This is source integration, **not yet built/deployed/validated on the device**.
Physical frame-counter/idle, ROI, SELinux/class loading, AOD/FOD/colour regression
and actual backlight still need validation. Raw maximum remains 0.499938; this
change does not enable 1600/4500-nit HBM or implement the Gallery 37/35 C gate.

Offline commands (no lunch, Soong, Ninja, ROM artifacts, device or IPC activity):

```sh
python3 displaycolor/tests/stock_pq_header.py \
  --library /path/to/stock/vendor.mediatek.hardware.pq_aidl-V4-ndk.so \
  --llvm-bin /path/to/LineageOS/prebuilts/clang/host/linux-x86/clang-r547379/bin
python3 displaycolor/tests/run_pq_reader_tests.py \
  --source-root /path/to/LineageOS \
  --output /path/to/output/pq-reader-host-suite
python3 displaycolor/tests/check_histogram_transport.py \
  --source-root /path/to/LineageOS \
  --aidl /path/to/output/host/linux-x86/bin/aidl \
  --output /path/to/output/transport-check \
  --intermediates /path/to/output/soong/.intermediates
```

The second command uses real source-tree Binder headers, checks the compiled
virtual call indices, and exercises the actual reader with a fake service.
It is a small native host test, **not** an Android arm64 link, real Binder parcel
round trip, service-manager/SELinux check or device validation. The frozen OEM
binary itself is not executed by these host tests.
The transport check additionally generates the new private Java/NDK source,
checks the native service/stubs and the 47-assertion IO-free ownership state
machine. Its optional `--intermediates` checks the actual controller with existing
platform headers, the final entry point with vendor headers, rejects a wrong
controller variant, and checks the real generated Java AIDL/client against an
existing platform framework.jar. It does not run Soong or validate an arm64 link.
