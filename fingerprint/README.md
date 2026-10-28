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
