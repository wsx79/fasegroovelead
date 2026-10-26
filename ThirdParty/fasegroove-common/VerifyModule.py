#!/usr/bin/env python3
"""Verify the complete vendored family module against its recorded revision.

Default: check this script's directory and its sibling .lock.json offline.
--canonical REPO also checks against a checkout of the exact pinned source SHA.
--write-lock is used by the canonical sync script after committing common/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def inventory(folder):
    result = {}
    for path in sorted(folder.rglob('*')):
        if '__pycache__' in path.parts:
            continue
        if path.is_symlink():
            raise ValueError(f'Symlink in shared module: {path}')
        if path.is_file():
            # Git autocrlf must not make Windows differ from the canonical LF files.
            result[path.relative_to(folder).as_posix()] = hashlib.sha256(
                path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    if not result:
        raise ValueError(f'Empty or missing shared module: {folder}')
    return result


def revision(repo):
    return subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()


def validate(module, lock, canonical=None):
    data = json.loads(lock.read_text(encoding='utf-8'))
    if data.get('schema') != 1 or data.get('repository') != 'https://github.com/wsx79/fasegroove':
        raise ValueError('Unsupported shared-module lock format or repository')
    if not re.fullmatch(r'[0-9a-f]{40}', data.get('revision', '')):
        raise ValueError('Invalid canonical revision')
    expected = data.get('files')
    if not isinstance(expected, dict) or not expected:
        raise ValueError('Missing shared-module inventory')
    actual = inventory(module)
    if actual != expected:
        changed = sorted(name for name in actual.keys() | expected.keys()
                         if actual.get(name) != expected.get(name))
        raise ValueError('Shared module differs from lock: ' + ', '.join(changed))
    if canonical is not None:
        if revision(canonical) != data['revision']:
            raise ValueError('Canonical checkout must match the pinned revision')
        if inventory(canonical / 'common') != expected:
            raise ValueError('Lock/module differs from the pinned canonical source')
    return data


def write_lock(module, lock, canonical):
    if canonical is None:
        raise ValueError('--write-lock needs --canonical')
    dirty = subprocess.check_output(
        ['git', '-C', str(canonical), 'status', '--porcelain', '--untracked-files=all', '--', 'common'],
        text=True)
    if dirty:
        raise ValueError('Commit common/ before syncing a pinned module')
    files = inventory(canonical / 'common')
    if files != inventory(module):
        raise ValueError('Copy the canonical common/ before recording the lock')
    data = {'schema': 1, 'repository': 'https://github.com/wsx79/fasegroove',
            'revision': revision(canonical), 'files': files}
    lock.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--canonical', type=Path)
    parser.add_argument('--write-lock', action='store_true')
    args = parser.parse_args()
    module = args.module.resolve()
    lock = module.with_name(module.name + '.lock.json')
    try:
        data = (write_lock(module, lock, args.canonical) if args.write_lock
                else validate(module, lock, args.canonical))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'FAIL {error}', file=sys.stderr)
        return 1
    print(f"PASS shared module: {len(data['files'])} files at {data['revision'][:12]}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
