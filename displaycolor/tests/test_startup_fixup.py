# SPDX-License-Identifier: Apache-2.0
"""Run with the original supplied stock executable; no Android build/device IO."""
import argparse
from collections import Counter
import hashlib
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import displayfeature_startup_fixup as fix


def word(data, pc):
    return struct.unpack_from('<I', data, fix._file_offset(data, pc))[0]


def publish_paths(data):
    # Branch outcomes are both explored. External calls return normally, with
    # only init/publication recorded. This is not an emulator for HAL failures,
    # registers, exception behavior or actual Binder dispatch.
    pending = [(fix.MAIN_START, (), frozenset())]
    outputs = []
    while pending:
        pc, events, visited = pending.pop()
        if not fix.MAIN_START <= pc < fix.MAIN_END or pc in visited:
            continue
        visited = visited | {pc}
        insn = word(data, pc)
        relative = fix._relative(insn, pc)
        if insn & 0xFC000000 == 0x94000000:  # BL
            target = relative[1]
            if target == fix.INITIALIZE:
                events += ('init',)
            elif target == fix.PUBLISH:
                outputs.append(events)
                events += ('publish',)
            pending.append((pc + 4, events, visited))
        elif insn & 0xFC000000 == 0x14000000:  # B
            pending.append((relative[1], events, visited))
        elif relative and relative[0] == 'branch':
            pending += [(relative[1], events, visited), (pc + 4, events, visited)]
        elif insn & 0xFFFFFC1F == 0xD65F0000:  # RET
            continue
        else:
            pending.append((pc + 4, events, visited))
    return outputs


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('stock', type=Path)
    args = p.parse_args()
    old = args.stock.read_bytes()
    new = fix.transform(old)
    assert hashlib.sha256(new).hexdigest() == fix.PATCHED_SHA256
    assert fix.transform(new) == new
    print('PASS pinned input/output and idempotence')
    for source in [old, new]:
        changed = bytearray(source)
        changed[-1] ^= 1
        try:
            fix.transform(bytes(changed))
        except ValueError:
            pass
        else:
            raise AssertionError('accepted unreviewed ELF')
    print('PASS reject unknown and modified output')
    assert old[:fix.MAIN_START] == new[:fix.MAIN_START]
    assert old[fix.MAIN_END:] == new[fix.MAIN_END:]
    assert len(old) == len(new)
    print('PASS exact preservation outside main (including ELF/unwind/ABI data)')
    for pc in range(fix.MAIN_START, fix.MAIN_END, 4):
        if fix.OLD_INIT <= pc < fix.JOIN:
            continue
        moved = pc + 8 if fix.ENTRY <= pc < fix.OLD_INIT else pc
        before, after = word(old, pc), word(new, moved)
        r = fix._relative(before, pc)
        if r:
            expected = fix._target(r[1]) if r[0] == 'branch' else r[1]
            assert fix._relative(after, moved)[1] == expected
            # Verify all bits outside the immediate remain unchanged.
            mask = ((3 << 29) | (0x7FFFF << 5) if r[0] in ['page', 'address']
                    else ((1 << r[2]) - 1) << r[3])
            assert before & ~mask == after & ~mask
        else:
            assert before == after
    print('PASS every relocated instruction and PC-relative target')
    assert fix._relative(word(new, fix.ENTRY), fix.ENTRY)[1] == fix.INITIALIZE
    assert word(new, fix.ENTRY + 4) == word(old, fix.OLD_INIT)
    assert fix._relative(word(new, fix.ENTRY - 4), fix.ENTRY - 4)[1] == 0x487C
    print('PASS null guard retained and object reloaded after init')
    old_paths, new_paths = publish_paths(old), publish_paths(new)
    assert old_paths and any('init' not in p for p in old_paths)
    assert new_paths and all(p == ('init',) for p in new_paths)
    print('PASS all modeled publication paths initialize exactly once first')
    for data in [old, new]:
        calls = Counter(fix._relative(word(data, pc), pc)[1]
                        for pc in range(fix.MAIN_START, fix.MAIN_END, 4)
                        if word(data, pc) & 0xFC000000 == 0x94000000)
        assert calls[fix.INITIALIZE] == calls[fix.PUBLISH] == 1
    print('PASS no added init/publication calls')


if __name__ == '__main__':
    main()
