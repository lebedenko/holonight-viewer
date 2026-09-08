#!/usr/bin/env python3
"""Independent release fixtures: stdlib PNG/BMP, fixed JPEG, libwebp RGBA."""
import base64
import ctypes
import ctypes.util
from pathlib import Path
import struct
import sys
import zlib

def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(b'\0\xff\0\0\x80')) + chunk(b'IEND', b''))
jpeg = base64.b64decode('/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAAUACgDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDi6KKK+ZP3E6Xwl/y+f8A/9mrpa8tuta1DSNn2G48rzc7/AJFbOOnUH1NVv+E08Qf9BD/yDH/8TXs4TgTMc1orG0JwUZbXcr6O3SLW67n88cc5PXxGf4irBqz5d7/yRXY9boryT/hNPEH/AEEP/IMf/wATRXT/AMQwzf8A5+U/vl/8gfJ/6v4n+aP3v/I0aKKK+dP6zMfXP+WH/Av6VkUUV+48H/8AIlo/9vf+lyPxzir/AJG9b/t3/wBJiFFFFfTHzx//2Q==')
# BMP: one opaque red pixel, bottom-up BGR row padded to four bytes.
bmp = (b'BM' + struct.pack('<IHHI', 58, 0, 0, 54)
       + struct.pack('<IiiHHIIiiII', 40, 1, 1, 1, 24, 0, 4, 0, 0, 0, 0)
       + b'\0\0\xff\0')
# WebP lossless fixture encoded by libwebp, independently of Qt's image handlers.
codec = ctypes.CDLL(ctypes.util.find_library('webp'))
codec.WebPEncodeLosslessRGBA.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                       ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
codec.WebPEncodeLosslessRGBA.restype = ctypes.c_size_t
codec.WebPFree.argtypes = [ctypes.c_void_p]
output = ctypes.c_void_p()
size = codec.WebPEncodeLosslessRGBA(b'\xff\0\0\x80', 1, 1, 4, ctypes.byref(output))
if not size:
    raise RuntimeError('Cannot encode independent WebP fixture')
webp = ctypes.string_at(output, size)
codec.WebPFree(output)
# Preserve the exact compact regression even if a future encoder changes output.
compact_webp = bytes.fromhex('524946461c000000574542505650384c0f0000002f000000100710fd8f020622a2ff0100')
# Also exercise a normal RIFF with an optional unknown chunk. Qt 6.11.2 cannot
# scan the compact 36-byte stream; keep it separately as the mandatory compact regression.
webp += b'JUNK' + struct.pack('<I', 32) + bytes(32)
webp = webp[:4] + struct.pack('<I', len(webp) - 8) + webp[8:]
# EXIF orientation 6: stored 40x20 JPEG must display as 20x40.
exif = bytes.fromhex('ffe1002245786966000049492a0008000000010012010300010000000600000000000000')
jpeg = jpeg[:2] + exif + jpeg[2:]

def riff_chunk(kind, payload):
    return kind + struct.pack('<I', len(payload)) + payload + bytes(len(payload) % 2)


def riff(payload):
    return b'RIFF' + struct.pack('<I', len(payload) + 4) + b'WEBP' + payload


extended_webp = riff(riff_chunk(b'VP8X', b'\x10' + bytes(9)) + compact_webp[12:])
# Two independently specified animation frames, both red at alpha 128.
frame = riff_chunk(b'ANMF', bytes(12) + b'\x64\0\0\x02' + compact_webp[12:])
animated_webp = riff(riff_chunk(b'VP8X', b'\x12' + bytes(9))
                     + riff_chunk(b'ANIM', bytes(6)) + frame + frame)
codec.WebPEncodeRGBA.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                ctypes.c_int, ctypes.c_float, ctypes.POINTER(ctypes.c_void_p)]
codec.WebPEncodeRGBA.restype = ctypes.c_size_t
size = codec.WebPEncodeRGBA(b'\xff\0\0\xff', 1, 1, 4, 90, ctypes.byref(output))
if not size:
    raise RuntimeError('Cannot encode independent lossy WebP fixture')
lossy_webp = ctypes.string_at(output, size)
codec.WebPFree(output)

FORMATS = {'png': png, 'jpg': jpeg, 'bmp': bmp, 'webp': webp}

if __name__ == '__main__':
    destination = Path(sys.argv[1])
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'compact.webp').write_bytes(compact_webp)
    (destination / 'extended.webp').write_bytes(extended_webp)
    (destination / 'animated.webp').write_bytes(animated_webp)
    (destination / 'lossy.webp').write_bytes(lossy_webp)
    for extension, data in FORMATS.items():
        (destination / ('sample.' + extension)).write_bytes(data)
