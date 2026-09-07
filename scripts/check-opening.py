#!/usr/bin/env python3
"""Exercise CLI opening with generated fixtures and installed Qt image handlers."""

import base64
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parent.parent
binary = Path(sys.argv[1]).resolve(strict=True)
fixtures = Path(tempfile.mkdtemp(prefix='opening-check.', dir=root / 'build'))


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(b'\0\xff\0\0\x80')) + chunk(b'IEND', b''))
jpeg = base64.b64decode('/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAAUACgDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDi6KKK+ZP3E6Xwl/y+f8A/9mrpa8tuta1DSNn2G48rzc7/AJFbOOnUH1NVv+E08Qf9BD/yDH/8TXs4TgTMc1orG0JwUZbXcr6O3SLW67n88cc5PXxGf4irBqz5d7/yRXY9boryT/hNPEH/AEEP/IMf/wATRXT/AMQwzf8A5+U/vl/8gfJ/6v4n+aP3v/I0aKKK+dP6zMfXP+WH/Av6VkUUV+48H/8AIlo/9vf+lyPxzir/AJG9b/t3/wBJiFFFFfTHzx//2Q==')
for name, data in [('фото space.png', png), ('фото space.jpg', jpeg), ('-dash.png', png),
                   ('corrupt.png', b'broken image')]:
    (fixtures / name).write_bytes(data)
environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QSG_RHI_BACKEND='software', LC_ALL='C')
for arguments in [['one.png', 'two.png'], ['https://example.org/a.png'], ['file://host/a.png']]:
    result = subprocess.run([str(binary), *arguments], env=environment, capture_output=True, timeout=5)
    if result.returncode == 0 or b'exactly one local' not in result.stderr:
        raise SystemExit(f'CLI rejection failed: {arguments}: {result.stderr!r}')

for name in ['фото space.png', 'фото space.jpg', '-dash.png', 'missing.png', 'corrupt.png']:
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
print(f'CLI PNG/JPEG, Unicode/spaces, dash, rejection and recoverable errors passed: {fixtures}')
