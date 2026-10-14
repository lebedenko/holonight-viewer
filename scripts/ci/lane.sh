#!/bin/sh
set -eu
lane=$1
mkdir /work/viewer
cp -a /input/. /work/viewer/
cd /work/viewer
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
export JOBS=${JOBS:-2} CMAKE_BUILD_PARALLEL_LEVEL=${JOBS:-2}
mkdir -p "$HOME"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
export QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software
python3 --version
clang-format --version
clang-tidy --version
reuse --version
task --version
command -v xvfb-run
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_tooling_tidy.py
bash scripts/record-toolchain.sh
pacman -Q > build/ci-provenance/packages.txt
capture() {
  status=$?
  cp -a build/ci-provenance /output/
  for preset in test sanitizer; do
    if [ -d "build/$preset/Testing" ]; then
      mkdir -p "/output/$preset"
      cp -a "build/$preset/Testing" "/output/$preset/"
      find "build/$preset/tests" -maxdepth 1 -name 'viewer-menu-separators-*.png' -exec cp '{}' "/output/$preset/" \;
    fi
  done
  if [ -d .cache/tooling ]; then cp -a .cache/tooling /output/tooling; fi
  exit "$status"
}
trap capture 0
if [ "$lane" = prepare ]; then
  export HOLONIGHT_CONFIG_SOURCE=/prepared/sources/holonight-config
  export HOLONIGHT_QT_SOURCE=/prepared/sources/holonight-qt
  export HOLONIGHT_IMAGES_SOURCE=/prepared/sources/holonight-images
  export HOLONIGHT_THUMBNAILS_SOURCE=/prepared/sources/holonight-thumbnails
  if [ -d /prepared/deps ]; then cp -a /prepared/deps build/deps; fi
  task deps PRESET=test
  mkdir -p /prepared/deps
  cp -a build/deps/. /prepared/deps/
  cp -a build/ci-provenance /prepared/environment
  exit
fi
cp -a /prepared/deps build/deps
cp /prepared/provider-revisions.json build/ci-provenance/
cp build/deps/provider-revisions.tsv build/ci-provenance/
python3 scripts/ci/test_dependencies.py
export HOLONIGHT_TIDY_DATABASE="$PWD/build/test/compile_commands.json"
export LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib"
if [ "$lane" = sanitizer ]; then
  task sanitizer-check
else
  task check
  bash scripts/prepare-runtime-check.sh
  context=$(cat build/runtime-check-context)
  cp scripts/ci/Dockerfile.runtime "$context/Dockerfile"
  cp -a "$context" /output/runtime-context
fi
