# SPDX-License-Identifier: Apache-2.0
"""Adapt only the reviewed stock IVibrator registration suffix to AOSP default."""
import hashlib
from pathlib import Path

STOCK_SHA256 = '6d9979f21764fc69ef28f7f8368ce07965b94168961bcb817faa6beeb4908f96'
ADAPTED_SHA256 = 'a082557886f3446b7ffc5f700adc1743c5e45400752660ded1ea2890b8a80966'
OFFSET = 0x7e9b
OLD = b'/vibratorfeature\0'
NEW = b'/default\0' + b'\0' * (len(OLD) - len(b'/default\0'))


def adapt_instance(data):
    digest = hashlib.sha256(data).hexdigest()
    # An installed LineageOS dump already carries the reviewed transformation.
    # Accept only that exact result, never an arbitrary file with the same suffix.
    if digest == ADAPTED_SHA256:
        return data
    if digest != STOCK_SHA256:
        raise ValueError('Unreviewed vibrator firmware')
    if data[OFFSET:OFFSET + len(OLD)] != OLD or data.count(OLD) != 1:
        raise ValueError('Unexpected vibrator registration suffix')
    adapted = data[:OFFSET] + NEW + data[OFFSET + len(OLD):]
    if hashlib.sha256(adapted).hexdigest() != ADAPTED_SHA256:
        raise ValueError('Unexpected vibrator fixup result')
    return adapted


def blob_fixup_vibrator_instance(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)
    path.write_bytes(adapt_instance(path.read_bytes()))
