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

## Read-chain checks exposed an existing parser defect

The second host harness compiles the real ThermalFiles translation unit as well
as the original configuration parser. It extracts unchanged readThermalSensor,
readDataByType, readTemperature, getSeverityReference, fillCurrentTemperatures,
initializeSensorMap and severity function bodies. Sensor-status structures and
the temperature-conversion macro are copied unchanged from the production header.
Android I/O, clock, statistics and Temperature value/transport boundaries are
fixtures; this is not the complete service or an AIDL ABI test.

The result is **50 passing and 19 failing assertions**, with a failing exit code
retained. Passing checks cover 35000 raw -> 35 C once, six public entries in the
initial callback-filtered snapshot, BATTERY type filtering, hot/falling severity
and pending-notification flags, missing/empty file errors, and cache expiration
without smoothing. A pending flag is not proof that Binder received a callback.

The failure is in the existing std::atof physical-reading conversion, not the
identity formula. For each public type, "garbage" becomes 0, "85000junk" becomes
approximately 85 C, and "inf" becomes infinity, all returned as valid readings.
A further check shows malformed input clearing a previously established CPU hot
severity. An original-configuration control also accepts garbage as zero, proving
that the defect predates the candidate. No real temperatures or thermal events
were injected; all inputs are host fake files.

This prevents treating the device-JSON proposal as a complete robust solution.
JSON cannot make atof reject malformed input. A justified follow-up source fix
would strictly parse the complete trimmed physical input, reject nonfinite values
and propagate a read error before updating cache/severity. Existing numeric and
kernel-sentinel semantics must be retained; do not invent temperature bounds,
replace failure with zero, or add a filter. libbase ParseFloat checks full-string
consumption, range and errno, but still needs an explicit finite-value check for
NaN. Such a generic reader fix belongs in an independent, narrow HAL change,
not a framework thermal-policy patch or a proprietary binary modification.

The shared HAL source and selected product JSON remain unchanged at this audit
stage. Actual watcher/notification dispatch, read-error retry, restart and cost
checks are still required, independently of the proposed parsing correction.

## Narrow reader correction implemented, not deployed

hardware/mediatek commit affc75f replaces the physical reader's atof with libbase
ParseFloat plus an explicit finite check. A local parsed value is assigned only
after validation. Failed reads return ERROR before cache/severity updates. This
is a general source-reader correction; it changes neither thresholds nor stock
cooling ownership and requires no framework or proprietary-binary patch.

The expanded real-reader host suite now passes 118 assertions, including the
previous 19 failures, NaN/negative infinity/overflow/blank input, valid zero and
negative temperatures, whitespace, existing kernel sentinel conversion, and
preservation of the last successful cache and hot severity on a failed fresh read.
The original six-entry configuration also rejects malformed data with this fix.
These remain host boundary checks; watcher dispatch and failed-read retry are
separate pending checks. The device reporting candidate is still not selected.

The dependency manifest pins the upstream MediaTek base, not this unpublished
local commit. Reproduction must apply the independently exported correction after
that base; do not assume an upstream checkout includes the fix. No ROM was built
or deployed as part of this source correction.

## Watcher dispatch and retry boundary

A third host harness adds the unchanged thermalWatcherCallbackFunc body to the
real parser/read-chain fixture. It reproduces three retry failures: all monitored
reads return ERROR, all return UNDER_COLLECTING, or a healthy entry with a long
poll interval masks failed siblings. The first two return milliseconds::max;
its conversion to the current Looper pollOnce int argument produces -1. The
actual Android Looper thread was not run; the helper return value and source
call-site conversion were checked separately.

Independent hardware/mediatek commit 59892c1 makes an unsuccessful read vote the
existing 2000 ms kMinPollIntervalMs. It does not change successful intervals,
thresholds, cache/severity or cooling ownership. Failed entries remain eligible
for retry because their successful last_update_time is not advanced. This is a
retry interval, not a thermal filter or an added brightness/charging policy.

The host result changed from 15 passing/3 failing to 18 passing/0 failing. The
actual callback function dispatches CPU/GPU/NPU hot and falling changes through
cb_, suppresses duplicate unchanged severities and fabricated read-error events,
and resumes valid reads on the next scheduled invocation. Initial snapshot and
type filtering were checked in the preceding read-chain harness, not by assuming
NONE-to-NONE produces a watcher change event.

Statistics, power and cooling helper bodies remain boundary fixtures; nonempty
bindings, power hints and cooling write requests are rejected by the fixture.
These checks do not validate the real cooling controller, Binder recipients,
Android thread scheduling or service restart. Production source binds cb_ to
Thermal::sendThermalChangedCallback, whose type-filtering/dead-client handling
still requires separate boundary/runtime validation.

The two minimal shared corrections must be replayed after the pinned upstream
base in order (affc75f then 59892c1). The identity-reporting device configuration
remains a candidate, and no claim of stock-policy equivalence or deployment is
made from these host results.

## Subscription lifetime and real callback-queue checks

The original register/unregister/send methods and CallbackSetting definition were
tested with the real read helper and queued/Binder boundary fixtures. Thirteen
checks passed, but unregistering a callback and registering it for a different
type before draining its queued first snapshot delivered the stale type as well.
hardware/mediatek commit 1480fd5 adds a unique shared token to each registration;
the queued handler must match the same registration lifetime. All 14 checks then
pass, including initial type filters, all-type delivery, duplicate/null rejection,
cancelled pending work and dead/failed-client removal. These are transport-fixture
checks, not remote Binder validation.

The real nested Looper class has a separate defect: aborted_ was uninitialized,
and the outer while condition read it without the mutex used by shutdown. A
standalone harness compiled that actual class/constructor and method bodies with
real host threads. The reused-nonzero-storage case and twenty FIFO/construction/
destruction rounds produced 2 passes/20 failures on the original host build.
Commit 278b978 initializes the flag and checks shutdown only under the queue mutex;
all 22 checks pass after the fix. This is a bounded host regression, not a race
detector, an Android init restart test or proof of full HAL object teardown.

The existing init contract starts the source HAL via vendor.thermal.link_ready=1,
after the thermal symlink oneshot. The HAL scans actual thermal zones itself.
No init change or extra stock daemon has been selected based on these host checks.
Full constructor readiness, HAL/helper destruction ordering, framework reconnect,
runtime permissions and polling cost remain separate acceptance boundaries.

There are now four independent shared-source fixes to replay after the pinned
base: affc75f, 59892c1, 1480fd5 and 278b978, in that order. They address demonstrated
generic reader/notification defects, not device-specific thermal thresholds or
factory cooling policy. The device identity-reporting candidate remains unselected.
