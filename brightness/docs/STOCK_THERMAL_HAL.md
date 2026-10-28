# Stock Thermal HAL provenance and reuse assessment

Target: OS3.0.307.0 vendor/bin/hw/android.hardware.thermal-service.mediatek,
SHA-256 `9573fc589f691709c2706339f6d39390dc132cd6bc759fefadabfdc1d090282d`.
Addresses below identify this exact ELF; none are production memory offsets.

## Confirmed static facts

The initializer beginning at 0x9530 allocates seven-float threshold vectors.
CPU uses constants at 0x3ab8, GPU copies the saved CPU vector, BATTERY uses
0x3a2c, SKIN uses 0x3a48, POWER_AMPLIFIER uses 0x3a64 and NPU also copies CPU.
The NPU hot vector store is at 0x9c04 (type slot 9, structure stride 0x50).

| Current type | Factory hot thresholds, severity indices 0 through 6 |
| --- | --- |
| CPU / GPU / NPU | NaN, NaN, NaN, 85, 90, 100, 117 |
| BATTERY | NaN, NaN, NaN, 50, 55, 59, 60 |
| SKIN | NaN, 53, 55, 57, 70, 80, 90 |
| POWER_AMPLIFIER | NaN, NaN, NaN, 68, 90, 100, 110 |

All six current JSON HotThreshold arrays match these factory vectors. Numerical
provenance does not prove complete runtime equivalence, including hysteresis,
initial callbacks, units, sysfs mappings or notifications.

The ELF contains multiple 0x40-stride zone/type-name tables, including an older
mtktscpu/mtktsAP family and a soc_max/ap_ntc family with separate PA entries.
Do not treat every table entry as an available physical sensor, or copy one
family's names without establishing which detection branch the kernel selects.

The constructor calls zone detection (0xa3c0), passes its result to watcher
setup (0xd880), then starts the watcher. The setup chooses a kernel uevent socket
for mode 1; other modes use the init-provided thermal_hal_socket and listen/accept.
The event parsers collect names and the helper matches them to type slots before
reading temperature and dispatching through its callback. Actual selected mode,
notification producer and event delivery remain unverified on this device.

## Why not just enable Monitor in the source HAL?

hardware/mediatek's initializeTrip can write trip_point_0_temp and
trip_point_0_hyst for watched zones whose policy/genl path supports it. Therefore
enabling callback flags can change kernel notification configuration even without
cooling bindings. That is a wider behavioural change than adding a reporting bit.

## Stock reuse viability, still a candidate

The stock fragment declares Thermal AIDL V1; the device retains target-level 8,
whose framework matrix includes V1. V1 is therefore not automatically disallowed
merely because the system is Android 16. Full merged VINTF validation is separate.

All eleven direct DT_NEEDED library names exist in the last-build product, and
their combined exports cover the stock executable's required global symbol names.
This is a name-only screening check. It does not establish linker namespace
visibility, symbol versions, transitive closure, C++ ABI/layout, successful load,
or correct runtime sensor/event behaviour. In particular, stock reuse must bring
its paired init/socket declaration and manifest, remove the competing source
service selection, and preserve reproducible extraction. Do not run both services
or patch a V1 fragment to advertise V3.

This assessment does not yet change the product HAL selection or thresholds.
Next resolve the kernel-family branch and notification producer, then assess
dependency/ABI and lifecycle integration before building or deploying a candidate.

## Archived stock route and producer boundary

A retained rooted stock collection contains 70 contiguous thermal_zone indices,
including soc_max but neither mtktscpu nor ap_ntc. Applying the bounded detection
logic to those names selects mode 3, hence the init socket route. This is a
reconstruction from a past collection, not observation of a currently running HAL.

thermal_core has a confirmed socket sender: 0x33d60 creates an AF_UNIX stream
socket, copies /dev/socket/thermal_hal_socket, connects at 0x33e00 and sends at
0x33e8c. Its message strings include thermal_hal_notify and thermal_hal_update.
Nevertheless, its stock init service is commented out and a retained stock ps
snapshot lists the thermal HAL, mi_thermald and system_ext thermald, without
thermal_core. That snapshot does not rule out short-lived processes or another
producer. Do not enable thermal_core merely to satisfy this socket dependency.
Restoring the HAL alone is not yet a demonstrated callback repair.

The separate, running system_ext/bin/thermald is a kernel-signal alert bridge.
Its main at 0x4520 opens clsd_pid and clmutt_tmd_pid under /proc/driver/thermal,
registers a signal handler and writes its PID (0x4664/0x46e0). Signal branches
construct THERMAL_WARNING and THERMAL_DIAG intents for ActivityManager delivery.
It is not interchangeable with the HAL's socket-event producer. This daemon is
not selected by the current product; equivalent alert handling has not been
established. Receiver compatibility must be reviewed before reusing its native
Binder parcel path. Do not claim that the absence removes every kernel or
standard-framework overtemperature protection.

Three contracts therefore need separate completion: standard thermal callbacks,
kernel/vendor alert delivery into Android, and mi_thermald's cooling/scenario
control. Existing brightness host tests do not cover the first two contracts.

Finally, framework dumpsys may print "ThermalHAL AIDL 3" using the framework's
compiled IThermal.VERSION. It is not proof that the remote service implements V3;
use actual interface version/metadata instead. The stored stock dump and V1 ELF
must not be treated as a contradiction solely on that label.

## Alert sender cannot be copied unchanged

The stock system_ext thermald broadcast path targets service "activity" and
descriptor "android.app.IActivityManager". At 0x442c it passes constant 12 to
the Binder virtual call at 0x443c, with request/reply Parcel pointers and zero
flags, matching IBinder::transact's signature and slot 0x28.

The local last-built framework.jar's IActivityManager$Stub constants instead
identify broadcastIntent as 25, broadcastIntentWithFeature as 26, and
logFgsApiStateChanged as 12. Both the executable instructions and the generated
class were inspected. This is a concrete incompatibility in the native alert
client, independent of whether its ELF dependencies resolve. No transaction was
executed to demonstrate it and no thermal event was injected.

Do not reorder the upstream AIDL to accommodate this private native client.
Nor is changing 12 to 25 alone a validated repair: the client manually writes
Intent/Bundle/argument fields, and the expected MediaTek receiver has not been
established in this port. A proper adaptation must cover both serialization and
the receiving behaviour, preserving the original event semantics.

LineageOS already has standard thermal handling: ThermalManagerService receives
HAL temperature callbacks, handles SHUTDOWN severity for applicable types, and
SystemUI's PowerUI subscribes to skin/USB events for warnings. Those are distinct
from MediaTek's THERMAL_WARNING/THERMAL_DIAG broadcasts. The existing standard
handlers do not establish that kernel signal events have been translated into
that chain. A focused search found no matching MediaTek actions in this tree's
SystemUI Java/Kotlin/XML; a broader application search was stopped and cannot
support a claim that no receiver exists anywhere.

Current decision: retain the working daemon/control and brightness-cap paths;
do not add the raw stock alert executable or thermal_core as an unverified fix.
The Thermal HAL notification route and the alert bridge remain explicit open
integration items, with no new upstream or binary patch made for this finding.
