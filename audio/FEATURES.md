# Standard Spatializer and AAudio MMAP

These are device integration candidates, not runtime acceptance results. The
source is rothko OS3.0.307.0.WNNCNXM, not another MediaTek device.

## AAudio

Restore `aaudio.mmap_policy=2` and `aaudio.mmap_exclusive_policy=2` (stock AUTO).
On the current HIDL <= 7.1 path, AudioFlinger::getMmapPolicyInfos() reads these
properties. AudioPolicyManager::updateMmapPolicyInfos() expands the default
entry using actual MMAP_NOIRQ ports/routes, and audioserver can then start
media.aaudio. No AIDL query shim or upstream override is needed.

Keep the stock formats, sample rates, input profiles, and device routes. AUTO
allows fallback rather than forcing applications into unsupported hardware
streams. In particular, a device not routed to mmap_no_irq_out must not be
advertised as MMAP-capable just because the system-wide policy is AUTO. The
stock speaker route does not include mmap_no_irq_out; input and supported
wired-output MMAP must be tested independently.

## Spatializer

The AudioService gate is `ro.audio.spatializer_enabled`, not the removed
`audio.deero.audio.spatializer_enabled` key. Enabling that gate requires the
complete native chain:

- Register stock `mispatializer` and `spatializer` effects with their original
  UUIDs and factory library names from stock audio_effects.xml.
- Extract both factory libraries and their native closure: 7 libraries total.
  The original standard Spatializer type UUID and implementation UUID pair
  are present in both factory descriptors.
- Extract the original `hrtf5c.bin` headphone model and both stock
  `dax-*-spatializer.xml` parameter tables. Retain the already selected original
  ODM speaker model. Do not invent the alternate `_tt` model absent in stock.
- Isolate the Android 14 stagefright foundation dependency of libdlbpreg,
  libswspatializer and libswspatializer_ext using the existing v34 library.
  Executable effect code, licenses, parameter models and signatures are not
  patched.
- Use the exact primary SPATIALIZER output profile from the stock
  spatializer-capable policy. Extend only its actual stock sinks in the two
  selected policy variants; all ordinary and MMAP routes stay unchanged.
- Keep software A2DP as the default. A dedicated SPATIALIZER stereo PCM port
  in the existing bluetooth module lets SpatializerThread render to that
  software transport, without claiming the broken primary hardware-offload
  route works. The normal A2DP output port and routes remain available.

This does not install Dolby UI, a DMS daemon, license provisioning, or an
always-on music postprocessor. The standard framework owns effect sessions,
levels and device availability. Native factory initialization, negotiated
capabilities, model loading, real rendering and optional head tracking still
need acceptance on the unified build. A descriptor or a visible switch alone
is not proof of audible spatialization. Lack of a head-tracking headset is not
reason to advertise synthetic tracking.

## Offline checks and acceptance

The change was checked without lunch, Soong, Ninja, flashing, or phone setting
changes. Checks cover stock bindings/descriptors, model provenance, route and
MMAP preservation, XML parsing, the 7 new libraries' dependency closure and
strong symbols against the available product libraries. This is not Android
linker execution or an ABI/firmware compatibility guarantee.

Build vendorimage and its matching vbmeta_vendor as part of the unified build.
On the phone, check:

1. Audio/Spatializer dumps: native effect is loaded, supported levels/modes
   are real, Settings reports availability only for supported connected devices.
2. Supported multichannel media, with spatialization off/on, using wired/USB
   and Bluetooth headphones. Verify audible output, volume, route switches,
   ordinary stereo, speaker playback, calls/recording and Bluetooth continuity.
3. AAudio low-latency shared/exclusive output and input on actually routed
   devices: MMAP selection, buffer creation, start/stop, timestamp continuity,
   disconnect and repeated reopen. Verify unsupported routes fall back.
4. No effect factory/HAL deaths, parameter failures, model-read failures,
   permission denials or leaked output-stage sessions after route changes.

Do not mark these features accepted until these checks pass. Hardware A2DP
offload remains separately deferred.
