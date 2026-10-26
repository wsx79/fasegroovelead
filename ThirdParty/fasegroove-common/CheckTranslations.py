#!/usr/bin/env python3
"""Reject duplicate keys within each effective translation table.

A product may override common texts; it may not silently override its own earlier
rows. All product catalogs are combined in the same order as Product.cpp.
"""
import argparse
import json
from pathlib import Path
import re
import sys


def entries(files):
    # Tokens preserve comments/URLs and support multiline rows and adjacent C++
    # string literals. Counting strings per physical line misses valid entries.
    token_pattern = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"|[{},]|\S')
    for path in files:
        text = path.read_text(encoding='utf-8')
        fields = None
        parts = []
        location = str(path)
        for match in token_pattern.finditer(text):
            token = match.group()
            if token.startswith(('//', '/*')):
                continue
            if token == '{' and fields is None:
                fields = []
                parts = []
                location = f"{path}:{text.count(chr(10), 0, match.start()) + 1}"
            elif token.startswith('"') and fields is not None:
                parts.append(json.loads(token))
            elif token in (',', '}') and fields is not None:
                if parts:
                    fields.append(''.join(parts))
                    parts = []
                elif token == ',':
                    raise ValueError(f'{location}: empty translation column')
                if token == '}':
                    if len(fields) != 6:
                        raise ValueError(f'{location}: expected six translation columns')
                    yield tuple(fields), location
                    fields = None
            elif token == ',' and fields is None:
                continue
            else:
                raise ValueError(f'{location}: unexpected translation token {token!r}')
        if fields is not None:
            raise ValueError(f'{location}: unfinished translation row')


def rows(files):
    seen = {}
    for values, location in entries(files):
        key = values[0]
        if key in seen:
            raise ValueError(f'{location}: duplicate key {key!r}; first at {seen[key]}')
        seen[key] = location
    return seen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product', type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    root = args.product.resolve()
    try:
        common = rows([root / 'ThirdParty/fasegroove-common/CommonTranslations.inc'])
        catalogs = [root / 'Source/Translations.inc']
        extra = root / 'Source/ArrangerTranslations.inc'
        if extra.exists():
            catalogs.append(extra)
        own = rows(catalogs)
    except (OSError, ValueError) as error:
        print(f'FAIL {error}', file=sys.stderr)
        return 1
    print(f'PASS translations: {len(common)} common, {len(own)} product keys; '
          f'{len(common.keys() & own.keys())} explicit product overrides')
    return 0


if __name__ == '__main__':
    sys.exit(main())
