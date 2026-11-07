# SPDX-License-Identifier: Apache-2.0
"""Make the stock camera read the China beauty configuration.

The MediaTek camera HAL (libcameraplatforminfoxiaomi, MetadataUpdateBeautyData)
publishes the beauty scene table from /odm/etc/camera/beauty/beauty_config_gl.json
on this device regardless of ro.miui.region. The China build of MiuiCamera looks
up a flat table (RearCapture/FrontCapture/...) that only beauty_config_cn.json
provides, so with the unmodified global file every scene lookup fails and the
beauty panel lists only "none".

Replacing the global table with the China table restores the presets. The China
file is extracted immediately before this one (proprietary-files.txt order), so
it is available as a sibling.
"""

from pathlib import Path


def blob_fixup_beauty_gl_to_cn(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)
    china = path.with_name('beauty_config_cn.json')
    if china.is_file():
        path.write_bytes(china.read_bytes())
