# Isolated static raster decoding

Viewer's default raster metadata and first-frame decoding use `HolonightImages::Isolated`.
This milestone isolates static raster processing, not the entire Viewer. GIF's initial
frame is isolated; subsequent playback remains in-process. SVG validation, rendering,
and local resource handling remain unchanged. The grid retains File Browser's isolated
thumbnail provider.

## Consumer contract

Viewer validates local regular files and its 256 MiB input limit, retaining one
caller-owned `QFile` on the existing image worker. Metadata and full-resolution decode
are separate synchronous operations, with cancellation checked between them. Each
operation captures from zero, validates the complete runtime and starts a fresh helper.
The captures are not an atomic file snapshot. There is no combined provider operation,
runtime reuse, environment discovery, or in-process fallback.

The application-private `ImageRuntimeOptions` constructor and `--image-runtime` option
select an explicit bundle for tests and relocated installations. Production defaults
to `${CMAKE_INSTALL_FULL_LIBEXECDIR}/holonight-images/runtime`, configured through
`VIEWER_IMAGE_RUNTIME`; staging never embeds `DESTDIR`. Main supplies the same options
to document decoding and thumbnail composition. No public Images API or protocol changes.

The chooser and directory scan use one private raster suffix list: PNG; JPEG (`jpeg`,
`jpg`, `jpe`); BMP; TIFF (`tiff`, `tif`); WebP; GIF. SVG remains available. Explicitly
opened files still undergo content validation regardless of extension. Other raster
formats are rejected even when the host has a Qt plugin for them. Only untagged pixels
and profiles matching Qt's named color spaces are supported: sRGB, linear sRGB,
Adobe RGB, Display P3, ProPhoto RGB, BT.2020, BT.2100 PQ and BT.2100 HLG. Other valid
profiles return Unsupported, without conversion, profile removal, or fallback.

Structured EXIF facts retain Viewer's presentation formatting and metadata outcome.
Incomplete metadata remains nonfatal; infrastructure failures abort the request.
`DecodeResult::runtime` is separate from the optional provider outcome. Shared runtime
messages are used without manufacturing an image-format error. Cleanup failure takes
precedence over cancellation; ordinary cancellation drains silently through ImageLoader.
Full-resolution decode, orientation, output limits and premultiplied display conversion
remain unchanged. Failed and cancelled results cannot populate the decoded cache.
Request generations, foreground/tentative/prefetch scheduling and refresh epochs remain
owned by the existing loader and document.

## Verification

`tests/lanes.json` assigns every enumerated case exactly once. The portable lane uses
a private non-activating D-Bus session, including chooser mock coverage and injected
PNG fixtures for UI cases that invoke the chooser. Host decoding never uses that bus.
Graphics, opt-in performance and native/manual cases keep separate selections.

Run the portable suite with `task test`. Run required host regressions outside any
sandbox that restricts process visibility, namespaces or cgroups:

```sh
task test:isolated RUNTIME=/absolute/host-native/prefix/libexec/holonight-images/runtime
```

Alternatively configure `VIEWER_HOST_RUNTIME_TESTS=ON` and `VIEWER_TEST_RUNTIME` to
register host CTest checks. Missing runtime prerequisites, failed cases and skipped
required host cases fail acceptance. Container checks qualify portable behavior and
installed startup only; they cannot establish runtime isolation acceptance.

Install host-native Viewer and its qualification component into a disposable prefix
containing installed dependencies and a prepared Images runtime. Then run:

```sh
python3 scripts/check-isolated-viewer.py --prefix /absolute/disposable/prefix \
  --fixtures build/dev/tests/release-fixtures --output build/isolated-evidence
```

The gate checks all six formats, JPEG orientation, alpha and rendered Viewer output.
It copies the prefix, makes the original unavailable, repeats with explicit relocated
runtime and QML/library paths, and restores the original in a `finally` block.
`complete.json` is written only after both phases pass. Provider lifecycle evidence
is reused; launcher optimization, performance experiments, animation/SVG migration,
deployment, publication and pin updates are outside this milestone.

Local acceptance (2026-11-09):

- 209 portable cases and 182 required host cases passed, with no required skips.
- CLI opening and the rendered runtime CTest probe passed.
- All 24 container CTest checks passed. Formatting, static analysis, QML lint,
  import/type checks, REUSE and staged installed startup passed. Unchanged provider
  builds were reused for the final focused reruns after static-analysis fixes.
- The final host-native installed and relocated probes passed all six formats and
  rendered output, with the original staged prefix unavailable during relocation.
- Thirteen focused adapter/loader/document regressions cover runtime/provider
  outcomes, incomplete structured EXIF, cancellation between operations, invalid
  bundles without fallback, failed-cache exclusion, tentative opening/refresh and
  cleanup precedence for foreground and prefetch. Existing regressions cover
  generations, stale replies, navigation/cache reuse and shutdown.
- The offline lane-accounting checks passed (three tests).

Evidence is retained in ignored `build/isolated-acceptance-final/` (including
`complete.json`, per-phase logs and native test output) and
`build/acceptance/check-pfiddrn8/` (`final-check.log` and `final-delta.log`).
An earlier tool-sandbox invocation could not verify helper cleanup; the required
host lane was rerun outside that sandbox and passed. Container results alone do
not qualify the isolated runtime.
