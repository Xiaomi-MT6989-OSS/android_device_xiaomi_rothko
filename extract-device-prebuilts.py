#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Rebuild device-local payloads from reviewed stock or a product build tree.

Does not modify the checkout, generate Soong modules, build, or access a phone.
Installed-LineageOS extraction is a separate, not yet implemented workflow.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path, PurePosixPath


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_path(value):
    path = PurePosixPath(value)
    if path.is_absolute() or '..' in path.parts or str(path) in ('', '.'):
        raise ValueError(f'Unsafe relative path: {value}')
    return Path(str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-dump', '--input', dest='input_root', type=Path, required=True)
    parser.add_argument('--input-kind', choices=('stock', 'build-tree'), default='stock')
    parser.add_argument('--patchelf', type=Path,
                        help='Reviewed patchelf-0_18 from prebuilts/extract-tools')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--output', type=Path, help='New directory; never overwritten')
    mode.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    manifest = json.loads(Path(__file__).with_name('device-prebuilts.json').read_text())
    if manifest['format_version'] != 1:
        raise ValueError('Unsupported manifest version')
    input_root = args.input_root.resolve(strict=True)
    if not input_root.is_dir():
        raise ValueError('Input must be an unpacked directory')
    if args.input_kind == 'stock' and args.patchelf is None:
        raise ValueError('Stock extraction requires the reviewed patchelf')
    tool = args.patchelf.resolve(strict=True) if args.patchelf else None
    if tool and sha(tool) != manifest['patchelf']['sha256']:
        raise ValueError('Unreviewed patchelf; exact-output replay requires the pinned tool')
    variants = manifest.get('reviewed_build_apk_variants', {})
    destinations = {row['destination']: row for row in manifest['files']}
    for destination, variant in variants.items():
        row = destinations.get(destination)
        if (not row or not destination.endswith('.apk') or
                variant['stock_sha256'] != row['source_sha256'] or
                variant['kind'] != 'platform-signed-apk' or
                not re.fullmatch(r'[0-9a-f]{64}', variant['sha256'])):
            raise ValueError('Invalid reviewed APK variant')
    if args.output:
        output = args.output.resolve()
        if output.exists() or output.is_relative_to(input_root):
            raise ValueError('Output must be new and outside the input')
    seen = set()
    plan = []
    for row in manifest['files']:
        rel = relative_path(row['destination'])
        if rel.parts[0] != 'prebuilts' or str(rel) in seen:
            raise ValueError('Unexpected or duplicate output path')
        seen.add(str(rel))
        source_path = row['source'] if args.input_kind == 'stock' else row['lineage_build_path']
        source = (input_root / relative_path(source_path)).resolve(strict=True)
        if not source.is_relative_to(input_root) or not source.is_file():
            raise ValueError('Source must be a file within the supplied dump')
        if not all(re.fullmatch(r'[0-9a-f]{64}', row[key])
                   for key in ('source_sha256', 'output_sha256')):
            raise ValueError('Invalid pinned digest')
        if row['mode'] not in ('0644', '0755'):
            raise ValueError('Unexpected source file mode')
        for step in row['patchelf_steps']:
            if (len(step) == 2 and step[0] == '--set-soname') or (
                    len(step) == 3 and step[0] == '--replace-needed'):
                continue
            raise ValueError('Unsupported ELF transformation')
        digest = sha(source)
        expected = row['source_sha256'] if args.input_kind == 'stock' else row['output_sha256']
        variant = (args.input_kind == 'build-tree' and digest != expected and
                   digest == variants.get(row['destination'], {}).get('sha256'))
        if digest != expected and not variant:
            raise ValueError(f'Input differs from reviewed payload: {source_path}')
        # Build-tree ELF inputs already contain the reviewed transforms. Do not
        # apply patchelf again; the exact whole-file digest is checked instead.
        steps = row['patchelf_steps'] if args.input_kind == 'stock' else []
        output_digest = digest if variant else row['output_sha256']
        plan.append((row, source, rel, digest, output_digest, steps, variant))
    if args.verify_only:
        print(f'Verified {len(plan)} {args.input_kind} inputs; no outputs written')
        return
    # All inputs are checked before creating an output directory. A failed
    # transformation leaves an incomplete directory, never an in-place update.
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for row, source, rel, input_digest, output_digest, steps, variant in plan:
        target = output / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
        # Also detect source changes between preflight and copy.
        if sha(target) != input_digest:
            raise ValueError(f'Input changed while copying: {row["source"]}')
        for step in steps:
            subprocess.run([str(tool), *step, str(target)], check=True)
        if sha(target) != output_digest:
            raise ValueError(f'Output differs from reviewed payload: {row["destination"]}')
        target.chmod(int(row['mode'], 8))
        results.append({'path': row['destination'], 'sha256': output_digest,
                        'exact_maintained_payload': output_digest == row['output_sha256'],
                        'reviewed_signing_variant': variant})
    (output / 'extraction-report.json').write_text(json.dumps({
        'state': 'payload-extraction-complete', 'firmware': manifest['firmware'],
        'installed_lineage_extraction_verified': False,
        'input_kind': args.input_kind,
        'exact_maintained_payload_count': sum(r['exact_maintained_payload'] for r in results),
        'reviewed_signing_variant_count': sum(r['reviewed_signing_variant'] for r in results),
        'generated_build_modules': False, 'files': results,
    }, indent=2) + '\n')
    print(f'Rebuilt and verified {len(results)} payloads in {output}')


if __name__ == '__main__':
    main()
