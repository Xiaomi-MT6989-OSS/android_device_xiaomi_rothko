# Reporting without taking ownership of stock thermal control

This is an audit proposal, not the selected product configuration. Do not infer
that a new HAL or configuration has been built or deployed.

## Route assessment

| Route | Evidence | Decision |
| --- | --- | --- |
| Restore the stock V1 HAL and its init/socket | Compatible interface version is possible; archived stock zones select socket mode, but an active event producer is not established | Keep as a reuse candidate; do not add thermal_core speculatively |
| Enable Monitor on the six physical entries | Enables severity calculation and callback reporting, but initializeTrip may write kernel thresholds/hysteresis | Do not apply as a reporting-only fix |
| Set TripPointIgnorable | updateTripPointThreshold attempts the write first; the flag changes failure handling, not ownership | Not a no-write mechanism |
| Set vendor.disable.thermalhal.control | clearAllThrottling writes cooling requests and disables the normal watcher path | Not a reporting-only mode |
| Use existing virtual-sensor identity reports | initializeTrip skips virtual sensors; unmonitored hidden backing sensors provide raw readings | A device-configuration-only candidate requiring real-parser/helper validation |

## Identity report candidate

Four hidden physical entries refer to the already verified zones soc_max,
battery, mtktsAP and mtktsbtsmdpa. They have Monitor=false, no power hints and no
cooling bindings. Six public virtual entries preserve the current public names,
types, multipliers and stock-derived hot threshold arrays. Each uses one backing
entry with WEIGHTED_AVG, one constant coefficient of 1 and zero offset.

This is exactly one input multiplied by one, not smoothing, estimated temperature
or a new thermal policy. readDataByType(SENSOR) calls readThermalSensor and returns
the raw reading; readTemperature applies the public multiplier once. Do not
multiply the backing reading into Celsius and then apply 0.001 a second time.

No TriggerSensor or SeverityReference is assigned: those paths can promote a
physical backing entry to is_watch, defeating the no-trip-write boundary.
Hidden=true alone does not prevent monitoring or writes.

initializeSensorMap skips virtual entries. initializeTrip also skips them before
looking up the thermal zone path. The watcher still starts with an empty set of
kernel-monitored sensors; its timeout invokes thermalWatcherCallbackFunc, which
iterates the watched virtual entries. Monitor=true permits severity evaluation,
initial callback snapshots and subsequent severity-change notifications.

The candidate explicitly uses 2000 ms polling/passive/cache intervals, the
existing source HAL kMinPollIntervalMs. This is a reporting cadence choice,
not an assertion that stock socket delivery has the same latency. No StepRatio,
prediction, PID, cooling bindings or power hints are configured. Existing five
cooling declarations remain queryable; empty binding maps do not create cooling
requests in the normal watcher flow. The existing control-disable property must
not be enabled, because its separate clear-all path still writes cooling nodes.

## Required checks before selecting this route

1. Use the actual parser and helper with simulated read-only thermal files,
   intercepting attempted trip/cooling writes; do not substitute a hand-written
   mathematical model for the production code.
2. Confirm six public temperature/threshold entries, hidden backing entries,
   correct units and reporting at/below/above each stock threshold.
3. Confirm initial registration, type filtering, severity rise/fall, multiple
   callbacks and failure/restart behaviour. Establish the actual hysteresis
   semantics rather than asserting complete stock equivalence.
4. Verify read failure handling and that no backing entry is promoted to a
   monitored physical sensor. Include missing, empty and malformed readings.
5. Check polling cost, Binder delivery, SELinux and coexistence with mi_thermald
   in the later device-validation stage. Do not generate thermal stress or
   SHUTDOWN conditions on the phone to test the software thresholds.

No shared HAL patch is justified yet: this route must be evaluated first under
the device-configuration priority. If the existing implementation cannot meet
the reporting/no-write contract, document the failing source path before choosing
a narrowly scoped HAL change. None of these routes establishes full equivalence
of Xiaomi scene selection, cloud policy or physical display brightness.

## First host checks completed

209 checks passed using the original thermal_info.cpp translation unit, its
original SensorInfo definitions, JsonCpp sources and generated AIDL enum headers.
Selected helper functions were copied byte for byte from thermal-helper.cpp:
getSeverityFromThresholds, setMinTimeout, updateTripPointThreshold and initializeTrip.
Only Android file/property/logging boundaries and unused estimator construction
were replaced by host fixtures. Estimator use fails the test rather than
pretending to implement a model. These are isolated function checks, not the full
Android service.

The parser accepts both the existing configuration and the candidate. Checks
confirm the existing unmonitored flags, six candidate public reporting entries,
four hidden physical inputs, preserved types/thresholds/units, empty trigger and
cooling bindings, and no smoothing. Duplicate names, absent identity coefficients
and a hidden-but-callback-enabled entry are rejected by the real parser.

The actual trip initializer is called with an empty path map under both genl
settings: no path lookup, read, write or kernel-monitored entry occurs. As a
positive instrumentation control, enabling Monitor on the six original physical
entries with fake user_space policies produces 12 captured writes, one temperature
and one hysteresis trip write per entry. This confirms that the fixture can detect
the unsafe direct-Monitor route; no real thermal paths are accessed.

The real severity function was also exercised at each finite hot threshold and
on falling just below it, with the currently configured zero hot hysteresis.
Matching those software results does not establish original daemon hysteresis or
full stock control equivalence.

Still pending: actual virtual raw-read/unit conversion, initial and subsequent
callback delivery, watcher/error/restart behaviour, polling cost and Android/device
integration. The candidate remains outside the selected product configuration.
Do not use these 209 checks to mark the thermal reporting gap repaired.
