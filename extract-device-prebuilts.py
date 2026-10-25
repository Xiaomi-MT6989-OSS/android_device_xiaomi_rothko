#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Rebuild reviewed device-local payloads in a new directory from a stock dump.

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
    parser.add_argument('--stock-dump', type=Path, required=True)
    parser.add_argument('--patchelf', type=Path, required=True,
                        help='Reviewed patchelf-0_18 from prebuilts/extract-tools')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--output', type=Path, help='New directory; never overwritten')
    mode.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    manifest = json.loads(Path(__file__).with_name('device-prebuilts.json').read_text())
    if manifest['format_version'] != 1:
        raise ValueError('Unsupported manifest version')
    stock = args.stock_dump.resolve(strict=True)
    tool = args.patchelf.resolve(strict=True)
    if sha(tool) != manifest['patchelf']['sha256']:
        raise ValueError('Unreviewed patchelf; exact-output replay requires the pinned tool')
    if args.output:
        output = args.output.resolve()
        if output.exists() or output.is_relative_to(stock):
            raise ValueError('Output must be new and outside the stock input')
    seen = set()
    plan = []
    for row in manifest['files']:
        rel = relative_path(row['destination'])
        if rel.parts[0] != 'prebuilts' or str(rel) in seen:
            raise ValueError('Unexpected or duplicate output path')
        seen.add(str(rel))
        source = (stock / relative_path(row['source'])).resolve(strict=True)
        if not source.is_relative_to(stock) or not source.is_file():
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
        if sha(source) != row['source_sha256']:
            raise ValueError(f'Stock input differs from reviewed firmware: {row["source"]}')
        plan.append((row, source, rel))
    if args.verify_only:
        print(f'Verified {len(plan)} stock input hashes and the pinned host tool; no outputs written')
        return
    # All stock inputs are checked before creating an output directory. A failed
    # transformation leaves an incomplete directory, never an in-place update.
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for row, source, rel in plan:
        target = output / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
        # Also detect source changes between preflight and copy.
        if sha(target) != row['source_sha256']:
            raise ValueError(f'Input changed while copying: {row["source"]}')
        for step in row['patchelf_steps']:
            subprocess.run([str(tool), *step, str(target)], check=True)
        if sha(target) != row['output_sha256']:
            raise ValueError(f'Output differs from reviewed payload: {row["destination"]}')
        target.chmod(int(row['mode'], 8))
        results.append({'path': row['destination'], 'sha256': row['output_sha256']})
    (output / 'extraction-report.json').write_text(json.dumps({
        'state': 'payload-extraction-complete', 'firmware': manifest['firmware'],
        'installed_lineage_extraction_verified': False,
        'generated_build_modules': False, 'files': results,
    }, indent=2) + '\n')
    print(f'Rebuilt and verified {len(results)} payloads in {output}')


if __name__ == '__main__':
    main()
