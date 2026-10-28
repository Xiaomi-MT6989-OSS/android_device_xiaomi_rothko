#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fixture integration checks; requires reviewed local stock/vendor files.

No proprietary fixtures are included, and no input file is modified. Also run
under python -O to ensure rejection checks do not depend on assertions.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def load(root, name):
    spec = importlib.util.spec_from_file_location(name, root / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rejected(call, reason):
    try:
        call()
    except ValueError:
        return
    raise RuntimeError(reason)


def text_offset(data):
    header = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, header[5] + i * header[10])
                for i in range(header[11])]
    names = sections[header[12]]
    table = data[names[4]:names[4] + names[5]]
    return next(s[4] for s in sections if table[s[0]:].split(b'\0', 1)[0] == b'.text')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-dump', type=Path, required=True)
    parser.add_argument('--vendor', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    vibrator, xml, allocator = (load(root, name) for name in (
        'vibrator_instance_fixup', 'audio_param_fixup', 'allocator_v2_isolation'))
    vib_path = 'odm/bin/hw/vendor.xiaomi.hardware.vibratorfeature.service'
    xml_path = 'vendor/etc/audio_param/HpImpedance_AudioParam.xml'
    allocator_path = 'vendor/bin/hw/mt6989/android.hardware.graphics.allocator-V2-service-mediatek.mt6989'
    with tempfile.TemporaryDirectory(prefix='rothko-extraction-callbacks-') as directory:
        temp = Path(directory) / 'payload'
        # Exercise the real callback ABI, not only the transformation helper.
        for rel, callback in ((vib_path, vibrator.blob_fixup_vibrator_instance),
                              (xml_path, xml.blob_fixup_hp_impedance)):
            source = (args.stock_dump / rel).read_bytes()
            expected = (args.vendor / 'proprietary' / rel).read_bytes()
            temp.write_bytes(source)
            callback(None, SimpleNamespace(dst=rel), str(temp), tmp_dir=directory)
            require(temp.read_bytes() == expected, 'Stock callback output differs')
            callback(None, SimpleNamespace(dst=rel), str(temp), tmp_dir=directory)
            require(temp.read_bytes() == expected, 'Repeated callback changed data')
        for helper, expected in ((vibrator.adapt_instance,
                                 (args.vendor / 'proprietary' / vib_path).read_bytes()),
                                (xml.repair, (args.vendor / 'proprietary' / xml_path).read_bytes())):
            for invalid in (b'', expected[:-1], bytes([expected[0] ^ 1]) + expected[1:]):
                rejected(lambda: helper(invalid), 'Unreviewed helper input accepted')
        expected = (args.vendor / 'proprietary' / allocator_path).read_bytes()
        temp.write_bytes(expected)
        allocator.blob_fixup_allocator_v2_isolation(
            None, SimpleNamespace(dst=allocator_path), str(temp), tmp_dir=directory)
        require(temp.read_bytes() == expected, 'Reviewed isolated ELF changed')
        altered = bytearray(expected)
        altered[text_offset(expected)] ^= 1
        temp.write_bytes(altered)
        rejected(lambda: allocator.blob_fixup_allocator_v2_isolation(
            None, SimpleNamespace(dst=allocator_path), str(temp), tmp_dir=directory),
            'Modified already-isolated ELF accepted')
        require(temp.read_bytes() == bytes(altered), 'Rejected ELF was modified')
    print(json.dumps({'stock_callbacks_exact': 2, 'callbacks_repeatable': 2,
                      'unknown_vibrator_xml_inputs_rejected': 6,
                      'reviewed_allocator_reentry_exact': True,
                      'modified_isolated_elf_rejected_without_write': True}, indent=2))


if __name__ == '__main__':
    main()
