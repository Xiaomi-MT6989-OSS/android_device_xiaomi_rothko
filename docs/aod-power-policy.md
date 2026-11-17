# AOD power policy candidate — 2026-11-17

Status: device-overlay candidate; aapt2 resource compilation and diff checks
passed. ROM build, flash and device acceptance remain pending. This does not
claim to fix the delayed OEM AOD brightness input or the reported brief flash.

## Observed behavior and owning layer

On `24.0-20261115-UNOFFICIAL-rothko`, an unplugged light-change observation
recorded DOZE_SUSPEND but no growth of elapsedRealtime minus uptime. After
reconnection, PowerManager reported Dozing, no CPU/display suspend blocker,
native autosuspend disabled, interactive enabled, and both power-decoupling
resources false. The power snapshot was taken after USB reconnection; it is
not a complete trace of the unplugged power state.

The device selects the standard DozeService but inherits the two legacy false
defaults. In the examined LineageOS 24 framework, PowerManagerService's
onDisplayStateChange enables autosuspend in that configuration only when
allOff is true. DisplayManagerService does not classify DOZE_SUSPEND as OFF.
This configuration therefore prevents the normal policy-based AOD suspend
path even when the relevant blockers are released.

## Chosen change and provenance

Enable config_powerDecoupleAutoSuspendModeFromDisplay and
config_powerDecoupleInteractiveModeFromDisplay in the device framework overlay.
Existing PowerManager suspend blockers continue to protect CPU work and draw
windows; interactive mode follows display policy. No new wake locks, timers,
sensor flags, firmware patches or shared-platform changes are introduced.

Examined framework base: `6935a1c405573b277bc1aedceb47780688ef5ea7`:
core/res/res/values/config.xml, services/core/java/com/android/server/power/
PowerManagerService.java and services/core/java/com/android/server/display/
DisplayManagerService.java. The upstream resource comments recommend the
decoupled mode with a doze component, subject to hardware support.

The selected lineage-libperfmgr Power HAL handles INTERACTIVE through named
hints. This device's powerhint.json has no INTERACTIVE action, and its local
extension handles only DOUBLE_TAP_TO_WAKE. The change does not add a touch
write through that HAL path. Hardware suspend/wake behavior still needs testing.

Prior art: [Duchamp framework overlay](https://github.com/mt6897-devs/device_xiaomi_duchamp/blob/991bea83800167bc20f3ba846d43d099a3137601/overlay/DuchampFrameworksOverlay/res/values/config.xml),
Git blob `d7b515064584fd537a45f7bd880c88c49f4d1326`, enables both resources.
It is a same-generation MTK integration lead, not proof of Rothko acceptance.
The examined Rothko HyperOS 3 framework-res contains false for both resources;
its separate OEM power integration is not being copied or assumed equivalent.

Private capture/provenance: project root research/display/aod/
LINEAGE_ADAPTATION_TARGET_20261117.zh-CN.md and work/aod-brightness-20261117/
light-observer/unplugged-run-03.json. Observer files were removed from the phone.

## Validation and remaining acceptance

The complete modified config.xml compiles with the existing host aapt2;
XML checks confirm each resource occurs once with true; git diff --check passes.
These checks do not prove runtime overlay selection or suspend capability.

After the next appropriate build, verify both effective resources, autosuspend
enabled in stable AOD with blockers released, and actual elapsed/uptime growth
without USB. Check minute/content/brightness updates, repeated AOD entry/exit,
power-button wake, double-tap and FOD including screen-off operation. Verify
failure/exit paths release existing draw leases. Then evaluate ordinary ALS
delivery under real suspend before selecting it as a brightness input.

The brightness bug remains open: delayed AOD events originate before the
framework consumer. A prompt raw ALS stream while the AP is awake cannot
establish the independent wakeup contract needed for a production replacement.
