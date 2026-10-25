#!/usr/bin/env python3
"""Verify the family module has no local patches."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]) / 'common'
target = root / 'ThirdParty/fasegroove-common'
a = {str(p.relative_to(source)): p.read_bytes() for p in source.rglob('*') if p.is_file()}
b = {str(p.relative_to(target)): p.read_bytes() for p in target.rglob('*') if p.is_file()}
if a != b:
    raise SystemExit('Shared module differs from canonical fasegroove/common')
print('Shared module: identical')
