# SPDX-License-Identifier: Apache-2.0
"""Adapt only the reviewed stock IVibrator registration suffix to AOSP default."""
import hashlib
from pathlib import Path

STOCK_SHA256 = '6d9979f21764fc69ef28f7f8368ce07965b94168961bcb817faa6beeb4908f96'
OFFSET = 0x7e9b
OLD = b'/vibratorfeature\0'
NEW = b'/default\0' + b'\0' * (len(OLD) - len(b'/default\0'))


def adapt_instance(data):
    assert hashlib.sha256(data).hexdigest() == STOCK_SHA256, 'Unreviewed vibrator firmware'
    assert data[OFFSET:OFFSET + len(OLD)] == OLD
    assert data.count(OLD) == 1, 'Ambiguous registration suffix'
    return data[:OFFSET] + NEW + data[OFFSET + len(OLD):]


def blob_fixup_vibrator_instance(ctx, file, file_path):
    path = Path(file_path)
    path.write_bytes(adapt_instance(path.read_bytes()))
