#!/usr/bin/env python3
"""Launch the installed Viewer through GIO, without changing MIME defaults."""
import os
from pathlib import Path
import runpy
import signal
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
fixtures = Path(tempfile.mkdtemp(prefix='installed-desktop.', dir=root / 'build'))
formats = runpy.run_path(str(Path(__file__).with_name('format-fixtures.py')))['FORMATS']
entry = Path(sys.argv[1]).resolve(strict=True)
subprocess.run(['desktop-file-validate', str(entry)], check=True)
for extension, data in formats.items():
    path = fixtures / ('-фото 100% space.' + extension)
    path.write_bytes(data)
    process = subprocess.Popen(['gio', 'launch', str(entry), str(path)],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
    try:
        try:
            out, err = process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            out, err = process.communicate(timeout=5)
        else:
            raise RuntimeError(f'Viewer exited before user close: {out!r} {err!r}')
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
    (fixtures / (extension + '.log')).write_bytes(out + err)
    if any(word in err.lower() for word in [b'error', b'failed', b'not installed', b'not found']):
        raise RuntimeError(err.decode(errors='replace'))
    assert path.read_bytes() == data
print(f'Installed GIO launch passed for four formats: {fixtures}')
