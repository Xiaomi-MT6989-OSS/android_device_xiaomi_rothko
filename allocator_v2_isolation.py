# SPDX-License-Identifier: Apache-2.0
"""Isolate rothko stock allocator V2 C++ names, preserving ELF layout.

Only the fully qualified mangled AIDL allocator namespace in .dynstr changes.
The equal-length replacement has the same GNU hash, so symbol indexes, GNU
hash tables and Android packed relocations remain valid without rebuilding.
The Binder descriptor, executable code and std::allocator names are untouched.
"""
from pathlib import Path
import hashlib
import json
import struct

OLD_NAMESPACE = b'4aidl7android8hardware8graphics9allocator'
NEW_NAMESPACE = b'4aidl7android8hardware8graphics9rkccndb1y'


def gnu_hash(name):
    value = 5381
    for byte in name:
        value = (value * 33 + byte) & 0xffffffff
    return value


def isolate_symbols(file_path, already_isolated_sha256=None):
    path = Path(file_path)
    original = path.read_bytes()
    if original[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64')
    header = struct.unpack_from('<HHIQQQIHHHHHH', original, 16)
    if header[1] != 183 or header[10] != 64:
        raise ValueError('Expected AArch64 standard section headers')
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
        raise ValueError('Unexpected dynamic tables')
    table = section_bytes(strings)
    if NEW_NAMESPACE in table:
        # Only a byte-exact reviewed output may skip the transformation. An
        # arbitrary ELF containing this namespace is still a collision/error.
        if (already_isolated_sha256 is not None
                and hashlib.sha256(original).hexdigest() == already_isolated_sha256
                and OLD_NAMESPACE not in table):
            return 0
        raise ValueError('Already isolated or namespace collision')
    changed = 0
    old_names, new_names = set(), set()
    for offset in range(symbols[4], symbols[4] + symbols[5], 24):
        name_offset = struct.unpack_from('<I', original, offset)[0]
        if name_offset >= len(table):
            raise ValueError('Symbol name outside string table')
        name = table[name_offset:].split(b'\0', 1)[0]
        replacement = name.replace(OLD_NAMESPACE, NEW_NAMESPACE)
        if gnu_hash(name) != gnu_hash(replacement):
            raise ValueError('GNU hash changed')
        if name != replacement:
            if not name.startswith(b'_Z'):
                raise ValueError('Expected mangled C++ symbol')
            changed += 1
            old_names.add(name)
        new_names.add(replacement)
    if old_names & new_names:
        raise ValueError('Incomplete namespace isolation')
    # All substring occurrences must belong to complete mangled dynamic names.
    # String suffix sharing can produce fewer occurrences than symbol entries.
    for item in table.split(b'\0'):
        if OLD_NAMESPACE in item and not item.startswith(b'_Z'):
            raise ValueError('Namespace occurs outside mangled symbol string')
    start, size = strings[4:6]
    output = original[:start] + table.replace(OLD_NAMESPACE, NEW_NAMESPACE) + original[start + size:]
    if len(output) != len(original):
        raise ValueError('File size changed')
    path.write_bytes(output)
    return changed


def blob_fixup_allocator_v2_isolation(ctx, file, file_path, *args, **kwargs):
    reviewed = json.loads(Path(__file__).with_name('allocator-v2-isolation-hashes.json').read_text())
    isolate_symbols(file_path, reviewed.get(file.dst))
