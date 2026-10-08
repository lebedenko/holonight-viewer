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
fetch_provider holonight-config d6a392b41991f70a004d58f7694c7b6115cb7280
fetch_provider holonight-qt 6c7ac33004702e166b8c152dcde918296be54286
fetch_provider holonight-images d834984dc413dc9e56f7f3157fa605d6a8667088
fetch_provider holonight-thumbnails 2284b1822b0f8f856677e14d21d91e8fa10bd98c
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
