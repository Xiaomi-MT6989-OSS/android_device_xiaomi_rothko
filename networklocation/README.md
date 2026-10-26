# Factory network location and geocoding

This integrates the unmodified OS3.0.307.0.WNNCNXM `com.xiaomi.metoknlp`
system provider. It is a source candidate, not a claim of successful positioning.
It does not install Google services or change the GNSS provider.

## Why this provider

The factory APK advertises network-provider v3/v2/legacy and geocode actions.
Its network implementation extends `com.android.location.provider.LocationProviderBase`.
The current platform bridge wraps present `ProviderRequest` objects for the same
legacy callback, so no Android 14 Location Binder is copied into system_server.
The factory geocoder's forward/reverse request and callback interfaces match
the current Android 16 interfaces. Four embedded arm64 JNI libraries depend
only on ordinary NDK libraries; no MIUI framework jar or separate location
daemon was identified as a mandatory initialization dependency.

Miui build reflection handles missing classes, and the provisioning check also
accepts the standard `device_provisioned` setting. This integration does not
fabricate Xiaomi consent, provisioned values, locations or server responses.
The factory backend includes AMap/Baidu implementation code and network
endpoints; their availability, authorization and actual results require runtime
testing. This is not a new offline location database.

## Signature, permissions and framework access

- Extract the original APK using `EXTRACT_ONLY;FILEGROUP=MetokNLP-stock`.
  Its Soong app definition and explicit Java uses-library remain device-owned.
- Keep the original Xiaomi v2/v3 signature and exact APK bytes. The JNI members
  are uncompressed and 4 KiB zip-aligned. The factory DEX is compressed, so this
  one module uses the upstream `skip_preprocessed_apk_checks` exception. This
  does not disable Android package signature enforcement. Signature verification,
  alignment and JNI storage were separately checked with existing SDK tools;
  the upstream preprocessed checker confirms this exception is necessary.
- Install the same nine privileged permissions from the stock product policy,
  in the same product partition as the app. Do not grant the requested but
  cross-signature `INTERACT_ACROSS_USERS_FULL` permission.
- Include the stable `com.android.location.provider` Java shared library and
  declare it in `uses_libs`. The platform-generated library declaration must be
  present in the built image.
- Scope the standard hidden-API system-package allowlist to this one preloaded
  package. On HyperOS it is platform-signed; on this ROM the preserved Xiaomi
  signature is not the platform key. Required geocoder Stub/callback methods
  are blocked APIs, even though same-name AIDL classes appear inside the APK:
  the boot classloader selects the framework classes first. Do not change global
  hidden-API enforcement, system signatures or permission protection levels.
- Add this package alongside the AOSP fused package to the native default
  location-permission policy. Keep provider overlays enabled and leave network
  and geocoder package selection unpinned, so native system-provider arbitration
  remains available. Google-provider coexistence still requires separate testing.

The location master setting and the factory provider's own privacy behavior
remain authoritative. No always-on location client is added.

## Unified-build acceptance

Check the product APK's exact hash/signature, permissions, library XML and
sysconfig. After a normal boot verify package scan, network/geocoder binding
and provider registration without exceptions or denials. Then request an actual
non-GNSS fix and forward/reverse geocode, verify coordinate/provider attribution,
failure handling, master-switch off/on, Wi-Fi/cellular availability, user switch,
and coexistence with an independently installed Google provider where applicable.
Do not publish raw coordinates, radio identifiers or location histories.

Static analysis and successful compilation do not establish backend availability,
correct coordinates, all-region coverage or complete multi-user behavior.
