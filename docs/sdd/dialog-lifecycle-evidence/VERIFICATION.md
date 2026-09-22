# Verification — 2026-09-22

Starting and tested Viewer revision: `149b86a908689477d1ba0cb4e7d486d091bf1024`.
No application or test code changed. The existing configured `build/test` artifact uses
GCC 16.2.1, Qt 6.11.2, Debug, and the project-local QML provider prefix. The unchanged
Images provider is `efe3e780327fa793fb76c82b18fddde15298120b`.

## Corrected lifecycle evidence

From the umbrella root:

```sh
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
QML_IMPORT_PATH="$PWD/holonight-viewer/build/deps/prefix/lib/qt6/qml" \
dbus-run-session --config-file=holonight-viewer/tests/fixtures/dbus-session.conf -- \
holonight-viewer/build/test/tests/viewer-smoke \
  --gtest_filter=Viewer.InspectionControlsAndLifecycle
ctest --test-dir holonight-viewer/build/test -R '^viewer-smoke$' --output-on-failure
```

Both passed outside the sandbox, which otherwise denies binding the private D-Bus socket.
Focused lifecycle: 1/1 passed. Full smoke: 182 passed, seven existing opt-in skips;
includes all 13 PortalFileChooser and six PortalViewer cases. CTest provides the same
private bus, offscreen platform and configured QML imports. No desktop session bus or
native pointer/focus automation was used.

The lifecycle test opens the fallback dialog, checks that the actual-size control is
disabled and zoom/fit commands cannot manipulate the image while modal, rejects the
dialog, checks preserved geometry, and confirms fit commands resume afterward.
The portal cases preserve portal-first selection, cancellation and error handling,
window destruction, focus restoration and fallback only when the portal cannot serve.

The earlier direct `viewer-smoke` invocation recorded in Files' orientation verification
omitted the required private-bus runner. Its failure waiting for `openDialog` is invalid
acceptance evidence for fallback lifecycle: a desktop portal can serve the request instead.
This corrected isolated result supersedes that failure assessment; the historical record
is retained. There is no reproduced application defect to fix.

Logs: `/tmp/shared-hardening-viewer-focused.log`, `/tmp/shared-hardening-viewer-smoke.log`
and `build/test/Testing/Temporary/LastTest.log`. Full smoke output reviewed. No new native
manual check is claimed. Documentation-only closure does not repeat application builds.
Final `git diff --check`, local SDD link validation and `reuse --no-multiprocessing lint`
passed (233/233 files). Publication and umbrella pin updates remain outside this handoff.
