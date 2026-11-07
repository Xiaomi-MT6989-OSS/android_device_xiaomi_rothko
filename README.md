# Redmi K70 Ultra / rothko — LineageOS 23.2

Unofficial source-only collaboration tree. Android 16 framework with stock
Android 14/API 34 vendor from OS3.0.307.0.WNNCNXM. This is a development
candidate, not an official or newly runtime-approved release.

The original community history and its signed base
059ac72366d7ca095f2341e635145f611349e0de are preserved. Later history
was filtered in a separate copy to remove payloads/host paths while
retaining authors and committers. Original histories remain unchanged.

The tree alone cannot build the ROM. Use the pinned device/vendor/kernel
revisions, public base manifest and independent platform patch series in
the companion rothko-lineage-integration repository. See its BUILD.md,
KNOWN_ISSUES.md, VALIDATION.md and HISTORY.md.

778 device-local stock payloads are intentionally absent and Git-ignored.
Reconstruct them using extract-device-prebuilts.py and its hash manifest;
see DEVICE_PREBUILTS.md. Vendor/ODM and kernel recipes are separate.
No original binary or raw firmware should be committed here.

The stock camera is the selected app; remaining still-photo effects and
image-quality reports are listed in KNOWN_ISSUES.md. Software A2DP is
the selected default. Preserve existing per-file notices; see LICENSING.md.
LK/preloader/bootloader are outside this product and must not be flashed
by source preparation or added to an OTA.
