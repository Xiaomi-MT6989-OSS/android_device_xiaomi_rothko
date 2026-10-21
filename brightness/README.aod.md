# Stock AOD brightness integration status

The stock MIUIAod brightness controller subscribes to wake-up sensor type
33171029 (`xiaomi.sensor.aod`). Values 3 and 5 choose its low brightness mode;
value 4 chooses high. The stock framework calibrates those targets from 5 and
60 nits using the panel's DDC map. They are not the upstream NIGHT/LOW/HIGH/SUN
sensor buckets.

SystemUI now names the actual sensor. The display configuration provides the
corresponding backlight values and a low default. Unknown/non-brightness events
retain the current brightness. Existing proximity handling remains responsible
for pausing AOD. The calibration uses the stock linear segment from 2 to 500
nits; A16 `DisplayDeviceConfig.loadDozeBrightness` converts these backlight
values to framework brightness.

This configuration is only one part of the adaptation. Stock LocalDisplayAdapter
uses a separate doze brightness path in DOZE/DOZE_SUSPEND, through DisplayFeature
25 to the original panel driver. It restores normal backlight before resetting
the applied doze mode to 0. That route is not implemented by these resources.
In particular, a framework brightness of 0 can still map to a positive ordinary
backlight; this does not prove the panel's dedicated doze mode is configured.

Do not work around a black AOD by keeping the panel ON, disabling suspend, forcing
ordinary refresh rates, or periodically writing brightness. The driver uses its
cached doze mode for FOD parameters, so any later route must preserve original
FodEngine ownership of local high brightness and display readiness.

Validation so far: physical-nits interpolation checks, OEM event/scrim mapping,
AAPT2 resource compilation and whitespace checks. No ROM build or device
validation of these resource changes has been performed. Dedicated route and
FOD/pulse/rapid power-transition integration remain pending.
