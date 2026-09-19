#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-build}"
if [[ "${build_dir}" != /* ]]; then
  build_dir="${repo_root}/${build_dir}"
fi

qmltypes_file="${build_dir}/apps/viewer/qml/holonight-viewer/HolonightViewer/holonight-viewer.qmltypes"
if [[ ! -s "${qmltypes_file}" ]]; then
  echo "Missing Viewer QML metadata: ${qmltypes_file}" >&2
  exit 1
fi

for type_name in ImageDocument ImageCanvas ClipboardController PortalFileChooser WindowState WindowKeyRouter; do
  grep -q "name: \"${type_name}\"" "${qmltypes_file}" || {
    echo "Missing QML type metadata for ${type_name}" >&2
    exit 1
  }
done

echo "Viewer QML type metadata check passed."
