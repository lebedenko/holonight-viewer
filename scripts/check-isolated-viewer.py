#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify installed host-native Viewer, then relocate with its original prefix unavailable."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--prefix', required=True, type=Path)
parser.add_argument('--fixtures', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
prefix = args.prefix.resolve(strict=True)
output = args.output.resolve()
if prefix in (Path('/usr'), Path('/usr/local'), Path('/')) or output.is_relative_to(prefix):
    parser.error('Use a disposable staged prefix and a separate fresh output directory')
output.mkdir(parents=True, exist_ok=False)
relocated = output / 'relocated'
hidden = prefix.with_name(prefix.name + '.viewer-unavailable')
if hidden.exists():
    parser.error('Unavailable-prefix path already exists')

def qualify(root, phase):
    environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QSG_RHI_BACKEND='software',
                       QT_QUICK_BACKEND='software', QT_QPA_PLATFORMTHEME='',
                       QML_IMPORT_PATH=str(root / 'lib/qt6/qml'), LD_LIBRARY_PATH=str(root / 'lib'))
    for name in ('QML2_IMPORT_PATH', 'QT_PLUGIN_PATH', 'QT_QUICK_CONTROLS_STYLE', 'QT_QUICK_CONTROLS_CONF'):
        environment.pop(name, None)
    for name in ('CONFIG', 'CACHE', 'DATA', 'STATE'):
        environment['XDG_' + name + '_HOME'] = str(output / phase / name.lower())
    command = [str(root / 'libexec/holonight-viewer/installed-runtime-probe'),
               str(args.fixtures.resolve(strict=True)), str(root / 'libexec/holonight-images/runtime')]
    with (output / (phase + '.log')).open('w') as log:
        subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=150)

qualify(prefix, 'installed')
shutil.copytree(prefix, relocated, symlinks=True)
prefix.rename(hidden)
try:
    qualify(relocated, 'relocated')
finally:
    hidden.rename(prefix)
(output / 'complete.json').write_text(json.dumps({'installed': str(prefix), 'relocated': str(relocated)}, indent=2) + '\n')
print('Installed and relocated host Viewer acceptance passed:', output)
