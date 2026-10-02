#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Verify configured Qt discovery, explicit overrides, spaces, and failures."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='qml configured Qt spaces ') as temporary:
    fixtures = Path(temporary)
    (fixtures / 'scripts').mkdir()
    shutil.copy(root / 'scripts/qml-format.sh', fixtures / 'scripts/qml-format.sh')
    shutil.copytree(root / 'tooling', fixtures / 'tooling', ignore=shutil.ignore_patterns('__pycache__'))
    wrapper = fixtures / 'scripts/qml-format.sh'
    path = fixtures / 'path'
    path.mkdir()
    (path / 'python3').symlink_to(sys.executable)
    qt = fixtures / 'Qt tools'
    cache = fixtures / 'build/test/CMakeCache.txt'
    cache.parent.mkdir(parents=True)
    env = dict(os.environ, PATH=str(path))
    env.pop('QMLFORMAT', None)
    env.pop('Qt6_DIR', None)

    def executable(file, body):
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text('#!/bin/bash\n' + body + '\n')
        file.chmod(0o755)

    def check(expected, override=None, code=0):
        current = dict(env)
        if override is not None:
            current['QMLFORMAT'] = override
        result = subprocess.run(['/bin/bash', str(wrapper), '-i', 'a file.qml'],
                                env=current, text=True, capture_output=True)
        assert result.returncode == code, (result.returncode, result.stderr)
        assert expected in result.stdout + result.stderr, (expected, result)

    check('Cannot find qmlformat', code=1)
    executable(path / 'qmlformat', 'printf "stale-global\\n"')
    check('Cannot find qmlformat', code=1)  # PATH must not mask a missing configured kit.
    cache.write_text(f'Qt6_DIR:PATH={qt}/lib/cmake/Qt6\n')
    executable(qt / 'bin/qmlformat', 'printf "configured-qt\\n"')
    check('configured-qt')
    (qt / 'bin/qmlformat').chmod(0o644)
    check('Cannot find qmlformat', code=1)
    executable(qt / 'lib/qt6/bin/qmlformat', 'printf "configured-distro-qt\\n"')
    check('configured-distro-qt')
    explicit = fixtures / 'explicit formatter'
    executable(explicit, 'printf "override <%s> <%s> <%s>\\n" "$#" "$1" "$2"')
    check('override <2> <-i> <a file.qml>', override=str(explicit))
    executable(path / 'qmlformat-qt6', 'printf "path-override\\n"')
    check('path-override', override='qmlformat-qt6')
    for invalid in ['', str(fixtures / 'missing'), str(qt), 'printf']:
        check('Invalid QMLFORMAT override', override=invalid, code=1)
    executable(explicit, 'echo "formatter failed" >&2\nexit 23')
    check('formatter failed', override=str(explicit), code=23)
    failed_check = subprocess.run(['/bin/bash', str(root / 'scripts/check-qml-format.sh')],
                                 env=dict(os.environ, QMLFORMAT=str(explicit)), capture_output=True)
    assert failed_check.returncode == 23, failed_check
    explicit.chmod(0o644)
    check('Invalid QMLFORMAT override', override=str(explicit), code=1)
print('Configured Qt, stale-global rejection, explicit overrides, spaces and failures passed')
