# SPDX-License-Identifier: Apache-2.0
"""Isolate stock TinyXML2 without changing Android packed relocation indexes.

The eight-character namespace below has the same 32-bit GNU hash as tinyxml2.
Substitution therefore preserves the hash of every mangled symbol, including
PQ helper signatures containing TinyXML2 parameter types. Only .dynstr bytes
change; symbol indexes, versions, hash tables and executable bytes stay put.
The stock ELFs use GNU hash only. Reject SysV hash rather than leave it stale.
"""

from pathlib import Path
import struct

OLD_NAMESPACE = b'8tinyxml2'
NEW_NAMESPACE = b'8ryt1ioez'


def gnu_hash(name):
    value = 5381
    for byte in name:
        value = (value * 33 + byte) & 0xffffffff
    return value


def isolate_symbols(file_path):
    original = Path(file_path).read_bytes()
    if original[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64')
    header = struct.unpack_from('<HHIQQQIHHHHHH', original, 16)
    if header[1] != 183 or header[10] != 64:
        raise ValueError('Expected ARM64 with standard section headers')
    sections = [struct.unpack_from('<IIQQQQIIQQ', original,
                                   header[5] + i * header[10])
                for i in range(header[11])]

    def section_bytes(section):
        start, size = section[4:6]
        if start + size > len(original):
            raise ValueError('Section outside file')
        return original[start:start + size]

    names = section_bytes(sections[header[12]])
    named = {names[s[0]:].split(b'\0', 1)[0]: s for s in sections}
    if b'.hash' in named or b'.gnu.hash' not in named:
        raise ValueError('Isolation requires GNU hash only')
    symbols = named[b'.dynsym']
    strings = sections[symbols[6]]
    if strings != named[b'.dynstr'] or symbols[9] != 24:
        raise ValueError('Unexpected dynamic symbol/string table')
    table = section_bytes(strings)
    changed = 0
    old_names = set()
    new_names = set()
    for offset in range(symbols[4], symbols[4] + symbols[5], 24):
        name_offset = struct.unpack_from('<I', original, offset)[0]
        if name_offset >= len(table):
            raise ValueError('Symbol name outside string table')
        name = table[name_offset:].split(b'\0', 1)[0]
        replacement = name.replace(OLD_NAMESPACE, NEW_NAMESPACE)
        if gnu_hash(name) != gnu_hash(replacement):
            raise ValueError('GNU hash changed')
        if name != replacement:
            changed += 1
            old_names.add(name)
        new_names.add(replacement)
    if old_names & new_names:
        raise ValueError('Incomplete namespace isolation')
    start, size = strings[4:6]
    output = original[:start] + table.replace(OLD_NAMESPACE, NEW_NAMESPACE) + original[start + size:]
    if len(output) != len(original):
        raise ValueError('String table size changed')
    Path(file_path).write_bytes(output)
    return changed


def blob_fixup_tinyxml2_isolation(ctx, file, file_path, *args, **kwargs):
    isolate_symbols(file_path)
