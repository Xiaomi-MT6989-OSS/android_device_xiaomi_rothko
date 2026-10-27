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
  in the same system_ext partition as the app. Do not grant the requested but
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

Check the installed APK's exact hash/signature, permissions, library XML and
sysconfig. After a normal boot verify package scan, network/geocoder binding
and provider registration without exceptions or denials. Then request an actual
non-GNSS fix and forward/reverse geocode, verify coordinate/provider attribution,
failure handling, master-switch off/on, Wi-Fi/cellular availability, user switch,
and coexistence with an independently installed Google provider where applicable.
Do not publish raw coordinates, radio identifiers or location histories.

Static analysis and successful compilation do not establish backend availability,
correct coordinates, all-region coverage or complete multi-user behavior.

## JNI install layout (2026-10-27 correction)

The signed APK requests `extractNativeLibs=true`. PackageAbiHelperImpl explicitly
skips native extraction for a non-updated system app, and app_import does not
install JNI. An uncompressed/aligned ZIP member alone is therefore insufficient:
the first build had an empty nativeLibraryDir and the AMap callback timed out.

A controlled same-version update of the exact factory APK let PackageManager
extract its libraries. libapssdk then appeared in the process maps; a foreground
network-only current-location request returned a fresh fix in 681ms and a 1s
continuous request returned in 8ms. Before this change, valid foreground requests
had timed out at 30s / 20s. The update was removed with keep-data and the product
APK restored after the comparison. No global policy was disabled.

The permanent build extracts four reviewed arm64 ELF members with an isolated
host tool, checks APK/member SHA256 and ELF architecture, and installs unchanged
native prebuilt modules under /system_ext/lib64/metoknlp. Standard install_symlink
modules expose them under the app's own lib/arm64 directory, as used by native
JNI installation on system apps. They are not added to a public-library list.
The app requires all four links. The factory signature, compressed DEX, default
location/privacy controls and NDK dependencies remain intact; no Android platform
source patch is needed. Verify this exact preinstalled layout on the next build;
the updated-APK comparison does not by itself accept the final image layout.

The original shell-only test and the first locked-screen Activity run were
background-throttled to 30 minutes and cannot be used as standalone failure
proof. Later unlocked foreground runs explicitly reached the provider at 0s/1s
and reproduced the defect. The AppOps package/UID mismatch and ordinary Wi-Fi
scan throttling remain separate observations; they were still present during the
successful JNI comparison, so they are not grounds to weaken permission checks.

## Partition correction after dependency-graph review

The original filegroup still extracts the factory product APK. Installation is
now in system_ext: the APK, both permission/sysconfig declarations, native
prebuilts and private app symlinks move together. No vendor input changes.
Soong CanUseSdk() excludes vendor/product native image variants; therefore a
product JNI prebuilt cannot obtain the ordinary SDK libstdc++ dependency merely
by setting sdk_version. The first graph check rejected that declaration.
The system_ext extension uses the normal core/SDK variant and native namespace,
without adding product_available to bionic, duplicating STL implementations,
skipping ELF checks or rewriting DT_NEEDED. Public-library lists stay unchanged.

After an incremental partition relocation remove only the three obsolete
module-owned staged product files before building images, so a stale product
APK cannot shadow the new system_ext provider. Do not clean the build tree.
The final image must have exactly one package and the permission XML must share
its partition. Recheck the package source path and native library mappings on
the next device acceptance; the installed phone still has the previous product
image until that flash occurs.

## Runtime acceptance follow-up (2026-10-27)

The system_ext preinstalled APK and native mapping have now been verified on
the device. Initial foreground requests timed out despite the JNI being loaded;
the same APK update comparison also timed out in that session. Therefore JNI
absence is a confirmed packaging defect, not established as the sole failure
cause. A single location-master off/on control restored a code-5 SDK request
(result 0, 633ms; app callback 675ms). A later normal reboot returned network
locations without toggling settings. The latter SDK path reused a similar-Wi-Fi
cache, so callback timestamp age alone is not proof of a fresh backend request.
Forward and reverse geocoding each returned one result for fixed public test
inputs. All diagnostic packages were removed and the original enabled location
setting retained. Initial timeout reproducibility and cold-start robustness are
still open; no boot-time toggle workaround is shipped and release is not approved.
