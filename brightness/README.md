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
  HDR gate and OPR/peak-video decisions are separate unfinished integration work.
  Their presence in stock binaries is not evidence that this implementation
  reproduces them. An unchanged sconfig=-1 selects the original default table;
  this jar does not invent game ids or silently write scenario nodes.
* The current 0.499938 raw maximum remains unchanged. This patch does not enable
  1600/4500-nit HBM or claim full factory thermal/charging/performance equivalence.

## Checks

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
installed phone. No lunch, Soong, Ninja, image packaging, ADB or writes occur.

Before deployment, an authorized system build and complete matching AVB chain are
still required. On-device validation must inspect dumpsys display input/cap/reason,
confirm normal/auto/HDR/power-save behavior at natural temperatures, OFF/AOD/FOD
regression, expiry/recovery, and actual final backlight. Never fake temperatures
or directly exercise cooling/charging nodes to manufacture a passing test.
