# rothko stock camera candidate

This port uses OS3.0.307.0's unmodified, originally signed MiuiCamera APK.
The device product includes `camera/camera.mk` for the next unified build.
Aperture remains installed; this does not select the default camera.

The next candidate installs as a system_ext privileged app with its original
signature. The stock firmware places it in product/priv-app; the former
system_ext/app installation lost its privileged identity. system_ext preserves
the existing system-side JNI placement. The single-package
hidden API exemption matches the stock firmware's sysconfig. The resource wrapper
uses hidden ResourcesManager APIs before opening a camera. No global hidden API
setting or blanket permission grants are selected.

Unlike product apps, an unchanged system_ext system app is treated as bundled by
the current LoadedApk implementation. This is a device-side installation choice
for the original system-side JNI clients; it avoids publishing private camera
libraries to all apps or changing NativeLoader policy. Actual loading remains a
device-test requirement.

The target includes the original system-side JNI/interface libraries and the
original empty libhidltransport compatibility DSO. Module names use a private
suffix; installed filenames and SONAMEs retain their original names. The vendor
copies remain untouched. The system_ext ISP AIDL binding imports no symbols
(including weak imports) from its graphics common V6 NEEDED entry; extraction
removes only that unused dependency to avoid mixing V6 with the A16 libui V7
graph. Its code, read-only data, dynamic symbols and init/fini arrays remain
unchanged. This does not replace V6 by V7 or disable ELF checks. Original and
candidate digests are recorded in stock-jni-inventory.json. Static symbol checks
do not establish complete object ABI or runtime namespace compatibility.

The optional HyperOS Java dependencies are not a validated dexpreopt context.
`enforce_uses_libs: false` does not provide those libraries or fix JNI resolution.
The APK is preprocessed to retain its bytes/signature, and dexpreopt is disabled.
The stock miui-cameraopt JAR is not selected: a DEX type-table comparison found no
external references to its 297 defined classes in this APK. This does not exclude
reflection or service-side use; add it only when an actual dependency is found.

The November 6 unified image boots, enumerates nine HAL cameras and launches the
stock app, but full-resolution photography failed. It runs as untrusted_app,
cannot find the protected BGService, and lacks SYSTEM_CAMERA. A short Permissive
comparison delivered the BGService image notification, but the app then logged
`mockCamera unprepared` / `can not obtain camera ids`. After about 40 seconds it
finalized a preview-only fallback with `fullImageReceived=0`; that is not a
successful full-resolution photo. Aperture still generated a JPEG in the same
test. Idle JpegNode semaphore stacks alone do not establish an encoder deadlock.

`privapp-permissions-rothko-camera.xml` selects eleven permissions that this exact
APK requests, that appear in stock's com.android.camera allowlist, and that the
installed Android 16 permission set defines as privileged. It is installed in
system_ext alongside the app. Unknown Xiaomi permissions and unrelated packages
are not copied. Vendor policy restores stock priv_app BGService lookup and
two-way Binder calls to its actual server, mtk_hal_camera. This restores stock
integration requirements; full-image delivery, the private receiver camera,
all lenses, image quality, HDR photo/video and other app features still require
a new build and device tests. No new build has been started for this change.

Local resource scope: the existing stock ODM filter plugins contain literal paths
to 60 local PNG tables. Forty are present in this firmware dump but absent in the
current output, so the opt-in candidate restores those byte-identical files with
ODM prebuilt_etc modules. No cloud materials, download behavior or telemetry
configuration is enabled by this addition. Actual filter selection is untested.
