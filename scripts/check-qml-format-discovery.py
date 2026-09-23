#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Test formatter discovery without relying on or modifying host Qt tools."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='qml-discovery.') as temporary:
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
    executable(path / 'qmlformat', 'printf "path-default\\n"')
    check('path-default')
    executable(path / 'qmlformat-qt6', 'printf "path-qt6\\n"')
    check('path-qt6')
    executable(fallback, 'printf "fallback\\n"')
    check('fallback')

    properties = ('QT_INSTALL_BINS', 'QT_HOST_BINS',
                  'QT_INSTALL_LIBEXECS', 'QT_HOST_LIBEXECS')
    directories = [qt / property for property in properties]
    query = '[[ $1 == --query ]] || exit 1\ncase $2 in\n'
    for property, directory in zip(properties, directories):
        directory.mkdir()
        executable(directory / 'qmlformat', f'printf "{property}\\n"')
        query += f'{property}) printf "%s\\n" "{directory}" ;;\n'
    query += '*) exit 1 ;;\nesac'
    executable(path / 'qtpaths6', query)
    # All candidates compete initially; remove each winner to test query order.
    for property, directory in zip(properties, directories):
        check(property)
        (directory / 'qmlformat').unlink()
    check('fallback')

    executable(qt / 'qmlformat', 'printf "qtpaths\\n"')
    executable(path / 'qtpaths6',
               '[[ $1 == --query ]] || exit 1\ncase $2 in\n'
               'QT_INSTALL_BINS) echo "failed query" >&2; exit 1 ;;\n'
               'QT_HOST_BINS) exit 0 ;;\n'
               f'QT_INSTALL_LIBEXECS) printf "%s\\n" "{qt}" ;;\n'
               '*) exit 1 ;;\nesac')
    check('qtpaths')
    (qt / 'qmlformat').chmod(0o644)
    check('fallback')
    executable(path / 'qtpaths6', 'echo "failed query" >&2\nexit 1')
    check('fallback')
    fallback.unlink()
    check('path-qt6')
    (path / 'qmlformat-qt6').unlink()
    check('path-default')
    (path / 'qmlformat').unlink()
    check('Cannot find qmlformat', code=1)

    # Restore every discovery tier while validating explicit overrides.
    executable(fallback, 'printf "fallback\\n"')
    executable(qt / 'qmlformat', 'printf "qtpaths\\n"')
    executable(path / 'qtpaths6', f'printf "%s\\n" "{qt}"')
    executable(path / 'qmlformat-qt6', 'printf "path-qt6\\n"')
    executable(path / 'qmlformat', 'printf "path-default\\n"')
    check('Invalid QMLFORMAT override', override='', code=1)
    check('Invalid QMLFORMAT override', override=str(fixtures / 'missing'), code=1)
    check('Invalid QMLFORMAT override', override=str(qt), code=1)
    check('Invalid QMLFORMAT override', override='printf', code=1)
    explicit = fixtures / 'explicit formatter'
    executable(explicit, 'printf "override <%s> <%s> <%s>\\n" "$#" "$1" "$2"')
    check('override <2> <-i> <a file.qml>', override=str(explicit))
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
