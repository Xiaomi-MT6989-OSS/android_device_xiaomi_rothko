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

### Targeted stock initialization review

REA review of the shipped factory and extension confirms that their complete
`.text` sections are byte-identical (SHA-256
`8b46ae0bbe00d6089b7bf25364f39a032795eb0f4bc8df4054ee173b63811fdc`).
The review keeps three scopes separate:

- Factory selection: the stock Xiaomi wrapper's initial backend value is 1
  (Xiaomi), from its initialized data section, so its initial descriptor uses
  `bb8c97a8-cc6c-11ec-9d64-0242ac120002`. The direct Dolby factory uses
  `ccd4cf09-a79d-46c2-9aae-06a1698d6c8f`. AOSP loads library vector entries
  onto a reverse-order list, enumerates library effects in that order, and
  Spatializer::create chooses the first matching type. Both stock and this
  configuration place the direct Dolby library after the Xiaomi wrapper, so
  the direct Dolby implementation is selected first. Equal type UUIDs do not
  mean equal implementation UUIDs and do not themselves cause rejection.
- Initialization/capabilities: EffectSpatializer::init initializes its DSP
  object and a local EndpointParamCache; the latter returns success without
  a DMS call. SpatializerEffectContext::init uses the engine return value;
  when its DMS pointer is null, it keeps the local profile path and continues.
  Mandatory getParam IDs 0/2/4/5 read local/static level, tracking, channel and
  mode data, with size checks. They do not query DMS. The getParam jump-table
  branches were checked against complete LLVM disassembly because Hopper's
  pseudocode omits some switch targets.
- DMS integration: EffectContext::connectDolbyService uses tryGetService and
  returns false when the service is absent. The reviewed initialization and
  capability paths do not turn that into a mandatory init failure. The
  optional libaudiohalutils dlopen also has a null-handle path that logs and
  continues. This does not establish successful DSP rendering, license state,
  all later control paths or head tracking on the new system.

Relevant REA Evidence IDs: factory init
`ev_f9dc2c58417937794e50f081f92ed5b1ff8850c4cd1c20d5de8b3484d052e73c`,
capabilities
`ev_de006894c6152707360dc4912aeee346cdee48074607e6aebce551d97311fcfd`,
Spatializer context init
`ev_8bf74d9dc9bfa5c06f0d5e2cd734b0cf2f8559eaf847fa0a0c0945eed6d242bd`,
DMS connection
`ev_13159004a89d973b456034651c5e81d72695471fa22a2488f0d47c1429a9ab73`,
Xiaomi descriptor
`ev_799b1afe529e611f498940f003d076c1ad7ac9a639d236a5bc1a547eee96f77e`.
These are static observations, not captured device execution. Native sessions
were exported and closed. No effect bytecode, licensing, private backend
selection property or upstream factory selection was modified after review.

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
