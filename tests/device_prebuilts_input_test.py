#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify real input sets and preflight rejection without proprietary fixtures."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-dump', type=Path, required=True)
    parser.add_argument('--build-root', type=Path, required=True)
    parser.add_argument('--patchelf', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    script = root / 'extract-device-prebuilts.py'
    first = json.loads((root / 'device-prebuilts.json').read_text())['files'][0]
    if not first['destination'].endswith('.apk'):
        raise RuntimeError('Reviewed first input changed; update rejection fixture')
    results = []
    with tempfile.TemporaryDirectory(prefix='rothko-device-input-test-') as directory:
        temp = Path(directory)
        corrupt = temp / 'corrupt'
        entry = corrupt / first['lineage_build_path']
        entry.parent.mkdir(parents=True)
        entry.write_bytes(b'unknown APK contents')
        escape = temp / 'escape'
        linked = escape / first['lineage_build_path']
        linked.parent.mkdir(parents=True)
        linked.symlink_to((args.build_root / first['lineage_build_path']).resolve())
        existing = temp / 'existing'
        existing.mkdir()
        sentinel = existing / 'sentinel'
        sentinel.write_bytes(b'preserve')
        for optimized in (False, True):
            prefix = [sys.executable] + (['-O'] if optimized else []) + [str(script)]
            tests = [
                ('stock', ['--stock-dump', str(args.stock_dump.resolve()), '--patchelf',
                           str(args.patchelf.resolve()), '--verify-only'], None),
                ('build-tree', ['--input', str(args.build_root.resolve()), '--input-kind',
                                'build-tree', '--verify-only'], None),
                ('unknown-apk', ['--input', str(corrupt), '--input-kind', 'build-tree',
                                 '--output', str(temp / ('new-' + str(optimized)))],
                 'Input differs from reviewed payload'),
                ('symlink-escape', ['--input', str(escape), '--input-kind', 'build-tree',
                                    '--verify-only'], 'Source must be a file within'),
                ('existing-output', ['--input', str(args.build_root.resolve()), '--input-kind',
                                     'build-tree', '--output', str(existing)],
                 'Output must be new'),
                ('stock-requires-tool', ['--input', str(args.stock_dump.resolve()),
                                         '--verify-only'], 'requires the reviewed patchelf'),
            ]
            for name, arguments, error in tests:
                completed = subprocess.run(prefix + arguments, capture_output=True, text=True)
                if error is None:
                    if completed.returncode != 0 or 'Verified 778 ' not in completed.stdout:
                        raise RuntimeError(name + ' validation failed: ' + completed.stderr)
                elif completed.returncode == 0 or error not in completed.stderr:
                    raise RuntimeError(name + ' did not reject for the expected reason')
                if (temp / ('new-' + str(optimized))).exists() or sentinel.read_bytes() != b'preserve':
                    raise RuntimeError('Preflight rejection wrote to output')
                results.append({'case': name, 'optimized': optimized, 'passed': True})
    print(json.dumps({'scope': 'real input verification and rejection; no phone or build',
                      'tests': results, 'count': len(results)}, indent=2))


if __name__ == '__main__':
    main()
