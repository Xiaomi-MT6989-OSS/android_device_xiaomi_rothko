#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile original worker methods against host-only input/transport fixtures."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True,
                        help='AOSP/LineageOS root containing system/libbase')
    parser.add_argument('--output', type=Path, required=True,
                        help='New host-output directory; must not already exist')
    parser.add_argument('--cxx', default='g++', help='Host C++17 compiler')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    native = here.parents[1]
    source_root = args.source_root.resolve()
    output = args.output.resolve()
    base_include = source_root / 'system/libbase/include'
    if not (base_include / 'android-base/unique_fd.h').is_file():
        parser.error('source root lacks libbase unique_fd.h')
    output.mkdir(parents=True, exist_ok=False)
    path = native / 'UdfpsSubHal.cpp'
    source = path.read_text()

    def part(start, end):
        begin = source.index(start)
        return source[begin:source.index(end, begin)]

    worker = part('class UdfpsSubHal final', '  Return<void> getSensorsList_2_1(')
    worker += part('  Return<Result> initialize(', '  Return<Result> setOperationMode(')
    worker += part(' private:\n  void kick()',
                   '}  // namespace android::hardware::sensors::V2_1::subhal::rothko')
    (output / 'production-class.inc').write_text(worker)
    mode = part('bool setTouchFodIcon(bool enabled)', "// This device's stock Goodix")
    (output / 'production-mode.inc').write_text(mode.split('\n}  // namespace')[0])
    constant = re.search(r'constexpr int kTouchFodIconEnable = \d+;', source)
    if constant is None:
        raise RuntimeError('touch mode constant not found')
    (output / 'production-constants.inc').write_text(constant.group() + '\n')
    inputs = [path, native / 'FodInput.h', here / 'harness.cpp', Path(__file__).resolve()]
    record = {
        'scope': 'Original selected worker methods, real host thread/pipe/eventfd/poll; '
                 'HIDL, discovery/ioctl and touchfeature fixtures. No phone or Binder transport.',
        'inputs': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                   for p in inputs],
    }
    compiler = subprocess.run([
        args.cxx, '-std=c++17', '-O0', '-g0', '-pthread', '-I' + str(native),
        '-I' + str(base_include), '-I' + str(output), str(here / 'harness.cpp'),
        '-o', str(output / 'worker-test'),
    ], capture_output=True, text=True, timeout=45)
    (output / 'build.log').write_text(compiler.stdout + compiler.stderr)
    record['compile_exit_code'] = compiler.returncode
    status = compiler.returncode
    if status == 0:
        test = subprocess.run([str(output / 'worker-test')], capture_output=True,
                              text=True, timeout=45)
        (output / 'run.log').write_text(test.stdout + test.stderr)
        print(test.stdout, end='')
        print(test.stderr, end='')
        record['test_exit_code'] = test.returncode
        status = test.returncode
    else:
        print(compiler.stderr, end='')
    (output / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    return status


if __name__ == '__main__':
    raise SystemExit(main())
