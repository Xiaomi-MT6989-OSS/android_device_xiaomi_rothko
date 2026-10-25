# Device-local proprietary payload extraction

`extract-device-prebuilts.py` rebuilds the payloads currently stored in this
device tree from a reviewed OS3.0.307.0.WNNCNXM stock dump or the reviewed
product build tree. It complements the
vendor/ODM extractor; it does not replace that extractor or generate build rules.

The pinned `device-prebuilts.json` maps 778 files:

- 20 IMS APK/JAR/native files from stock system_ext and system.
- 756 APN/PLMN/CA resources from stock vendor/etc/md, including one empty file.
- 2 Bluetooth audio libraries from stock vendor, with reviewed ELF transforms.

Android.bp, ims.mk, permissions, init files, provenance records and modem
SHA256SUMS remain source-controlled integration metadata. The script does not
rewrite them. It never reads modem NV, a phone, calibration or identity data.

From the device directory, with STOCK_DUMP and PAYLOAD_OUTPUT set to your paths:

```sh
source_root="$(cd ../../.. && pwd)"
python3 extract-device-prebuilts.py \
  --stock-dump "$STOCK_DUMP" \
  --patchelf "$source_root/prebuilts/extract-tools/linux-x86/bin/patchelf-0_18" \
  --output "$PAYLOAD_OUTPUT"
```

PAYLOAD_OUTPUT must not exist and must be outside STOCK_DUMP. The result contains
`prebuilts/` and an extraction report. Review before transferring payloads to a
working tree; no automatic installation, checkout overwrite, regeneration or
build is performed. A transformation failure leaves an incomplete new output
directory; there is no success report until every output hash matches.
`--verify-only` replaces `--output` for input/tool hash checks without writes.

The input and output SHA-256 of every file are pinned. The host patchelf binary
is pinned as well because its version and invocation order affect binary layout.
Output executable bits follow the tracked source files rather than the local
working tree umask. Input corruption, unknown firmware or an existing output
directory is rejected. No proprietary binary is embedded in the JSON or script.

For the Bluetooth primary module, the AIDL dependency is renamed first and the
legacy session dependency second. For the legacy bridge, replay first sets its
private SONAME, then renames the generic AIDL session to the historical private
name, then to the stock MTK session. The intermediate step reproduces the exact
already-validated binary layout; it does not leave an active dependency on the
intermediate provider. Keep the invocations separate. Both outputs must match
their existing whole-file hashes, not merely expose the same DT_NEEDED entries.

Limits:

- The build-tree path below is not installed-phone extraction. ADB collection,
  other signing keys, stripped/optimized JAR variants and packaged image inputs
  remain unverified. Unknown APKs and other content are rejected.
- Matching bytes proves payload reproduction, not licensing, live IMS acceptance,
  hardware audio offload, full vendor regeneration or official device readiness.
- The APK input is the original stock APK; Android.bp retains the existing
  platform-signing integration. This tool does not ship private signing keys.
- This Linux x86-64 workflow uses the reviewed tool from prebuilts/extract-tools.
  Other hosts/tool versions require separately validated output reproduction.

## Reviewed product build tree

Every payload has a `lineage_build_path` derived from its existing module
installation location or copy rule and checked against the current product
output. In particular libhidltransport is installed in system_ext, not its
stock system source partition. Both IMS JARs retain the exact maintained bytes.
The two audio ELF files already contain the reviewed transforms and are copied
without invoking patchelf again. Whole-file hashes must still match.

```sh
python3 extract-device-prebuilts.py \
  --input "$PRODUCT_BUILD_TREE" --input-kind build-tree \
  --output "$PAYLOAD_OUTPUT"
```

For the current build, 777 outputs match the original maintained payloads.
The IMS APK is a separately reviewed, exact platform-signed variant whose only
ZIP entry content difference is META-INF/CERT.RSA. The stock and built inputs
were re-signed with the tracked public AOSP test key and both reproduced the
current build APK exactly. The variant digest and its original stock digest
are pinned in the manifest. Other APK bytes/signing variants are not accepted.
The script does not sign APKs or carry any private keys. Existing platform
signing integration remains responsible for signing on the next build.

Unknown inputs, symlinks outside the supplied root and existing output
directories are rejected before output creation. `--verify-only` checks all
inputs without writing. The report distinguishes exact maintained outputs from
the explicitly accepted signing variant; it never calls this stock-byte exact.

`tests/device_prebuilts_input_test.py` exercises both real input sets, unknown APK
rejection, path escape rejection, existing output preservation and the required
stock transform tool, with and without Python optimization. A source-preparation
product-tree check is not a clean system rebuild or runtime validation.
