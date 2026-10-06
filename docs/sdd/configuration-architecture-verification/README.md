# Configuration architecture standalone verification

Work package: CA-006a. Upstream baseline: `75752baac40257d6191d7be3fcba7477e3e1dea9`.

## Scope and evidence

Keep Viewer application behavior and configuration unchanged. Repair only acceptance fixtures discovered during umbrella integration against Config `d6a392b41991f70a004d58f7694c7b6115cb7280` and Qt `98803bca05e16ae0d0784a6cb43b0ace561385de`.

Four smoke failures reproduce with these providers and with the previous Qt provider `c434d6821140d5269550cb2388820f9cbfb2e163`. The grid fixture gives an integer-sized window a fractional-height child viewport: a selected cell ends at 1309 while the viewport ends at 1308.666666666667 after pixel-rounded scrolling. Give the fixture an integer-sized viewport and keep the strict full-cell containment assertion. The loading fixture opens an absent path in ambient `/tmp`; a completed empty scan prevents grid entry. Supply a real path in a private temporary folder while retaining the cancellation-controlled decoder and immediate loading assertions.

## Acceptance

- Reproduce the four original failures and pass them with deterministic fixtures.
- Run the repository-authorized clean external CMake acceptance with explicit provider/import paths, full CTest, source analysis, formatting, QML and installed contracts (equivalent coverage to `task check`).
- Verify installed runtime against the accepted providers without Shell or Settings.
- Review the final diff; publish only the completed repository change.

## Results

2026-10-06:

- Original four failures reproduced against accepted Qt and previous Qt (separate baseline build/import prefix); corrected focused tests passed 4/4.
- Fresh external acceptance: `cmake -S . -B build/configuration-architecture-acceptance -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=$PWD/build/deps/prefix -DQML_IMPORT_PATH=$PWD/build/deps/prefix/lib/qt6/qml`; `cmake --build build/configuration-architecture-acceptance -j 4`; full `ctest --test-dir build/configuration-architecture-acceptance --output-on-failure`: 27/27 passed in 173.11 seconds.
- `task format-check`, full `task tidy` coverage, clean-build `qml-lint`, `scripts/check-qmltypes.sh`, `task license-check`, `task qml-import-check` and `scripts/check-install.sh`: passed. Initial tidy found two existing fixture annotation omissions; corrected const/nodiscard and resumed all remaining translation units with the same configuration. Affected 24 loading/grid tests from the clean build passed in 11.546 seconds. No application source changes.
- Installed runtime: stage with `scripts/prepare-runtime-check.sh`, build disposable `holonight-configuration-viewer-runtime` from matching local Qt 6.11.2/GCC 16.2.1 image, install its missing AppStream validation tool, run `docker run --rm --network none holonight-configuration-viewer-runtime`: passed metadata/negative controls, seven-format GIO launches, required decoders and CLI opening checks. No source mounts, Shell or Settings installed. Qt version verified unchanged after adding the validation tool.
- Installed `hn-viewer` startup with a private sparse v2 appearance file (light scheme, purple accent, comment and unknown future table): passed; document bytes unchanged.
- Full logs reviewed; earlier annotation and sandbox socket failures were corrected/rechecked. Artifacts retained locally in `build/configuration-integration/`. Runtime image: `sha256:9948acf9f8f34e24e6e17b8cf31c7f71a2a49b8cebe4d6cec18b1621add4fb01`.

Final diff: two deterministic test fixture corrections and this SDD only. Manual ecosystem interaction remains owned by umbrella CA-006.
