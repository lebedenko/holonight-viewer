#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Test formatter discovery without relying on or modifying host Qt tools."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
(root / 'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='qml-discovery.', dir=root / 'build') as temporary:
    fixtures = Path(temporary)
    path = fixtures / 'path'
    path.mkdir()
    qt = fixtures / 'Qt tools'
    qt.mkdir()
    fallback = fixtures / 'arch-fallback'
    wrapper = fixtures / 'wrapper.sh'
    # Relocate only the fixed compatibility fallback so host installation cannot
    # mask missing-tool tests. All discovery logic is the production wrapper.
    wrapper.write_text((root / 'scripts/qml-format.sh').read_text().replace(
        '/usr/lib/qt6/bin/qmlformat', str(fallback)))
    env = dict(os.environ, PATH=str(path))
    env.pop('QMLFORMAT', None)

    def executable(file, body):
        file.write_text('#!/bin/bash\n' + body + '\n')
        file.chmod(0o755)

    def check(expected, override=None, code=0):
        current = dict(env)
        if override is not None:
            current['QMLFORMAT'] = override
        result = subprocess.run(['/bin/bash', str(wrapper), '-i', 'a file.qml'],
                                env=current, text=True, capture_output=True, check=False)
        assert result.returncode == code, (result.returncode, result.stderr)
        assert expected in result.stdout + result.stderr, (expected, result)

    check('Cannot find qmlformat', code=1)
    check('Invalid QMLFORMAT override', override='', code=1)
    check('Invalid QMLFORMAT override', override=str(fixtures / 'missing'), code=1)
    executable(fallback, 'printf "fallback\\n"')
    check('fallback')
    executable(path / 'qtpaths6', '[[ $1 == --query && $2 == QT_INSTALL_BINS ]] || exit 1\n'
               + 'printf "%s\\n" "' + str(qt) + '"')
    executable(qt / 'qmlformat', 'printf "qtpaths\\n"')
    check('qtpaths')
    executable(path / 'qmlformat-qt6', 'printf "path-qt6\\n"')
    check('path-qt6')
    executable(path / 'qmlformat', 'printf "path-default\\n"')
    check('path-default')
    explicit = fixtures / 'explicit formatter'
    executable(explicit, 'printf "override <%s> <%s>\\n" "$1" "$2"')
    check('override <-i> <a file.qml>', override=str(explicit))
    check('path-qt6', override='qmlformat-qt6')
    executable(explicit, 'echo "formatter failed" >&2\nexit 23')
    check('formatter failed', override=str(explicit), code=23)
    failed_check = subprocess.run(
        ['/bin/bash', str(root / 'scripts/check-qml-format.sh')],
        env=dict(os.environ, QMLFORMAT=str(explicit)), capture_output=True, check=False)
    assert failed_check.returncode == 23, failed_check
    explicit.chmod(0o644)
    check('Invalid QMLFORMAT override', override=str(explicit), code=1)
print('QML formatter override, precedence, spaces, discovery and failures passed')
