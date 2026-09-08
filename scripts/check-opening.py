#!/usr/bin/env python3
"""Exercise CLI opening with generated fixtures and installed Qt image handlers."""

import os
import runpy
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
binary = Path(sys.argv[1]).resolve(strict=True)
fixtures = Path(tempfile.mkdtemp(prefix='opening-check.', dir=root / 'build'))


FORMATS = runpy.run_path(str(Path(__file__).with_name('format-fixtures.py')))['FORMATS']
png, jpeg = FORMATS['png'], FORMATS['jpg']
for name, data in [('фото space.png', png), ('фото space.jpg', jpeg), ('-dash.png', png),
                   ('corrupt.png', b'broken image'), ('sample.bmp', FORMATS['bmp']),
                   ('sample.webp', FORMATS['webp']), ('percent % фото.png', png)]:
    (fixtures / name).write_bytes(data)
environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QSG_RHI_BACKEND='software', LC_ALL='C')
for arguments in [['one.png', 'two.png'], ['https://example.org/a.png'], ['file://host/a.png']]:
    result = subprocess.run([str(binary), *arguments], env=environment, capture_output=True, timeout=5)
    if result.returncode == 0 or b'exactly one local' not in result.stderr:
        raise SystemExit(f'CLI rejection failed: {arguments}: {result.stderr!r}')

for name in ['фото space.png', 'фото space.jpg', '-dash.png', 'sample.bmp', 'sample.webp', 'percent % фото.png', 'missing.png', 'corrupt.png']:
    expected_error = name in ['missing.png', 'corrupt.png']
    process = subprocess.Popen([str(binary), '--', name], cwd=fixtures, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        try:
            output, errors = process.communicate(timeout=1)
        except subprocess.TimeoutExpired:
            process.terminate()
            output, errors = process.communicate(timeout=5)
        else:
            raise SystemExit(f'Viewer exited before user close for {name}: {errors!r}')
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()
    (fixtures / (name + '.log')).write_bytes(output + errors)
    if (b'Error opening' in errors) != expected_error:
        raise SystemExit(f'Unexpected opening result for {name}: {errors!r}')
    if any(message in errors for message in [b'failed to load', b'is not installed', b'ReferenceError', b'TypeError']):
        raise SystemExit(f'Runtime import/binding error for {name}: {errors!r}')
print(f'CLI PNG/JPEG/BMP/WebP, Unicode/spaces, dash, rejection and recoverable errors passed: {fixtures}')
