# Stock AOD brightness route

The stock MIUIAod controller subscribes to wake-up sensor type 33171029
(`xiaomi.sensor.aod`). Events 3 and 5 choose low brightness; event 4 chooses
high. The DDC calibration derives these targets from 5 and 60 nits, using the
stock 2..500-nit linear segment. These are not upstream NIGHT/LOW/HIGH/SUN
buckets. Unknown events retain brightness; proximity still controls AOD pause.

SystemUI selects that sensor. DDC supplies the event mapping and low default.
A16 `DisplayDeviceConfig.loadDozeBrightness` converts raw backlight values to
framework brightness. The route compares actual and default brightness after
`BrightnessSynchronizer.brightnessFloatToInt`, matching stock quantization.
Screen-off dimming below the low target still selects the panel low mode.

## Ordered application

The optional framework `DeviceDozeBrightnessController` is selected by a device
resource and defaults to absent elsewhere. Its constructor performs no IO.
LocalDisplayAdapter applies it in the existing power-work Runnable outside the
global display lock, using that Runnable's target state. A power-state change
also applies the route if the numerical brightness has not changed.

Only the verified built-in physical display is eligible. DOZE/DOZE_SUSPEND skip
ordinary backlight and synchronously request stock DisplayFeature 25, cookie
255: HIGH=1 or LOW=2. On exit, normal backlight is applied first, followed by
NORMAL=0. The original HAL and driver own the DDIC commands and mode cache.
FodEngine retains ownership of local high brightness and display readiness.

The device-private IRothkoDozeBrightness transport is separate from read-only
histogram sampling. Only system_server can call it. It uses the existing sole
OEM command worker; it is not gated on a later SurfaceFlinger scene report.
There is no timer, unsolicited replay, direct ioctl, or additional OEM writer.
A failed reply still requires cleanup, since it cannot prove no panel write
occurred. Failed cleanup remains pending until the next normal brightness
application; successful cleanup is not repeatedly sent.

The synchronous request waits for command completion. Abandoning a timed-out
write could let it run after a newer power transition. Consequently a stalled
OEM Binder call can delay display power work; this is a runtime validation risk,
not covered by host tests. Service absence/failure is logged, and is not treated
as successful panel configuration. Optional service registration failure does
not crash the existing colour controller.

## Validation and limits

Passed: physical-nits/event mapping checks, AAPT2 resource compilation, fresh
Java/NDK AIDL generation, native controller/service syntax checks, and compilation
of the actual LocalDisplayAdapter plus device controller against existing
platform API artifacts. Host tests cover mode quantization, route selection,
AOD/pulse exit/re-entry, uncertain failure cleanup, and foreign-display exclusion.
Host fixtures do not validate Binder scheduling or panel behavior.

The initial route-only checks above preceded deployment. Subsequent unified
images also include the opted-in SystemUI draw/present protection. User testing
confirmed persistent AOD across repeated idle intervals and room-light changes,
plus fully-off/AOD/power-button fingerprint paths, wrong-finger rejection and
no stuck local illumination. These combined results do not isolate the effect
of this backlight route from the draw/present change.

Still pending: complete proximity/cover and minute-update tracing, service
failure/restart injection, rapid-transition stress and idle-power measurement.
Preserve logs on black screen or stuck illumination. Physical 30 Hz operation
remains unverified; the observed developer refresh-rate overlay still reports
60 Hz. Neither this route nor the draw protection overrides the refresh counter,
forces ON, or permanently disables suspend. See the project review
`source-release/ADAPTATION_REVIEW_04_DISPLAY_AOD.zh-CN.md` for evidence boundaries.
