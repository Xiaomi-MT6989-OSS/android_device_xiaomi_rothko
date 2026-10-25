# Device-local proprietary payload extraction

`extract-device-prebuilts.py` rebuilds the payloads currently stored in this
device tree from a reviewed OS3.0.307.0.WNNCNXM stock dump. It complements the
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

- This supports a stock filesystem dump only. Installed-LineageOS extraction,
  including APK re-signing and optimized JAR handling, still needs its own path
  and clean validation. Do not bypass stock hash checks for an installed dump.
- Matching bytes proves payload reproduction, not licensing, live IMS acceptance,
  hardware audio offload, full vendor regeneration or official device readiness.
- The APK input is the original stock APK; Android.bp retains the existing
  platform-signing integration. This tool does not ship private signing keys.
- This Linux x86-64 workflow uses the reviewed tool from prebuilts/extract-tools.
  Other hosts/tool versions require separately validated output reproduction.
