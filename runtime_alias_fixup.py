# SPDX-License-Identifier: Apache-2.0
"""Apply reviewed runtime aliases while preserving ELF section order."""
from extract_utils.tools import patchelf_version_path_map
from extract_utils.utils import run_cmd


def blob_fixup_runtime_alias(ctx, file, file_path, soname, replacements, **kwargs):
    # Sorting sections in the combined SONAME/NEEDED rewrite changes symbol
    # section indexes in the OEM Bluetooth 2.2 binding with patchelf 0.18.
    args = [patchelf_version_path_map['0_18'], '--no-sort']
    if soname:
        args += ['--set-soname', soname]
    for original, replacement in replacements.items():
        args += ['--replace-needed', original, replacement]
    run_cmd(args + [file_path])
