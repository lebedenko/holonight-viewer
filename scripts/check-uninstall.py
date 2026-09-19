#!/usr/bin/env python3
"""The retired entrypoint must reject removal without modifying files."""
from pathlib import Path
import subprocess
result = subprocess.run(['bash', str(Path(__file__).with_name('uninstall.sh'))], capture_output=True)
assert result.returncode == 1
assert b'ownership manifest' in result.stderr
