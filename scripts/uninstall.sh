#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' 'Viewer standalone removal is retired. Use the umbrella ownership manifest for coordinated removal.' 'For legacy standalone installs, inspect the original install manifest, package ownership and file modifications before manual cleanup.' >&2
exit 1
