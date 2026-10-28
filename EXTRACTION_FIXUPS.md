# Reviewed extraction fixups

The current stock baseline is OS3.0.307.0.WNNCNXM. Device extraction rules and
extract-utils must be kept at the same reviewed source revisions.

The source preparation audit exercised all 138 entries selected by
`ExtractUtilsModule.should_fixup_file`, including explicit blob callbacks and
automatic FIX_SONAME handling. Each direct stock input was copied to scratch,
fixed through the actual `fixup_module_file` API and compared with the maintained
vendor file. A separate scratch copy of every maintained file was fixed again.
All 138 stock conversions and all 138 repeat operations matched the existing
vendor bytes after the fixes below. This is not a full extract-files run.

After integrating these fixes into the later WPA3-capable device tree, the
same scratch audit covered 146 selected entries: all stock conversions and
all repeat operations matched the current vendor files. This comparison does
not replace full extraction, generated-module, build, or runtime validation.

- The vibrator callback accepts the framework's `tmp_dir` argument and only
  transforms the pinned stock binary or preserves its exact reviewed output.
- `HpImpedance_AudioParam.xml` has a missing closing tag in stock. The repair
  accepts only the pinned original and repaired hashes, checks valid output XML,
  and never inserts another closing tag when processing an installed result.
- Allocator V2 namespace isolation continues to reject namespace collisions.
  Re-entry is allowed only for an exact reviewed hash, keyed by destination in
  `allocator-v2-isolation-hashes.json`. Presence of the new namespace alone is
  not sufficient. The pinned files were independently reproduced from stock.
- The Novatek rc fixup normalizes CRLF to LF before the existing reviewed
  disabled-service and DAC adjustments, reproducing the maintained file exactly.

From this device directory, with STOCK_DUMP and VENDOR_TREE set to your paths:

```sh
python3 tests/extraction_callbacks_test.py \
  --stock-dump "$STOCK_DUMP" --vendor "$VENDOR_TREE"
python3 -O tests/extraction_callbacks_test.py \
  --stock-dump "$STOCK_DUMP" --vendor "$VENDOR_TREE"
```

These checks require local reviewed payloads; no binary fixture is committed.
They exercise the callback calling convention, stock/repeated outputs and
unknown-input rejection. A modified already-isolated allocator ELF must be
rejected without changing the scratch file, including under Python `-O`.

The broader scratch audit lives in the integration preparation tools, rather
than adding a diagnostic service to Android. It does not change source inputs,
build a system or access a phone. It checks direct filesystem inputs only;
source fallback selection, extraction cleanup, installed APK signing, optimized
JARs, full payload coverage, image metadata and final build/runtime validation
remain separate requirements.


## Factory and product-tree input paths

The MetokNLP filegroup keeps the signed factory APK unchanged. Its extraction
source is the factory product path and its destination is the actual system_ext
installation path; extract-utils tries the destination then the source. This
allows the same recipe to read either an unpacked factory dump or this product
build without a second APK copy or a general fallback that accepts unknown data.

The factory hostapd fragment is already retained byte-for-byte as
`configs/wifi/android.hardware.wifi.hostapd.xml` and merged by BoardConfig.
The unused extraction filegroup is excluded: it was not installed separately
and could not be recovered at that path from a Lineage product directory.
The hostapd daemon, libraries, runtime declaration and security capabilities
are not changed by this source-input cleanup.

These path rules have been checked using the integration reproduction tool
against a prepared product tree. That tool separately maps recovery/root;
product-tree checks do not claim extraction from an installed phone or every
signature/optimization variant.
