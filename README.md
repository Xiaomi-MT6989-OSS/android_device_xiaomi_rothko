# Redmi K70 Ultra / rothko — LineageOS 23.2 bring-up

This local branch preserves the original `xiaomi-mt6989-devs` rothko history
(base `059ac72366d7ca095f2341e635145f611349e0de`). The hardware baseline is
HyperOS OS3.0.307.0.WNNCNXM, including its 6.1.138 Android 14 GKI, DT tables,
partitioned module sets and selected stock vendor/ODM binaries.

Use the independent `device/xiaomi/rothko-kernel` prebuilt repository and the
pinned MTK/Xiaomi dependencies recorded in the project manifests. Extract
blobs with this branch's `extract-files.py` and the corresponding stock dump;
do not substitute an OS2 vendor repository or revive the old KeyMint fixups.

Device-local IMS/audio/modem inputs have a separate pinned recipe:
`extract-device-prebuilts.py` and `DEVICE_PREBUILTS.md`. It complements the
vendor/ODM extractor. See `EXTRACTION_FIXUPS.md` for reviewed fixups and the
factory/product-tree input differences; installed-phone extraction is not yet
validated. Always extract into a new directory before reviewing the results.

The branch is for offline configuration/build validation. First boot,
recovery, encryption, boot-control/misc behavior, module CRCs, thermal/power
configuration and hardware operation have not been validated. A successful
extraction or build does not establish flash readiness.

LK, preloader, bootloader and all aliases are protected. No protected raw
firmware belongs in this tree, vendor or OTA. `create_pl_dev` and generic MTK
boot-control/UFS BootLU switching must remain excluded. Follow the project's
`SAFETY.md` and staged implementation plan before any device mutation.
