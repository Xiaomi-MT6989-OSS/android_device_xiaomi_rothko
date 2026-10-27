#!/usr/bin/env python3
# Copyright (C) 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
"""Extract one unchanged, reviewed arm64 JNI member from the factory-signed APK."""
import hashlib
from pathlib import Path
import sys
from zipfile import ZipFile

APK_SHA256 = "846c84ea6468d3e3e6e71496ec3ac0fb91760ced00427e7c062e5da01384e8d3"
MEMBER_SHA256 = {'libapssdk.so': '5b5a21495316fbd20ee04b5a0b515b2912cf3ef98e594a703f4a6ca2b84190c2', 'libmiui_apssdk.so': '4f8d80089a3bf70b7c0b8fea7e6647e2b740736b210fa04cc5a94d60a18a2918', 'libnlocSDK7d.so': 'c11cf4729458175212cfeca5557092c66844f3d6e332258373645a70915b3e48', 'libmievent.so': 'd404076edc917984b6af87c5013c0e428f5acd9cf3fa18211e1124937ee24d3f'}


def extract(apk, name, output):
    if name not in MEMBER_SHA256:
        raise ValueError("Unreviewed JNI member")
    if hashlib.sha256(apk.read_bytes()).hexdigest() != APK_SHA256:
        raise ValueError("Factory APK identity changed; review the new release first")
    member = "lib/arm64-v8a/" + name
    with ZipFile(apk) as archive:
        if archive.namelist().count(member) != 1:
            raise ValueError("Missing or duplicate JNI member")
        data = archive.read(member)
    if (data[:6] != b"\x7fELF\x02\x01" or int.from_bytes(data[18:20], "little") != 183
            or hashlib.sha256(data).hexdigest() != MEMBER_SHA256[name]):
        raise ValueError("JNI bytes or architecture differ from the reviewed factory payload")
    output.write_bytes(data)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("usage: extract-metok-jni APK MEMBER OUTPUT")
    extract(Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3]))
