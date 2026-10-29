# Stock fingerprint integration

The product retains rothko's stock mfp-daemon and Goodix implementation, exposing
the original IFingerprint/default AIDL V3 service. The device service adds the
original FodEngine condition notifications; it does not make authentication,
template or lockout decisions. Those remain with the biometric framework and
stock HAL/TEE.

The selected Goodix module, stock VINTF fragment and two private extension
libraries match the saved OS3.0.307.0 materials byte for byte. mfp-daemon has four
recorded DT_NEEDED redirects to isolated stock bindings in stock-runtime-compat.json.
Its executable sections and rodata match stock. This is static compatibility
evidence, not proof of all runtime ABI behaviour or authentication security.

## Ownership

- RothkoFodService runs as a device-specific system service, serializing original
  condition transactions on its handler thread. Framework/display callbacks only
  enqueue work. The current verified device sensor ID is 5.
- FodStateMachine requires a real request, matching user, attached overlay and
  known unlocked lockout state. It reports actual display power/fade state and
  keeps a valid keyguard operation across the verified power transition.
- The optional UdfpsLifecycle framework observer supplies request, overlay,
  lockout, display and death events. It is a local adaptation, not an interface
  already provided by unmodified upstream LineageOS.
- The one-shot input sub-HAL lives in sensors/udfps. It feeds the normal SystemUI
  Doze long-press/pulse route. It owns touchfeature mode 16 only; stock fingerprint
  owns mode 10, illumination, capture and authentication.

hardware/xiaomi's existing UdfpsHandler is an extension to its own fingerprint
HAL wrapper, whose init takes fingerprint_device_t. It is not a directly
pluggable lifecycle observer for the selected stock mfp-daemon. Replacing the
whole service to use that hook would require a separate ABI/behaviour assessment;
the existence of that interface alone does not justify replacement.

The init service stanza matches stock. Additional node/data setup is scoped to
rothko, instead of copying the generic stock init file's many other-vendor paths
and calibration permission rewrites. Device policy denies persist calibration
mutation; this is not a template or calibration repair tool.

## Validation boundaries

Existing host FodStateMachineTest and FodInputTest pass in this audit. They cover
software transitions and evdev interpretation, not the full system service,
Binder, driver, TEE or security certification. Historical paired-image tests
include enrollment, enrolled/wrong-finger distinction and several OFF/AOD/power-key
paths. OFF unlock can briefly show the standard Doze pulse clock; do not describe
that as a guaranteed clock-free display path.

Further review must verify each original protocol command/caller, the minimal
scope of shared lifecycle hooks, real transport death/blocking behaviour, user
switching/stopping, overlay/controller replacement, and input-service cleanup.
The service's pending-transaction diagnostic is not a Binder timeout mechanism.
Do not remove hooks, weaken lockout, alter sensor strength or add direct panel
commands merely to simplify this layering. No new firmware build or device test
has been performed in this audit stage.

## Protocol provenance and user-stop review

Archived stock framework/SurfaceFlinger caller analysis links extension transaction
1 to the descriptor's two integer arguments. Conditions 1/2 report overlay and
fade existence, condition 3 carries composer power mode, condition 4 uses paired
enroll/auth start and stop states, condition 5 carries real lockout, and condition
7 permits the original keyguard low-brightness path. It is not a direct command
to manufacture panel HBM. The stock and selected FOD touch-control property is
2.0. Condition 6's separate light-monitor path is not used by this bridge; do not
enable extra commands merely because the native daemon accepts their numbers.

SystemUI's config_udfpsReportOverlayLifecycle defaults false and the device
overlay opts in. The hidden lifecycle event is consumed by the framework instead
of forwarding it as a stock HAL UI-ready event. These distinctions must remain
when minimizing shared patches.

Device commit 6134492 restricts onUserStopping cancellation to the current user,
checking the user on the serialized FOD handler. Previously, stopping a background
user/profile also cancelled the foreground request. A host test of the unchanged
callback method body and real state machine reproduced two failures, then passed
all six checks after the correction: unrelated/old-user stop preserves capture,
current-user stop cancels it, and late overlay events cannot reactivate it.
Handler/TargetUser are fixtures; this is not Android multi-user/profile or
work-profile application-authentication acceptance. No lockout or authentication
decision was changed.

## Returning transport faults and input cleanup boundary

The actual connect/send/updateAndApply method bodies and state machine passed a
host protocol-fixture suite. It checks baseline-before-activation, idempotent
wire updates, finish/late-overlay handling, partial activation with an ambiguous
reply, rejected/exceptional transactions, unknown cleanup outcomes, unsupported
version, missing service/API, death-link failure and a late old-service death
after replacement. Cancelled requests do not replay into a new service. Parcels
are recycled and completed transactions clear their pending diagnostic.

Binder, Parcel, ServiceManager and handler are fixtures, not Android transport.
The suite's repeated boundary assertions are not independent hardware scenarios.
No source change was needed for these returning-fault cases. An indefinitely
blocked synchronous Binder call remains a known limit: pendingMs observes it but
does not cancel it, and the serial handler cannot deliver cancellation while
that call remains blocked. Do not add a timeout that falsely declares the remote
hardware stopped.

The input adapter uses a transition lock and generation check to serialize
activation/cancellation/delivery; trigger, normal input loss and framework
reinitialization pair mode16 cleanup without touching mode10. This is static
ownership review, not a complete native concurrency test. The fatal-poll path
has since been reproduced and corrected in the device input adapter; its 38-check
repository-owned host suite and scope are documented in sensors/udfps/tests/worker.
Failed final disable during unload and touch-service blocking remain explicit
limits. Passing the poll-recovery suite does not establish full driver/Binder
concurrency or guaranteed hardware cleanup.

## Controller and shared-observer review

A host fixture of the actual FingerprintProvider UI-event/controller methods and
this device state machine passed 15 scenarios: absent observer, standard event
forwarding, same-Binder replacement, current-controller death, late old death and
old-request detach, failed death-link registration, cancellation and fresh-request
recovery. Handler, scheduler and Binder are fixtures; this does not validate
Android process restart or hardware concurrency. No production change was needed.

SystemServer starts the device-specific services before SystemUI. Existing
biometric state/authentication listeners lack the exact request identity needed
here; biometric context listeners do not supply the actual overlay/ColorFade and
physical-power combination. The legacy hardware/xiaomi HAL wrapper is not a
direct plugin for the selected stock AIDL daemon. Retain the optional, separately
maintained lifecycle hook pending integration review; this is not a claim of
upstream acceptance or proof that no alternative architecture is possible.
