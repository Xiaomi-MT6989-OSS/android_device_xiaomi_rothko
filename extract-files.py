#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import blob_fixup, blob_fixups_user_type
from extract_utils.fixups_lib import lib_fixups as default_lib_fixups
from extract_utils.main import ExtractUtils, ExtractUtilsModule

namespace_imports = [
    'device/xiaomi/rothko',
    'hardware/mediatek',
    'hardware/xiaomi',
]

def lib_fixup_odm_suffix(lib, partition, *args, **kwargs):
    return f'{lib}_odm' if partition == 'odm' else None


def lib_fixup_stock_compat(lib, partition, *args, **kwargs):
    return f'{lib}_vendorcompat' if partition in ['vendor', 'odm'] else None


# These stock names are present in both partitions. Match the separate Soong
# modules without changing filenames, SONAMEs or the stock runtime layout.
lib_fixups = {
    **default_lib_fixups,
    ('android.frameworks.sensorservice-V1-ndk', 'libsensorndkbridge',
     'lib_android_keymaster_keymint_utils', 'libkeymint',
     'libkeymint_support', 'libkeymint_remote_prov_support', 'libaudiofoundation',
     'libaudioclient_aidl_conversion', 'libaudio_aidl_conversion_common_ndk',
     'libmediautils_vendor', 'libshmemcompat', 'libshmemutil',
     'av-types-aidl-cpp', 'audioclient-types-aidl-cpp',
     'libstagefright_foundation'):
        lib_fixup_stock_compat,
    ('libmialgo_aio_seg', 'libmialgo_utils', 'libremosaic_wrapper'):
        lib_fixup_odm_suffix,
}

# The stock audio primary imports libalsautils.so. Preserve the original
# device-tree isolation of this stock library from the AOSP implementation.
# Current ELF evidence does not justify the old KeyMint/composer interface
# replacements or speculative shim additions.
blob_fixups: blob_fixups_user_type = {
    'vendor/lib64/hw/audio.primary.mediatek.so': blob_fixup()
        .replace_needed('libalsautils.so', 'libalsautils-v34.so'),
}

module = ExtractUtilsModule(
    'rothko',
    'xiaomi',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    ExtractUtils.device(module).run()
