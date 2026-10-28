# Brightness and thermal integration audit

This records static source/material review, not new device validation.

| Layer | Current implementation | Review boundary |
| --- | --- | --- |
| Panel calibration and HBM | Device XML preserves the stock five nits/raw knots, 6001 lux admission and ratio map | Linear interpolation, normalized settings and AOD mapping are explicit Android adaptations; physical nits remain unmeasured |
| Factory daemon | Stock mi_thermald, its init file and selected vendor/ODM thermal configurations | Identical bytes do not prove runtime permissions, scenario producers or policy selection |
| Display thermal bounds | Device Java policy consumes the unchanged factory multi-factor XML and read-only sysfs outputs | Freshness/invalid-input restrictions are conservative port safeguards; cloud and Gallery boost are not reproduced |
| Framework integration | Optional device-policy class, bounds before HDR, common maximum retained by HDR modifier | Separate upstream hooks are necessary for runtime inputs; an overlay alone cannot implement them |
| Thermal AIDL | Source-built MediaTek service plus device thermal_info_config.json | Its reporting thresholds and notification behaviour are not established as factory-equivalent |

The selected extraction list contains 44 thermal-related entries. All maintained
files matched the saved stock artifacts byte for byte in this audit. The factory
multi-factor brightness table and mi_thermald init also matched. This is a source
material check, not proof that every file is installed, loaded or exercised.

Do not restore thermal_core merely because its binary exists: the stock
init.thermal_core.rc service and start actions are commented out. The separate
stock system_ext thermald has an active init definition, but is not selected by
this device. Its responsibilities/dependencies need analysis before deciding
whether an existing replacement covers them. Do not run multiple thermal writers
as a speculative fix.

## Thermal AIDL notification gap

All six configured sensors omit Monitor, SendCallback, SendPowerHint and cooling
bindings. In the current hardware/mediatek parser, callback and power-hint flags
default false; no hard/PID binding enables throttling. Consequently is_watch is
false. The watcher skips these entries and dispatches callbacks only for send_cb.
Explicit temperature reads and sustained framework notifications are distinct.

The source HAL contains cooling writes, but this configuration alone does not
prove it is actively applying a competing cooling policy. Its
vendor.disable.thermalhal.control path can explicitly clear cooling requests;
the property defaults false and is not set in the reviewed device property files.
Do not enable that property to make the service "read only".

Follow-up ELF analysis matched all six HotThreshold arrays to stock constants;
see `STOCK_THERMAL_HAL.md`. This resolves their numerical provenance, but not
all sensor mapping, units, hysteresis and callback semantics. The current JSON
is device-maintained, not an unchanged stock thermal_info_config.json. The source
HAL's monitoring path may also write kernel trip points: do not simply enable
flags without reviewing that ownership. Guessing new thresholds would contradict
the stock-policy goal.
The stock daemon, framework thermal notification service and display brightness
policy require separate runtime validation.

## Software baseline

The seven existing brightness host programs passed 1,789 assertions: factory
table/state/parser 1,466; reader lifecycle 63; OPR state/protocol 117; OPR transport
58; peak permission 20; doze brightness 28; common modifier pipeline 37.
These use Android boundary fixtures. They do not validate class loading, real
Binder/SELinux, actual sensor readings, calibrated output or runtime thermal
notification delivery. No temperatures, cooling states or charging nodes were
modified for this audit.
