#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check source/processed fragment restoration and real assemble_vintf replay."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock', type=Path, required=True)
    parser.add_argument('--processed', type=Path, required=True)
    parser.add_argument('--assemble-vintf', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('nfc_fixup', root / 'nfc_manifest_fixup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    stock, processed = args.stock.read_bytes(), args.processed.read_bytes()
    if module.restore(stock) != stock or module.restore(processed) != stock:
        raise RuntimeError('Source restoration differs')
    if module.restore(module.restore(processed)) != stock:
        raise RuntimeError('Restoration is not repeatable')
    for invalid in (b'', processed + b' ', stock[:-1]):
        try:
            module.restore(invalid)
        except ValueError:
            continue
        raise RuntimeError('Unreviewed fragment accepted')
    with tempfile.TemporaryDirectory(prefix='rothko-nfc-roundtrip-') as directory:
        temp = Path(directory)
        relative = 'vendor/xiaomi/rothko/proprietary/vendor/etc/vintf/manifest/nfc-service-tms.xml'
        source = temp / relative
        source.parent.mkdir(parents=True)
        source.write_bytes(module.restore(processed))
        output = temp / 'processed.xml'
        env = os.environ.copy()
        env['VINTF_IGNORE_TARGET_FCM_VERSION'] = 'true'
        subprocess.run([str(args.assemble_vintf.resolve()), '-i', relative, '-o', str(output)],
                       cwd=temp, env=env, check=True, capture_output=True)
        if output.read_bytes() != processed:
            raise RuntimeError('Restored fragment does not reproduce real Soong output')
    print(json.dumps({'stock_preserved': True, 'processed_to_stock_exact': True,
                      'repeated_restore_exact': True, 'unknown_inputs_rejected': 3,
                      'assemble_vintf_roundtrip_exact': True}, indent=2))


if __name__ == '__main__':
    main()
