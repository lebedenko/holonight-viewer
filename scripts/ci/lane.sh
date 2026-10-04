#!/bin/sh
set -eu
lane=$1
mkdir /work/viewer
cp -a /input/. /work/viewer/
cd /work/viewer
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC JOBS=2
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
cp scripts/ci/packages.json build/ci-provenance/supplements.json
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
  exit "$status"
}
trap capture 0
fetch_provider() {
  name=$1
  revision=$2
  git init -q "/work/$name"
  git -C "/work/$name" fetch --depth 1 "https://github.com/lebedenko/$name.git" "$revision"
  git -C "/work/$name" checkout --detach FETCH_HEAD
  [ "$(git -C "/work/$name" rev-parse HEAD)" = "$revision" ]
}
fetch_provider holonight-config 03fa635cedc506e101a148fc54f7eb46d17c6de5
fetch_provider holonight-qt f10e8c8ba57282e953f8ddc4a6b0c1109e05a3bf
fetch_provider holonight-images ac11f23e9ff2d70b1c142b643bc2abb6dd69f6c1
fetch_provider holonight-thumbnails d27addc044f277686850588147ec825c40c0f252
task deps
export LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib"
if [ "$lane" = sanitizer ]; then
  task sanitizer-check
else
  task check
  bash scripts/prepare-runtime-check.sh
  context=$(cat build/runtime-check-context)
  cp scripts/ci/Dockerfile.runtime "$context/Dockerfile"
  cp -a "$context" /output/runtime-context
  cp -a /work/packages /output/runtime-context/packages
  cp scripts/ci/packages.json /output/runtime-context/packages.json
  cp scripts/ci/install-runtime-tools.py /output/runtime-context/install-runtime-tools.py
fi
