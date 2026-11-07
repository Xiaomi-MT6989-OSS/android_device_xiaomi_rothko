# Stock DisplayFeature startup order

The OS3.0.307.0 service publishes its Binder endpoint before initializing its
native HAL. An early registerCallback stores a Binder callback in a fixed
five-slot array and returns OK, but skips native registration while the HAL is
uninitialized. Initialization does not replay the saved callbacks. On the
2026-11-06 device test, HAL initialization was complete and one Binder callback
was stored, but DisplayUtil::mCallback was null. Restarting only our controller
registered a native callback and restored confirmed mode changes. This is
evidence for the race, not a production recovery mechanism.

`displayfeature_startup_fixup.py` orders the existing initialization before
publication in the original executable. It accepts only these SHA-256 digests:

- Stock: `ebf52e8480c95f317ff9f4bd3ba564fad43e7f5b4c3d65c68e862d03c7dcde76`
- Fixed: `fd2e1578475e27c14d59ed83738bdee7b362d616aed302fbc994dc9ece183f8d`

This is an offline extraction fixup, not a runtime memory patch. It rotates the
existing instructions inside main, preserves the null-object guard, reloads
the object after the initialization call, and relocates affected PC-relative
instructions and incoming branches. The original error/cleanup paths remain.
The changed range is inside the uniform unwind state at VA 0x4704–0x48f4;
all bytes outside main, including unwind tables and exported ABI, are unchanged.
Addresses identify this exact artifact only. Other firmware versions fail closed
until independently reviewed. The input/output sizes remain 24864 bytes.

The extractor invokes this helper; vendor PROVENANCE records original and
transformed digests. There is no startup sleep, callback polling, service restart
loop, extra initialization call or synthetic completion. Initialization return
semantics remain stock; reordering does not make HAL failures successful.

The host test checks digest pinning/idempotence, rejection of unknown input,
outside-main preservation, every relocated instruction/target, the object
guard/reload, modeled publication paths and call counts. It is not an ARM
emulator and does not prove register behavior, Binder scheduling or HAL success.
Cold boot, repeated mode changes, service death, power/HDR transitions and
persistence still require device validation of a new image.

Evidence is in the project's November 6 unified-build runtime directory:
`color-startup-registration-race.json`, `oem-native-callback-state.json`,
`oem-native-callback-after-reregister.json`, and
`color-warm-reregister-test.json`. The disassembled candidate and report are in
the sibling `displayfeature-order-candidate` directory. These diagnostic
artifacts are not dependencies of the extraction helper.
