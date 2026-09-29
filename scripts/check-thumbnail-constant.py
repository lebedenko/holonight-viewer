#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Check that the thumbnail box size is defined once and never repeated as a literal."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent.parent
viewer = root / 'apps/viewer'
header = viewer / 'thumbnail_size.h'
definition = re.compile(r'\bkThumbnailBoxLogical\s*=\s*256\s*;')
literal = re.compile(r'(?<![\w.])256(?![\w.])')
suffixes = {'.h', '.cpp', '.qml'}
# Thumbnail and grid sources; unrelated 256 limits elsewhere in the viewer are out of scope.
scope = re.compile(r'thumbnail|grid', re.IGNORECASE)


def scoped_files():
    return sorted(p for p in viewer.rglob('*') if p.suffix in suffixes and scope.search(p.name))


def check():
    errors = []
    if not header.is_file():
        return [f'{header.relative_to(root)} is missing']
    definitions = [p for p in viewer.rglob('*') if p.suffix in suffixes and definition.search(p.read_text())]
    if definitions != [header] or len(definition.findall(header.read_text())) != 1:
        names = ', '.join(str(p.relative_to(root)) for p in definitions) or 'none'
        errors.append(f'kThumbnailBoxLogical = 256 must be defined exactly once in thumbnail_size.h (found: {names})')
    for path in scoped_files():
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if literal.search(line) and not (path == header and definition.search(line)):
                errors.append(f'{path.relative_to(root)}:{number}: literal 256; use kThumbnailBoxLogical or ThumbnailMetrics.boxSize')
    return errors


if __name__ == '__main__':
    problems = check()
    for problem in problems:
        print(problem, file=sys.stderr)
    sys.exit(1 if problems else 0)
