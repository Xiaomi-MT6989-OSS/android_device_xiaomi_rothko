# Screen-off UDFPS input adapter

This device-owned HIDL 2.1 sub-HAL adds one wakeup, one-shot sensor through the
existing stock AIDL multihal. It does not replace the stock MTK sub-HALs, operate
the fingerprint HAL, control illumination, or produce authentication results.
SystemUI already has MANAGE_BIOMETRIC and uses the standard Doze UDFPS long-press
path to wake the display and wait for real panel readiness.

The original multihal executable SHA-256 is
44636e5b128fb1d6c23d23e2e6f6028890da2e5d6365cb10a8efced75a500f2d.
REA/Hopper and original instructions show its constructor reads both vendor and
ODM sensors/hals.conf, unconditionally, before initialization. The adapter is
listed only in ODM; the original vendor list stays intact.

Input parameters come from this rothko's captured stock Goodix evdev ABI:

- Discover goodix_ts by name and verify key 0x152, ten MT slots, and ABS X/Y
  ranges 0..121999 and 0..271199. Never pin an event number or grab input.
- Coordinates use native panel pixels multiplied by 100. WIDTH values remain in
  their original units; absent widths stay zero, as permitted by the upstream
  Xiaomi UDFPS sensor payload.
- FOD key and coordinate frames can arrive in either order. A contact must be
  live and observed during this activation. Ended contacts, normal touches
  without the FOD key, ambiguous multi-touch, held fingers at activation, and
  SYN_DROPPED are rejected until an appropriate new press.
- Read evdev only while activated. Cancel, trigger, framework initialization,
  or input loss closes the subscription. An eventfd interrupts the blocking
  wait; there is no timed polling or persistent diagnostic process.

The existing sensors init service has the input group and the platform sensors
domain already has read-only input access. No new SELinux permission, touch
mode write, sysfs enable node, hardware ioctl, or invented motion sensor mapping
is needed. EVIOCG* ioctls read input identity/capabilities/state only.

FodInputTest.cpp covers event ordering, stale contacts, one-shot delivery,
cancel/rearm held-finger semantics, missing width and dropped-event recovery.
The recorded 2021-event device trace was additionally replayed offline: two real
FOD downs produce two deliveries; subsequent ordinary touches produce none.

Validation before the image build: host tests, direct Android ARM64 compilation,
linking against current platform outputs, ELF import checks, and an isolated
factory descriptor query/load/unload on the phone. The last check did not call
activate(true) or register with SensorService. It is not screen-off unlock
validation. Cold boot, real registration, success/rejection, AOD, lockout and
display cleanup still require the paired image test.
