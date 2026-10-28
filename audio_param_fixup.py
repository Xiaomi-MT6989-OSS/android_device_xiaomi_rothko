# SPDX-License-Identifier: Apache-2.0
"""Repair the reviewed stock impedance XML once, preserving parameter values."""
import hashlib
from pathlib import Path
import re
import xml.etree.ElementTree as ET

STOCK_SHA256 = 'a66ebe8d7291da3ce9a64a1f80f415cefb3820d181650ed85f41074543937073'
ADAPTED_SHA256 = '75433a21284cd4f9e95ce1bb9ed0b059c4151c6b63a6d62537a9cce2acd36641'


def repair(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest == ADAPTED_SHA256:
        return data
    if digest != STOCK_SHA256:
        raise ValueError('Unreviewed impedance parameter XML')
    output = re.sub(r'(?m)^(\t</ParamUnitPool>)$',
                    r'\t\t</ParamUnit>\n\1', data.decode('utf-8')).encode('utf-8')
    if hashlib.sha256(output).hexdigest() != ADAPTED_SHA256:
        raise ValueError('Unexpected impedance XML repair result')
    ET.fromstring(output)
    return output


def blob_fixup_hp_impedance(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)
    path.write_bytes(repair(path.read_bytes()))
