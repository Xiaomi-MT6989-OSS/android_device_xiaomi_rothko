#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check all reachable public Git blobs/messages and declared payload exclusions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

RULES = {
    'host_path': re.compile(rb'/(?:mnt/(?:rothko-aosp-project|aosp-src)|home/tunkoal|run/user/[0-9]+/gvfs)'),
    'private_key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA |ENCRYPTED )?PRIVATE KEY-----'),
    'token': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b'),
    'url_password': re.compile(rb'https?://[^\s/@:]+:[^\s/@]+@'),
    'device_serial': re.compile(rb'\bLND[A-Z0-9]{13}\b'),
    'test_phone_ip': re.compile(rb'\b192[.]168[.]31[.]70\b'),
    'binary_patch': re.compile(rb'^GIT binary patch$', re.M),
    'lfs_payload_pointer': re.compile(rb'^version https://git-lfs[.]github[.]com/spec/v1$', re.M),
}
PAYLOAD_SUFFIXES = {'.so', '.apk', '.jar', '.ko', '.img', '.lz4', '.bin', '.dex',
                    '.a', '.o', '.db', '.ta', '.apex', '.p12', '.pfx', '.key'}
DEVICE_METADATA = {'prebuilts/audio/Android.bp', 'prebuilts/audio/PROVENANCE.txt',
                   'prebuilts/ims/Android.bp', 'prebuilts/ims/PROVENANCE.txt',
                   'prebuilts/modem/PROVENANCE.txt', 'prebuilts/modem/SHA256SUMS'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, default=Path.cwd())
    p.add_argument('--kind', choices=('device', 'vendor', 'kernel', 'integration'), required=True)
    p.add_argument('--output', type=Path)
    a = p.parse_args()
    repo = a.repo.resolve(strict=True)
    if a.output and a.output.exists():
        p.error('report must be a new file outside the repository')
    if a.output and a.output.resolve().is_relative_to(repo):
        p.error('report must be outside the repository')
    def git(*args, data=None):
        return subprocess.check_output(['git', '-C', str(repo), *args], input=data,
                                       env=dict(os.environ, GIT_OPTIONAL_LOCKS='0'))
    issues = []
    head = git('rev-parse', 'HEAD').decode().strip()
    objects = {}
    for row in git('rev-list', '--objects', '--all').splitlines():
        fields = row.split(b' ', 1)
        if len(fields) == 2:
            objects[fields[0].decode()] = fields[1].decode('utf-8', 'surrogateescape')
    ids = list(objects)
    meta = git('cat-file', '--batch-check=%(objectname) %(objecttype) %(objectsize)',
               data=('\n'.join(ids)+'\n').encode()).decode().splitlines()
    blobs, total = [], 0
    for line in meta:
        oid, kind, size = line.split()
        if kind != 'blob':
            continue
        size = int(size)
        if size > 8 * 1024 * 1024:
            issues.append({'object': oid, 'path': objects[oid], 'rule': 'oversize_or_payload'})
        else:
            blobs.append(oid)
            total += size
    if total > 128 * 1024 * 1024:
        raise ValueError('public text history exceeds audit budget; review explicitly')
    data = git('cat-file', '--batch', data=('\n'.join(blobs)+'\n').encode()) if blobs else b''
    offset = 0
    for oid in blobs:
        end = data.index(b'\n', offset)
        header = data[offset:end].split()
        size = int(header[2]); content = data[end+1:end+1+size]; offset = end+size+2
        if header[0].decode() != oid or header[1] != b'blob':
            raise ValueError('unexpected batch response')
        if b'\0' in content:
            issues.append({'object': oid, 'path': objects[oid], 'rule': 'binary_blob'})
        for rule, pattern in RULES.items():
            if pattern.search(content):
                issues.append({'object': oid, 'path': objects[oid], 'rule': rule})
    # Include all path aliases introduced on every parent edge, not just the
    # discovery path assigned to each blob by rev-list --objects.
    paths = set(git('-c', 'core.quotepath=false', 'log', '--all', '-m', '--root',
                    '--format=', '--name-only', '--no-renames').decode().splitlines()) - {''}
    for path in sorted(paths):
        forbidden = Path(path).suffix.lower() in PAYLOAD_SUFFIXES
        forbidden |= (a.kind == 'device' and path.startswith('prebuilts/') and path not in DEVICE_METADATA)
        forbidden |= a.kind == 'vendor' and (path.startswith('proprietary/') or path == 'PROVENANCE.json')
        forbidden |= a.kind == 'kernel' and (path.startswith(('vendor_ramdisk/', 'vendor_dlkm/', 'system_dlkm/', 'dtb/'))
                                             or path in ('Image.lz4', 'dtbo.img', 'kernel.config', 'PROVENANCE.json'))
        if forbidden:
            issues.append({'path': path, 'rule': 'excluded_payload_path_in_history'})
    messages = git('log', '--all', '--format=%H%x00%B%x00').split(b'\0')
    for i in range(0, len(messages)-1, 2):
        oid, body = messages[i].strip().decode(), messages[i+1]
        for rule, pattern in RULES.items():
            if pattern.search(body):
                issues.append({'commit': oid, 'rule': rule})
    alternates = Path(git('rev-parse', '--git-path', 'objects/info/alternates').decode().strip())
    if not alternates.is_absolute():
        alternates = repo / alternates
    if alternates.exists():
        issues.append({'rule': 'external_object_store'})
    fsck = subprocess.run(['git', '-C', str(repo), 'fsck', '--full', '--no-reflogs', '--unreachable'],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if fsck.returncode or b'unreachable ' in fsck.stdout or b'dangling ' in fsck.stdout:
        issues.append({'rule': 'git_fsck_or_unreachable_objects'})
    if git('rev-parse', 'HEAD').decode().strip() != head:
        raise ValueError('HEAD changed during scan')
    result = {'head': head, 'kind': a.kind, 'reachable_text_blobs': len(blobs),
              'bytes_scanned': total, 'history_paths': len(paths), 'issues': issues,
              'scope': 'mechanical publication checks, not licensing/runtime/security certification'}
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        with a.output.open('x') as f:
            json.dump(result, f, indent=2); f.write('\n')
    print(json.dumps(result, indent=2))
    if issues:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
