#!/usr/bin/env python3
"""Verify development registration without changing the host desktop database."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile


root = Path(__file__).resolve().parent.parent
prefix = Path(os.environ['HOLONIGHT_DEPENDENCY_PREFIX']).resolve(strict=True)
imports = Path(os.environ['HOLONIGHT_QML_IMPORT_PATH']).resolve(strict=True)
data = Path(tempfile.mkdtemp(prefix='desktop-check.', dir=root / 'build'))
environment = dict(os.environ, XDG_DATA_HOME=str(data), QT_QPA_PLATFORM='offscreen')
desktop_name = 'org.holonight.Viewer.desktop'
installed = (root / 'packaging' / desktop_name).read_bytes()

for preset in ('debug', 'release'):
    executable = (root / 'build' / preset / 'apps/viewer/holonight-viewer').resolve(strict=True)
    subprocess.run(['python3', str(root / 'scripts/register-desktop.py'),
                    str(executable), str(prefix), str(imports)], env=environment, check=True)
    entry = data / 'applications' / desktop_name
    subprocess.run(['desktop-file-validate', str(entry)], check=True)
    fields = dict(line.split('=', 1) for line in entry.read_text().splitlines() if '=' in line)
    # These generated default paths contain no desktop-string escape sequences.
    # shlex decodes the quoted Exec arguments; this never invokes a shell.
    if '\\' in fields['Exec'] or '%' in fields['Exec'].replace('%f', ''):
        raise SystemExit('This check requires paths without backslashes or percent signs')
    arguments = shlex.split(fields['Exec'])
    if arguments[1:] != [f'QML_IMPORT_PATH={imports}', f'LD_LIBRARY_PATH={prefix / "lib"}',
                          str(executable), '--', '%f']:
        raise SystemExit(f'Unexpected development Exec arguments: {arguments}')
    subprocess.run([*arguments[:-2], '--version'], env=environment, check=True)
    icon = data / 'icons/hicolor/scalable/apps/org.holonight.Viewer.svg'
    if icon.read_bytes() != (root / 'packaging/org.holonight.Viewer.svg').read_bytes():
        raise SystemExit('Development icon differs from packaged icon')

if (root / 'packaging' / desktop_name).read_bytes() != installed:
    raise SystemExit('Development registration changed packaged desktop entry')
fields = dict(line.split('=', 1) for line in installed.decode().splitlines() if '=' in line)
if fields['Exec'] != 'holonight-viewer -- %f' or fields.get('MimeType') != 'image/png;image/jpeg;image/bmp;image/webp;':
    raise SystemExit('Installed entry must preserve file separation and guaranteed MIME types')
print(f'Development build switching and packaged entry separation passed: {data}')

# Exercise GIO's actual desktop-entry parser and %f expansion. The recorder is
# test tooling; it captures exactly what the application process would receive.
import json
import time

recorder = data / 'receiver % фото space'
result_path = data / 'arguments.json'
recorder.write_text('#!/usr/bin/python3\nimport json, os, sys\n'
                    + f'with open({str(result_path) + ".tmp"!r}, "w") as output:\n'
                    + '    json.dump([sys.argv[1:], os.getenv("QML_IMPORT_PATH"), '
                    + 'os.getenv("LD_LIBRARY_PATH")], output)\n'
                    + f'os.replace({str(result_path) + ".tmp"!r}, {str(result_path)!r})\n')
recorder.chmod(0o755)
subprocess.run(['python3', str(root / 'scripts/register-desktop.py'), str(recorder),
                str(prefix), str(imports)], env=environment, check=True)
entry = data / 'applications' / desktop_name
subprocess.run(['desktop-file-validate', str(entry)], check=True)
for name in ['фото space.png', '100% image.jpg', '-dash.bmp', 'transparent.webp']:
    path = data / name
    path.write_bytes(b'argument fixture')
    result_path.unlink(missing_ok=True)
    subprocess.run(['gio', 'launch', str(entry), str(path)], env=environment, check=True)
    deadline = time.monotonic() + 5
    while not result_path.exists() and time.monotonic() < deadline:
        time.sleep(0.02)
    actual = json.loads(result_path.read_text())
    assert actual == [['--', str(path)], str(imports), str(prefix / 'lib')], actual
print('GIO file substitution and argument/provider integrity passed')
