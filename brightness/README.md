# Rothko factory display thermal limits

This jar implements the packaged OS3.0.307.0 factory fallback table. It does not
import MIUI services or write thermal, charging or panel nodes. The small generic
framework hook is separately committed; the class name is selected by the device
overlay and is empty on other devices.

## Sources and scope

`configs/displayconfig/common_multi_factor_thermal_brightness_control.xml` is an
unchanged copy from stock product, SHA-256
`a8a0f20094366aa998ccff432370bb95bcffbf7126d6126b82f6fbd09c39e470`.
The original `ThermalBrightnessController` DEX instruction inventory and node/unit
analysis are recorded in the project research directory dated 2026-10-19.

Inputs: `board_sensor_temp` (millidegrees C), `sconfig` (scenario id),
`thermal_max_brightness` (nits, zero means no separate cap). If the original
`persist.vendor.ntc.flag` is enabled, `display_therm_temp` is also required. The
present phone has no enabled NTC flag and no such node; it is not fabricated.
Automatic mode uses the existing automatic-brightness controller's lux estimate,
without a second ALS subscription or filter; manual mode uses
stock's 6000 lux thermal-table input. This does not implement the separate OEM
manual sunlight feature.

The parser keeps original half-open intervals, nits and scenario ids. Unknown
scenario ids select stock default 0. The magnitude deadband matches the shipped
`asSameTemperature` default (integer part first, then 0.5 C board / 1 C NTC).
There is no additional temporal smoothing or interpolated thermal curve.

The common modifier converts nits with the active DDC, caps SDR target and the
common maximum, and runs before the existing HDR modifier. HDR must continue to
take the common maximum in both its builder and aggregated state. The standard
thermal/power/low-power controls and existing HDR thermal gate remain in place.

## Explicit adaptation differences and limits

* The stock JNI watcher uses inotify `IN_MODIFY` for the original thermal nodes.
  FileObserver uses the same event mask here, registering before the baseline
  snapshot and coalescing event bursts into an immediate worker read. Additional
  1000 ms reads are a health/retry fallback. OFF/DOZE/DOZE_SUSPEND release the
  watches and stop periodic reads; screen-on creates a fresh watch generation.
  Moved/deleted nodes revoke old data and cause re-registration. A read spanning
  watch invalidation cannot grant a new snapshot. The cap getter checks the atomic
  watch epoch as well: invalidation already received on the observer thread must
  revoke the old snapshot even before its display-handler notification runs.
  These lifecycle safeguards are
  adaptations, not a claim of byte-for-byte OEM JNI behavior.
* Samples expire after 2500 ms by elapsed realtime; the getter also checks age
  across suspend. Old generations cannot grant a new wake session permission.
  Failed/missing/stale input uses the tighter of the previous cap and the factory
  table's minimum positive cap (160 nits). A fresh valid sample is needed to lift
  that restriction. A lower valid independent safety cap remains effective.
* Unknown automatic-mode lux uses the tightest applicable stock lux band. Invalid
  or above-table temperatures cannot turn a bound into unlimited brightness.
  These failure cases are deliberate conservative differences from stock's
  possible NaN/retained-value behavior.
* Only the packaged factory fallback is available. HyperOS cloud configuration,
  actual game/PowerHAL scene producers, Dolby/camera overrides, the 37/35 C image
  HDR gate and full peak-video decisions are separate unfinished integration work.
  Their presence in stock binaries is not evidence that this implementation
  reproduces them. An unchanged sconfig=-1 selects the original default table;
  this jar does not invent game ids or silently write scenario nodes.
* The current 0.499938 raw maximum remains unchanged. This patch does not enable
  1600/4500-nit HBM or claim full factory thermal/charging/performance equivalence.

## Checks

`OprBrightnessClient` now connects the production provider to the independent
cached `IRothkoBrightness` interface. It uses a separate IO handler, not the
display handler or thermal reader. It requests statistics only for ON automatic
mode with known lux strictly above 20000; other automatic ON states only read
cached composition identity. OFF/DOZE/manual/stop release requests. Consumer death,
expired replies, request changes and native connection/display/HDR generations
cannot reuse previous permission. Its getter checks an atomic Binder death epoch
even before queued display notifications run. OPR is an extra cap, never a reason
to relax the factory thermal or independent safety limit.

The framework also relays its existing actual HDR-layer subscription to the optional
policy. This includes extended-range RGB content that need not choose PQ/HLG at the
color output. A known upstream presence/absence takes precedence over the native
candidate; before its first report the explicit native PQ/HLG candidate is only a
fallback. Native scene/freshness/death checks still apply. Known HDR below the distinct
peak lux threshold releases the ordinary histogram request. Above 100000 lux,
automatic HDR can request statistics for the separate factory peak decision. Every
actual HDR transition needs a new baseline even if statistics stay enabled. The
common thermal and independent safety limits remain active. No extra SF/ALS
subscription, app-name inference, picture/video inference or higher raw maximum
is introduced. Display-token replacement/removal invalidates the upstream report,
and delayed callbacks from old registrations are rejected even for a reused token.

The packaged peak branch is also enforced as an additional cap: normally 1600
calibrated nits; only automatic brightness, actual known HDR layers, lux strictly
above 100000 and a fresh advanced histogram with gray <=210 can return 4500. This
mirrors the factory peak predicate (whose `isHdrVideo` checks HDR layers/Dolby, not
media-file type) with conservative transport/freshness guards. Invalid gray never
grants peak, consistent with the packaged flag being false. Native histogram
ownership still requires ON, a live consumer lease and acknowledged OEM enable;
HDR no longer unconditionally prevents a requested peak sample. Lower thermal or
safety caps always win. This is a conditional limit, not a brightness target or
permission to change the current 0.499938 raw maximum. The original Gallery token
request, 37/35 C gate and up-to-2.25 boost path remain separate unfinished work;
their limits must not replace the generic HDR ratio map indiscriminately.

`StockOprBrightnessState` consumes the full PQ reply: transport/result, cached frame counter,
channel 0, stock format 9, 256 bins, and nonzero bin total. A valid black image is
accepted (all pixels in bin 0); a zeroed read failure is rejected. First-session
cache baselines and repeated frames cannot renew permission. Frame wrap/reset,
read-start timestamps, generation isolation and a 2500 ms health deadline are
explicit adaptation safeguards, not a claim of stock error handling or filtering.
The SDR table/lux gate is pinned to the stock APK resources; HDR OPR is disabled
on rothko and must not reuse this table. The transport begins new local sessions
at display/HDR/request/Binder generation changes and still combines nits with
thermal and all existing caps. Source integration does not demonstrate full screen ROI,
hardware frame progress, correct PQ ABI, SELinux, or physical HBM.

From the device directory, run:

```sh
python3 brightness/tests/run_host_tests.py \
  --source-root ${SOURCE_ROOT} \
  --output ${PROJECT_ROOT}/out/bringup/stock-brightness-policy-host
```

The runner performs small host Java unit checks only, with boundary vectors
independently derived from stock XML. Deterministic Android handler/DDC fixtures
exercise the actual reader and generic modifier; they do not verify Android
class loading, framework API compilation, SELinux, physical calibration or the
installed phone. The suite also checks cached-client cancellation/death/expiry and
the production minimum between thermal and OPR caps. No lunch, Soong, Ninja,
image packaging, ADB or device writes occur. Private AIDL/controller checks are
documented in `displaycolor/tests/README.pq.md`.

Before deployment, an authorized system build and complete matching AVB chain are
still required. On-device validation must inspect dumpsys display input/cap/reason,
confirm normal/auto/HDR/power-save behavior at natural temperatures, OFF/AOD/FOD
regression, expiry/recovery, and actual final backlight. Never fake temperatures
or directly exercise cooling/charging nodes to manufacture a passing test.
