# Rothko display color controller

This directory is device-private source. The provider is selected through
`SOONG_CONFIG_surfaceflinger_display_color_lib` in BoardConfig and the
controller is installed through `PRODUCT_PACKAGES`. Earlier unified images have
passed user tests for profile changes and persistence across reboot. Those
observations do not establish panel colorimetry, every rapid-switch sequence,
or service-failure recovery. Later source-review changes require a new build
and regression; they are not retroactively covered by the installed image.

Layout:

- `provider/` — the SurfaceFlinger-side `DisplayColorExtension`, linked into
  `libcompositionengine` as a whole static library.
- `controller/` — the separate service that owns the only channel to the stock
  Xiaomi DisplayFeature service. `StockDisplayFeature.h` declares the ABI of the
  byte-identical stock V2 NDK binding; all proxy/stub code and frozen metadata
  still come from that prebuilt library.
- `service.cpp` — registers `vendor.lineage.livedisplay.IDisplayModes/default`
  and the private controller interface.

`vendor.xiaomi.rothko.display.color-ndk` is an unstable, platform-private NDK
Binder transport between SurfaceFlinger and the controller process.
It is not a reconstruction of the Xiaomi frozen interface. The OEM V2 proxy
must only be linked into the controller, never into SurfaceFlinger.

The service instance is
`vendor.xiaomi.rothko.display.color.IRothkoDisplayColor/default`.
`setListener(sfSession, connection, callback)` must return promptly without OEM
IPC. SF allocates a new connection for each registration attempt; the controller
must replace/link the listener, discard earlier pending requests and validate
the session/connection on every report. `DisplayState.sequence` increases over
the SF lifetime and reports are coalesced, not an event log.

Controller policy requests use positive, strictly increasing request IDs within
each connection. Retransmission of the same ID and identical contents is
idempotent; changing the contents of the same ID is a protocol violation.
`intent=-1` clears the override. Only OEM SDR user intents 256, 257 and 258 are
accepted otherwise. Each reapply, including return from HDR or screen power
transitions, needs a new request ID. The worker assigns an increasing SF policy
revision independently. A bounded 32-slot revision/request/connection history
correlates real HWC results; expired or different-connection results report
`policyRequestId=0` and cannot acknowledge a current request.

The provider selects only the first connected internal physical display.
External and virtual outputs are ignored. Hook methods only access atomic cache
or try-lock/coalesce fixed-size local state. They never wait, perform Binder or
disk access, or invoke SF invalidate inline. A dedicated worker performs service
lookup, Binder registration/death handling, policy callbacks and asynchronous
invalidate. Private callback messages have a finite 16-slot queue. All cached
policy fields are read in one bounded atomic sequence check; concurrent writes
return the upstream/default policy for that frame instead of spinning.

An observed service death disables cached policy. An ordinary controller
reconnect replays the latest content/power continuously observed by SF, clears
old profile/request correlation and uses a new connection. Lost local updates,
queue overflow or invalid protocol mark all content/profile/power unknown and
clear policy. Content/profile can recover on fresh composition; actual power
can only recover on a real HWC power observation. No cached power is promoted
to known after such a loss. `synchronizedState` describes complete observations;
it is **not** a bootstrap precondition. The controller may replay a user mode
after fresh content and accepted On power, then wait for the new revision's
actual profile/HWC result to acknowledge it.

Accepted power changes invalidate content/profile. Failed power attempts also
clear the override and leave the actual effective power unconfirmed. Disabled,
unmanaged and HDR content suspend the cached user override. The companion
platform guard checks the current HDR dataspace before applying any cached user
intent, including the first HDR frame before this worker sees its content
report. Existing WCG and upstream native/legacy/client tone-map decisions are
preserved. The controller must retain confirmed user/default separately from
this temporary suspension.

The worker's fixed 20 ms polling interval is local and does not scan files.
State reports are sent only when dirty; this interval is not a Binder heartbeat.
If death linking fails, an otherwise idle dead controller is detected only when
a later report fails. Policy has no independent expiry in this interface.
Private Binder methods must remain prompt; a malicious or blocked synchronous
controller registration can stall this worker but cannot make a composition
hook wait. Production controller authorization, init, service contexts and
SELinux belong to the controller integration and are not supplied by this
provider alone.

Source/evidence contracts:

- `${PROJECT_ROOT}/research/2026-10-16/four-feature-repair-01/color-service-contract-review.zh-CN.md`
- `${PROJECT_ROOT}/research/2026-10-15/rea-display/ADAPTATION.zh-CN.md`
- `frameworks/native/services/surfaceflinger/CompositionEngine/include/compositionengine/DisplayColorExtension.h`

The original V2 protocol has no request token or hardware LUT acknowledgement.
These private request/revision identifiers must not be described as OEM ACKs.
HWC software acceptance is distinct from panel appearance. The current missing-
echo fallback accepts a mode after a 400 ms post-return observation window.
This is compatibility behaviour, not proof of mode application: the stock server
returns Binder OK even on missing dispatch and does not propagate HAL integer
errors. The fallback still needs protocol redesign backed by original behaviour.
The overall apply/caller deadlines continue to cover synchronous OEM call time. Unavailable current state is returned
as unavailable, rather than replaced by the saved boot preference; the SDK/UI
support explicit retry. The default getter remains separately available.

Each listener registration starts an internal generation, even if its external
session/connection identifiers are reused. Accepted user requests retain that
generation and connection. Replacement completes the displaced caller with an
error; a stale timeout cannot clear the replacement listener's policy. Admission
ownership is tied to the request object, and completion is idempotent. A request
queued during automatic restoration survives worker iterations. Worker updates
to current/ready are generation-checked around OEM IPC. Already-issued OEM calls
cannot be cancelled; this does not provide transactional cancellation of panel
state or filesystem persistence. Host regression coverage and its limits are
documented in `tests/README.md`.

Remaining validation includes failed/late callbacks, rapid transitions, backend
death or a live-but-stalled Binder peer, persistence failures, HDR/power return,
and calibrated color measurements. The device-specific 170m-as-sRGB compositor
override also needs separate necessity and camera-content testing.

Failed composition/power notification batches are not cached as delivered. They
keep the controller unavailable and disable sampling until a successful replay,
with retries bounded to the existing one-second backend recovery cadence. A
successful batch restores sampling eligibility only for the still-current scene
revision and listener generation. This is IPC failure recovery, not brightness
filtering or proof that a physical LUT has applied.
