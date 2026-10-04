#!/usr/bin/env python3
"""Check the shared desktop/AppStream format and application identity contract."""
import argparse
import configparser
from pathlib import Path
import runpy
import subprocess
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
SCREENSHOT = 'https://raw.githubusercontent.com/lebedenko/holonight-viewer/main/docs/images/viewer-dark.png'


def check(desktop, metadata):
    entry = configparser.ConfigParser(interpolation=None)
    entry.read_string(desktop)
    entry = entry['Desktop Entry']
    component = ET.fromstring(metadata)
    formats = runpy.run_path(str(ROOT / 'scripts/format-fixtures.py'))['DESKTOP_FORMATS']
    expected = {mime for mime, _ in formats.values()}
    assert set(entry['MimeType'].strip(';').split(';')) == expected, 'Desktop MIME mismatch'
    assert {node.text for node in component.findall('provides/mediatype')} == expected, 'AppStream MIME mismatch'
    assert component.findtext('id') == entry['Icon'] == 'org.holonight.Viewer', 'Application identity mismatch'
    assert component.findtext('launchable') == 'org.holonight.Viewer.desktop', 'Desktop ID mismatch'
    assert component.find('launchable').get('type') == 'desktop-id'
    assert entry['Exec'] == 'hn-viewer -- %f', 'Executable mismatch'
    assert component.findtext('provides/binary') == 'hn-viewer', 'AppStream executable mismatch'
    assert component.findtext('metadata_license') == 'CC0-1.0'
    assert component.findtext('project_license') == 'GPL-3.0-or-later'
    image = component.find('screenshots/screenshot/image')
    assert image.text == SCREENSHOT and image.get('width') == '1200' and image.get('height') == '800', 'Screenshot mismatch'


def self_check(desktop, metadata):
    for mime in ('image/tiff', 'image/svg+xml', 'image/gif'):
        cases = [(desktop.replace(mime + ';', ''), metadata),
                 (desktop, metadata.replace(f'<mediatype>{mime}</mediatype>', ''))]
        for damaged_desktop, damaged_metadata in cases:
            try:
                check(damaged_desktop, damaged_metadata)
            except AssertionError:
                continue
            raise AssertionError(f'Missing {mime} was accepted')
    try:
        check(desktop, metadata.replace('org.holonight.Viewer.desktop', 'org.holonight.Other.desktop'))
    except AssertionError:
        return
    raise AssertionError('Mismatched desktop ID was accepted')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('desktop', nargs='?', type=Path, default=ROOT / 'packaging/org.holonight.Viewer.desktop')
    parser.add_argument('metadata', nargs='?', type=Path, default=ROOT / 'packaging/org.holonight.Viewer.metainfo.xml')
    parser.add_argument('--validate', action='store_true')
    args = parser.parse_args()
    assert args.desktop.name == 'org.holonight.Viewer.desktop'
    desktop, metadata = args.desktop.read_text(), args.metadata.read_text()
    check(desktop, metadata)
    if args.desktop.parent == ROOT / 'packaging':
        screenshot = ROOT / 'docs/images/viewer-dark.png'
        header = screenshot.read_bytes()[:24]
        assert header[:8] == b'\x89PNG\r\n\x1a\n'
        assert struct.unpack('>II', header[16:24]) == (1200, 800), 'Screenshot dimensions mismatch'
        assert 'docs/images/viewer-dark.png' in (ROOT / 'README.md').read_text()
    self_check(desktop, metadata)
    if args.validate:
        subprocess.run(['appstreamcli', 'validate', '--no-net', '--strict', str(args.metadata)], check=True)
    print('Desktop/AppStream metadata contract passed (including negative controls)')
