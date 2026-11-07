# SPDX-License-Identifier: Apache-2.0
"""Order the pinned stock DisplayFeature HAL initialization before publication.

Offline ELF transformation only; these virtual addresses are research locations
in this exact artifact, never process-memory offsets used by the product.
Keep the existing null-object guard, stack frame, init function, Binder ABI,
error paths and unwind information. Move the existing init call to the guarded
block entry and reload the object afterward for the existing asBinder call.
"""
import hashlib
from pathlib import Path
import struct

STOCK_SHA256 = 'ebf52e8480c95f317ff9f4bd3ba564fad43e7f5b4c3d65c68e862d03c7dcde76'
PATCHED_SHA256 = 'fd2e1578475e27c14d59ed83738bdee7b362d616aed302fbc994dc9ece183f8d'
MAIN_START, MAIN_END = 0x46F0, 0x4924
ENTRY, OLD_INIT, JOIN = 0x47DC, 0x4890, 0x4898
INITIALIZE, PUBLISH = 0x34D0, 0x4E00


def _signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _relative(word, pc):
    """Return kind, target, immediate width, shift for AArch64 PC relatives."""
    if word & 0x7C000000 == 0x14000000:  # B / BL
        return ('branch', pc + _signed(word & 0x3FFFFFF, 26) * 4, 26, 0)
    if (word & 0xFF000010 == 0x54000000 or  # B.cond
            word & 0x7E000000 == 0x34000000 or  # CBZ / CBNZ
            word & 0x3B000000 == 0x18000000):  # LDR literal
        kind = 'literal' if word & 0x3B000000 == 0x18000000 else 'branch'
        return (kind, pc + _signed((word >> 5) & 0x7FFFF, 19) * 4, 19, 5)
    if word & 0x7E000000 == 0x36000000:  # TBZ / TBNZ
        return ('branch', pc + _signed((word >> 5) & 0x3FFF, 14) * 4, 14, 5)
    if word & 0x1F000000 == 0x10000000:  # ADR / ADRP
        imm = _signed(((word >> 5) & 0x7FFFF) << 2 | (word >> 29) & 3, 21)
        page = bool(word & 0x80000000)
        return ('page' if page else 'address',
                (pc & ~4095) + imm * 4096 if page else pc + imm, 21, 0)
    return None


def _encode(word, pc, target, relative):
    kind, _, bits, shift = relative
    delta = target - (pc & ~4095 if kind == 'page' else pc)
    scale = 4096 if kind == 'page' else (1 if kind == 'address' else 4)
    if delta % scale:
        raise ValueError('Unaligned relative target')
    value = delta // scale
    if not -(1 << (bits - 1)) <= value < (1 << (bits - 1)):
        raise ValueError('Relative target outside encoding range')
    value &= (1 << bits) - 1
    if kind in ('address', 'page'):
        return (word & ~((3 << 29) | (0x7FFFF << 5)) |
                (value & 3) << 29 | (value >> 2) << 5)
    mask = ((1 << bits) - 1) << shift
    return word & ~mask | value << shift


def _target(target):
    # An incoming edge at the guarded entry must execute the moved init.
    if target == ENTRY:
        return ENTRY
    if ENTRY < target < OLD_INIT:
        return target + 8
    if target == OLD_INIT:
        return JOIN  # Initialization already happened before publication.
    if OLD_INIT < target < JOIN:
        raise ValueError('Unexpected edge into the removed init sequence')
    return target


def _file_offset(data, address):
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected little-endian AArch64 ELF64')
    phoff = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    if size != 56:
        raise ValueError('Unexpected program header size')
    for i in range(count):
        kind, flags, offset, va, _, length, _, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + i * size)
        if kind == 1 and flags & 1 and va <= address < va + length:
            return offset + address - va
    raise ValueError('Instruction outside executable file-backed LOAD segment')


def transform(data):
    # Derive the one reviewed output from the original; accept only exact input
    # or exact output, never a partial patch or a different firmware revision.
    digest = hashlib.sha256(data).hexdigest()
    if digest == PATCHED_SHA256:
        return data
    if digest != STOCK_SHA256:
        raise ValueError('Unreviewed DisplayFeature service; refuse transformation')
    original = {pc: struct.unpack_from('<I', data, _file_offset(data, pc))[0]
                for pc in range(MAIN_START, MAIN_END, 4)}
    if original[OLD_INIT] != 0xF85E03A0:  # ldur x0, [x29, #-0x20]
        raise ValueError('Object reload changed')
    init_relative = _relative(original[OLD_INIT + 4], OLD_INIT + 4)
    if not init_relative or init_relative[1] != INITIALIZE:
        raise ValueError('Initialization call changed')
    if original[ENTRY - 4] != 0xB40004E0:  # guarded non-null object in x0
        raise ValueError('Null-object guard changed')
    result = bytearray(data)
    for old_pc, word in original.items():
        if OLD_INIT <= old_pc < JOIN:
            continue
        new_pc = old_pc + 8 if ENTRY <= old_pc < OLD_INIT else old_pc
        relative = _relative(word, old_pc)
        if relative:
            kind, target, _, _ = relative
            target = _target(target) if kind == 'branch' else target
            word = _encode(word, new_pc, target, relative)
            actual = _relative(word, new_pc)
            if not actual or actual[1] != target:
                raise ValueError('Relative instruction round-trip failed')
        struct.pack_into('<I', result, _file_offset(data, new_pc), word)
    # x0 is already the checked service object on both incoming entry paths.
    # Init may clobber caller-saved registers, so reload x0 before asBinder.
    call = _encode(original[OLD_INIT + 4], ENTRY, INITIALIZE, init_relative)
    struct.pack_into('<II', result, _file_offset(data, ENTRY), call, original[OLD_INIT])
    start, end = _file_offset(data, MAIN_START), _file_offset(data, MAIN_END - 4) + 4
    if result[:start] != data[:start] or result[end:] != data[end:]:
        raise ValueError('Transformation escaped main')
    output = bytes(result)
    if hashlib.sha256(output).hexdigest() != PATCHED_SHA256:
        raise ValueError('Unexpected transformed DisplayFeature service hash')
    return output


def blob_fixup_displayfeature_startup(ctx, file, file_path, *args, **kwargs):
    path = Path(file_path)
    path.write_bytes(transform(path.read_bytes()))
