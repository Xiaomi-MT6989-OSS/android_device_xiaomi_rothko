# SPDX-License-Identifier: Apache-2.0
"""Restore the reviewed stock source form of the Soong-processed TMS fragment."""
import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET

STOCK_SHA256 = 'ac47e202140fa164dd2c38aad52a87c8c45472247da5369e1cb47831895543b3'
PROCESSED_SHA256 = '4df8c5b7cefc98448c1632dfd4d9f0065368b9355e185c9f82885f86a53a32b7'


def restore(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest == STOCK_SHA256:
        return data
    if digest != PROCESSED_SHA256:
        raise ValueError('Unreviewed TMS NFC fragment')
    # These values come from the pinned original fragment. Soong's current
    # assemble_vintf output uses schema 9.0 and omits the default AIDL version 1.
    root = ET.fromstring(data)
    root.set('version', '1.0')
    hal = root.find('hal')
    version = ET.Element('version')
    version.text = '1'
    hal.insert(1, version)
    ET.indent(root, space='    ')
    output = (ET.tostring(root, encoding='unicode') + '\n\n').replace('\n', '\r\n').encode('utf-8')
    if hashlib.sha256(output).hexdigest() != STOCK_SHA256:
        raise ValueError('Unexpected TMS NFC source restoration')
    return output


def blob_fixup_nfc_manifest(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)
    path.write_bytes(restore(path.read_bytes()))
