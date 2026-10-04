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
     'android.hardware.bluetooth.audio-V3-ndk',
     'android.hardware.graphics.composer3-V2-ndk',
     'android.hardware.camera.device-V2-ndk',
     'android.hardware.camera.provider-V2-ndk',
     'lib_android_keymaster_keymint_utils', 'libkeymint',
     'libkeymint_support', 'libkeymint_remote_prov_support', 'libaudiofoundation',
     'libaudioclient_aidl_conversion', 'libaudio_aidl_conversion_common_ndk',
     'libmediautils_vendor', 'libshmemcompat', 'libshmemutil',
     'av-types-aidl-cpp', 'audioclient-types-aidl-cpp',
     'libgralloctypes', 'libui',
     'libcodec2_soft_common', 'libcodec2_vndk', 'libsfplugin_ccodec_utils',
     'libstagefright_aidl_bufferpool2',
     'libcamera2ndk_vendor',
     'libcodec2_hidl@1.0', 'libcodec2_hidl@1.1', 'libcodec2_hidl@1.2',
     'libcodec2_hidl_plugin', 'libstagefright_bufferqueue_helper',
     'libomp',
     'android.hardware.graphics.allocator-V2-ndk'):
        lib_fixup_stock_compat,
    ('libmialgo_aio_seg', 'libmialgo_utils', 'libremosaic_wrapper'):
        lib_fixup_odm_suffix,
}

# The stock audio primary imports libalsautils.so. Preserve the original
# device-tree isolation of this stock library from the AOSP implementation.
# Current ELF evidence does not justify the old KeyMint/composer interface
# replacements or speculative shim additions.
blob_fixups: blob_fixups_user_type = {
    # The API 34 foundation must coexist with the current source vendor library.
    # This list comes from rothko DT_NEEDED, including the extracted VNDK helper.
    (
        'vendor/lib64/hw/sensors.mediatek.V2.0.so',
        'vendor/lib64/libaudio_aidl_conversion_common_ndk.so',
        'vendor/lib64/libaudioclient_aidl_conversion.so',
        'vendor/lib64/libcamera2ndk_vendor.so',
        'vendor/lib64/libcodec2_mtk_c2store.so',
        'vendor/lib64/libcodec2_mtk_vdec.so',
        'vendor/lib64/libcodec2_mtk_venc.so',
        'vendor/lib64/libcodec2_soft_common.so',
        'vendor/lib64/libcodec2_soft_mtk_alacdec.so',
        'vendor/lib64/libcodec2_soft_mtk_imaadpcmdec.so',
        'vendor/lib64/libcodec2_soft_mtk_mp3dec.so',
        'vendor/lib64/libcodec2_soft_mtk_msadpcmdec.so',
        'vendor/lib64/libcodec2_vndk.so',
        'vendor/lib64/libcodec2_vpp_AIMEMC_plugin.so',
        'vendor/lib64/libcodec2_vpp_AISR_plugin.so',
        'vendor/lib64/libcodec2_vpp_fa_plugin.so',
        'vendor/lib64/libcodec2_vpp_frc_plugin.so',
        'vendor/lib64/libcodec2_vpp_qt_plugin.so',
        'vendor/lib64/libcodec2_vpp_rs_plugin.so',
        'vendor/lib64/libmisoundfx_ext.so',
        'vendor/lib64/libsfplugin_ccodec_utils.so',
        'vendor/lib64/libstagefright_bufferqueue_helper.so',
    ): blob_fixup()
        .replace_needed('libstagefright_foundation.so', 'libstagefright_foundation-v34.so'),
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
