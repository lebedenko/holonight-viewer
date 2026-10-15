#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify installed payloads in a fresh instance of the same development image."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
image, work = sys.argv[1], Path(sys.argv[2])
relative = (work / 'source/build/runtime-check-context').read_text().strip()
context = work / 'source' / relative
setup = '''set -euo pipefail
cp -a --no-preserve=ownership /payload/usr/. /usr/
chown -R root:root /usr/bin/hn-* /usr/share/applications/org.holonight.*
cp -a /checks /opt/check
ldconfig
update-desktop-database /usr/share/applications
install -d -m 0700 /root /run/viewer-test
export HOME=/root XDG_RUNTIME_DIR=/run/viewer-test
export QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software
cd /opt/check
'''
command = ['docker', 'run', '--rm', '--platform', 'linux/amd64', '--network=none',
           '--mount', f'type=bind,src={context / "payload"},dst=/payload,readonly',
           '--mount', f'type=bind,src={context / "check"},dst=/checks,readonly',
           '--entrypoint', 'bash', image, '-c']
with (work / 'installed-runtime.log').open('w') as log:
    subprocess.run(command + [setup + 'exec bash scripts/isolated-runtime.sh'],
                   stdout=log, stderr=subprocess.STDOUT, check=True)
