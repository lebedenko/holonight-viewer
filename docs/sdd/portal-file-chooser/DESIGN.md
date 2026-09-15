<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com> -->

# Portal File Chooser Integration — DESIGN

Implements SPEC (`docs/sdd/portal-file-chooser/SPEC.md`, REQ-F-001..024,
REQ-NF-001..011, REQ-C-001..005). Facts below were verified on this machine
(Arch Linux, Qt 6.11.2, wayland 1.26.0, wayland-protocols 1.49) unless marked
**unverified**.

## 1. Overview

Ctrl+O, the "Open…" menu item and the empty-state button set
`window.dialogRequested = true`, unchanged. Today that drives a `Loader`
instantiating a Qt `FileDialog` directly. This design inserts a new
`QML_ELEMENT` C++ object, `PortalFileChooser`, ahead of that `Loader`: it owns
the D-Bus `OpenFile` call, the xdg-foreign export, and the fallback decision.
The `Loader`/`FileDialog` stay byte-for-byte in markup, but their `active`
binding moves from `window.dialogRequested` to `portalFileChooser.
fallbackShown`.

Three new C++ translation units, no new QML files:

- `apps/viewer/portal_request_builder.h/.cpp` — pure functions/value types:
  filter construction, `current_folder` bytes, request-path prediction,
  `parent_window` formatting. No `QObject`, no D-Bus, no Wayland; unit-tested
  like `image_orientation.cpp`'s helpers.
- `apps/viewer/wayland_foreign_export.h/.cpp` — RAII helper around
  `zxdg_exporter_v2`/`zxdg_exported_v2` using raw `wayland-client` + a
  wayland-scanner-generated header (not `QtWaylandClient`, see §3).
- `apps/viewer/portal_file_chooser.h/.cpp` — the `QML_ELEMENT` `QObject`
  driving the state machine (§5), owning the `QDBusPendingCallWatcher` and
  `Request.Response` subscription.

## 2. Components

### 2.1 `PortalRequestBuilder` (free functions, `portal_request_builder.h`)

```cpp
struct PortalFileFilter {  // marshals as "a(us)" inside "a(sa(us))"
  QString label;
  QList<QPair<quint32, QString>> patterns;  // type 0 = glob, REQ-F-003
};
Q_DECLARE_METATYPE(PortalFileFilter)
void registerPortalMetaTypes();  // qDBusRegisterMetaType<T>, idempotent

QList<PortalFileFilter> buildPortalFilters(const QStringList& nameFilters);
QByteArray currentFolderBytes(const QString& localFilePath);   // NUL-terminated, "" if no path
QString formatParentWindow(const QString& exportedHandle);     // "wayland:<h>" or ""
QString sanitizeSenderForPath(const QString& uniqueBusName);   // ":1.42" -> "1_42"
QString predictedRequestPath(const QString& uniqueBusName, const QString& handleToken);
QString newHandleToken();  // "hn_" + uuid, '-' -> '_'
```

All operate on value types only (`QString`/`QByteArray`/`QList`) — no
`QObject`, no bus connection, no Wayland — so they run under plain `TEST()`
with no `QCoreApplication`.

### 2.2 `WaylandForeignExport`/`WaylandForeignExporter` (`wayland_foreign_export.h/.cpp`)

```cpp
class WaylandForeignExport {  // one per OpenFile call, never cached (REQ-F-008/021)
 public:
  WaylandForeignExport() noexcept;               // empty/unavailable
  WaylandForeignExport(WaylandForeignExport&&) noexcept;
  ~WaylandForeignExport();                        // destroys zxdg_exported_v2
  [[nodiscard]] bool isValid() const;
  [[nodiscard]] QString handle() const;
 private:
  struct Impl; std::unique_ptr<Impl> impl_;
};

class WaylandForeignExporter {  // process-lifetime; binds zxdg_exporter_v2 lazily
 public:
  explicit WaylandForeignExporter(QObject* parent = nullptr);
  // Invalid result (never a warning) if not Wayland, no compositor global,
  // no surface yet, or the roundtrip fails — REQ-F-009.
  [[nodiscard]] WaylandForeignExport exportWindow(QWindow* window);
 private:
  struct Impl; std::unique_ptr<Impl> impl_;
};
```

### 2.3 `PortalFileChooser` (`portal_file_chooser.h/.cpp`)

```cpp
class PortalFileChooser : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
  Q_PROPERTY(QStringList nameFilters MEMBER m_nameFilters NOTIFY nameFiltersChanged)
  Q_PROPERTY(QString currentLocalPath MEMBER m_currentLocalPath NOTIFY currentLocalPathChanged)
  Q_PROPERTY(bool fallbackShown READ fallbackShown NOTIFY fallbackShownChanged)
 public:
  explicit PortalFileChooser(QObject* parent = nullptr);
  ~PortalFileChooser() override;
  void setWindow(QQuickWindow* window);
  [[nodiscard]] bool fallbackShown() const;
  Q_INVOKABLE void requestOpen();               // Ctrl+O/menu/empty-state entry point; no-op if not Idle
  Q_INVOKABLE void fallbackAccepted(const QUrl& selectedFile);
  Q_INVOKABLE void fallbackRejected();
  void setCallTimeoutMsForTesting(int milliseconds);   // default 25000, REQ-F-017
 signals:
  void finished(QList<QUrl> urls);  // code 0, valid or not — REQ-F-012/013
  void cancelled();                 // REQ-F-014
  // + windowChanged/nameFiltersChanged/currentLocalPathChanged/fallbackShownChanged
 private:
  enum class State { Idle, Exporting, Calling, AwaitingResponse, FallbackShown };
  void beginCall();
  void onOpenFileFinished(QDBusPendingCallWatcher* watcher);
  void onResponse(uint code, const QVariantMap& results);
  void subscribeResponse(const QString& path);
  void unsubscribeResponse();
  void finishRequest();       // unsubscribe + destroy export, -> Idle
  void closeActiveRequest();  // Request.Close; used by finishRequest() and dtor
  // QPointer<QQuickWindow> m_window; WaylandForeignExporter m_exporter;
  // WaylandForeignExport m_activeExport; State m_state; QString m_handleToken,
  // m_subscribedPath; QDBusObjectPath m_activeHandle; int m_callTimeoutMs = 25000;
};
```

`fallbackAccepted`/`fallbackRejected` replace the old inline
`onAccepted`/`onRejected` bodies: the fallback `FileDialog` still calls
`window.document.open(...)` from QML (unchanged), it just also tells
`PortalFileChooser` the request resolved.

### 2.4 QML consumption: instance, not context property

**Decision:** instantiate `PortalFileChooser` directly in `Main.qml`, the way
`WindowKeyRouter` is instantiated today (`target: window`-style wiring) —
not a context property, not an initial property from `main.cpp`.

Rationale: `WindowKeyRouter` is the closest precedent for a `QML_ELEMENT`
needing a `QQuickWindow*` plus window-level state, and its lifetime should
match the window's (mirrors REQ-F-021's "window closes ⇒ object destroyed").
`ImageDocument` is an initial property because it outlives QML engine
failures and is shared with `main.cpp`'s CLI-open path; `PortalFileChooser`
has neither need. A context property was rejected: every test that
instantiates `Main.qml` would need to seed it even when unrelated to Open.

Alternative rejected: sequence "try portal, then FileDialog" purely in QML
inside the existing `Loader`. Rejected because REQ-F-010 requires
subscribing to `Request.Response` before the D-Bus call returns, and
REQ-F-017/023 require two different timeout policies (25 s on the call, none
while awaiting Response) — that belongs in C++ state, not QML bindings.

### 2.5 `Main.qml` changes

```qml
PortalFileChooser {
    id: portalFileChooser
    objectName: "portalFileChooser"
    window: window
    nameFilters: window.document.nameFilters
    currentLocalPath: window.document.localPath
    // `urls` is the portal's `uris` list as QUrls; ImageDocument::open() already
    // rejects empty, multiple and non-local lists with its error state (REQ-F-013).
    onFinished: urls => { window.document.open(urls); window.dialogRequested = false; }
    onCancelled: {
        const previousFocus = window.dialogFocusItem;
        window.dialogRequested = false;
        if (!window.modalActive && previousFocus && previousFocus.visible && previousFocus.enabled)
            previousFocus.forceActiveFocus(Qt.OtherFocusReason);
    }
}
property Item dialogFocusItem: null
onDialogRequestedChanged: {
    if (window.dialogRequested) {
        window.dialogFocusItem = window.activeFocusItem;
        portalFileChooser.requestOpen();
    } else {
        window.dialogFocusItem = null;
        portalFileChooser.cancel();
    }
}

Loader {
    active: portalFileChooser.fallbackShown   // was: window.dialogRequested
    sourceComponent: Item {
        FileDialog {
            id: fileDialog
            objectName: "openDialog"
            title: qsTr("Open image")
            fileMode: FileDialog.OpenFile
            nameFilters: window.document.nameFilters
            onAccepted: portalFileChooser.fallbackAccepted(fileDialog.selectedFile)
            onRejected: portalFileChooser.fallbackRejected()
            Component.onCompleted: fileDialog.open()
        }
    }
}
```

`window.dialogRequested` keeps its exact current meaning ("an Open flow,
portal or fallback, is outstanding"), so `modalActive` (REQ-C-005) needs no
change. `ImageDocument` is **not** changed: `ImageDocument::open()`
(`image_document.cpp:274-298`) already puts the document into `Error` with a
translated message for any list that is not exactly one local URL, so a
code-0 Response with empty or non-`file://` `uris` is forwarded unchanged and
reuses that existing error path (REQ-F-013). REQ-C-004 ("no `openDialog` until fallback") holds because the
`Loader`'s `active` is `fallbackShown`, false until `FallbackShown` is entered.

## 3. Wayland surface access (Qt 6.11.2, verified)

**`wl_display*`:** public, stable — `QNativeInterface::QWaylandApplication`
(`QtGui/qguiapplication_platform.h`, `Q_GUI_EXPORT`):
`qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()->display()`;
returns `nullptr` off-Wayland (REQ-F-009/NF-002 — skip silently).

**`wl_surface*`:** no stable public accessor exists in Qt 6.11. The only
accessor is `QNativeInterface::Private::QWaylandWindow`
(`QtGui/6.11.2/QtGui/qpa/qplatformwindow_p.h` — versioned path, part of
`Qt6::GuiPrivate`, already present via `qt6-base`): `virtual wl_surface*
surface() const`, plus `surfaceCreated()`/`surfaceDestroyed()` signals
(covers SPEC's "Surface Recreation Handling" risk).
`window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()` is
`nullptr` off-Wayland or before the surface exists.

**Decision:** use this private interface, fetched fresh per `requestOpen()`
call, link `Qt6::GuiPrivate`. **Risk (accepted):** `Qt6GuiPrivateDependencies.
cmake` pins `find_dependency(Qt6 6.11.2 ...)` exact-version — a Qt point bump
needs a rebuild and a re-check that the header still has this shape; same
class of risk the project already accepts for holonight-qt's private-Qt use
(`docs/holonight-qt-dlg-delegation-missed.md`).

Rejected: `QtWaylandClient::QWaylandWindow` (`private/qwaylandwindow_p.h`) —
same information, but needs a second private module
(`Qt6::WaylandClientPrivate`) instead of the one `GuiPrivate` already implies.

**Binding `zxdg_exporter_v2` without disturbing Qt's queue.** Decision: raw
`wayland-client` C API with a dedicated `wl_event_queue`, matching REQ-C-003's
plain-`wayland-scanner` framing — not `QWaylandClientExtensionTemplate`.
Once, lazily: `wl_display_create_queue`, then `wl_proxy_create_wrapper(display)`
with `wl_proxy_set_queue(wrapper, queue)` and `wl_display_get_registry(wrapper)`
— the wrapper idiom guarantees the registry's first events land on our queue
(calling `wl_proxy_set_queue` on a registry created on the default queue races
Qt's reader thread). Add a registry listener, then
`wl_display_roundtrip_queue(display, queue)` to flush the initial global
burst and `wl_registry_bind` the `zxdg_exporter_v2` global when seen. Per
export: `zxdg_exporter_v2_export_toplevel` (the exporter was bound on our
queue, so the new `zxdg_exported_v2` inherits it), add a
`handle`-event listener, `wl_display_roundtrip_queue` again to block until
`handle` arrives.

`wl_display_roundtrip_queue` dispatches only the given queue; events for Qt's
queues are routed to those queues, not dispatched here, so Qt's dispatch is
never re-entered. QtWaylandClient reads the socket from its own event thread;
libwayland's `prepare_read_queue`/`read_events` protocol, which
`wl_display_dispatch_queue` uses internally, is explicitly designed for
multiple concurrent readers, so this is safe alongside Qt's reader. This makes "Exporting" (§5)
a bounded synchronous roundtrip, not an async callback.

Rejected: fully async dispatch via a `QSocketNotifier` on
`wl_display_get_fd()`. Manually interleaving `prepare_read`/`read_events`
with Qt's own Wayland QPA plugin on the same `wl_display`/thread is exactly
unnecessary complexity for a roundtrip that completes in milliseconds.

**Risk (accepted):** a stalled compositor blocks `requestOpen()` with no
app-level timeout (libwayland has none) — distinct from, and far tighter
than, the 25 s D-Bus timeout; a stalled compositor already blocks the whole
UI today, so this doesn't add a new failure mode.

Rejected alternative worth recording: `QWaylandClientExtensionTemplate<T>` +
`qt6_generate_wayland_protocol_client_sources()` — a public Qt6WaylandClient
API (confirmed present: `qwaylandclientextension.h`, `Q_WAYLANDCLIENT_EXPORT`;
the CMake macro ships in `Qt6WaylandScannerTools`, owned by `qt6-base`,
alongside `/usr/lib/qt6/qtwaylandscanner`). It binds through Qt's own
registry/queue, sidestepping the whole question by construction, and is the
better long-term answer if xdg-foreign usage grows. Not chosen because: (a)
it's a new `Qt6::WaylandClient` link dependency beyond REQ-NF-001's plain
`wayland-client`/`wayland-protocols`/`wayland-scanner`; (b) wiring
`find_package(WaylandScanner)` needs Qt's bundled `extra-cmake-modules` on
`CMAKE_MODULE_PATH` — **unverified** end-to-end; (c) REQ-C-003 is framed
around plain `wayland-scanner` output.

## 4. CMake / build changes

`apps/viewer/CMakeLists.txt`:

```cmake
find_package(Qt6 6.11 REQUIRED COMPONENTS DBus GuiPrivate)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WaylandProtocols REQUIRED IMPORTED_TARGET wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
set(_xdg_foreign_xml "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml")
# add_custom_command x2: client-header -> build/.../xdg-foreign-unstable-v2-client-protocol.h
#                        private-code  -> build/.../xdg-foreign-unstable-v2-protocol.c
```

All finds `REQUIRED` — missing dependency fails configure with CMake's
standard message naming the package (REQ-NF-001, no silent disable). The
generated `.c` compiles as a plain C file alongside `viewer-ui`'s sources.
The top-level `project()` currently declares `LANGUAGES CXX` only, so it must
become `LANGUAGES C CXX` (or `enable_language(C)` in `apps/viewer`) — otherwise
CMake refuses the `.c` source (no new library — REQ-C-003's "build/
artifacts", generated-before-compiled via the `DEPENDS` edge).
`target_sources(viewer-ui PRIVATE portal_request_builder.{h,cpp}
wayland_foreign_export.{h,cpp} portal_file_chooser.{h,cpp} <generated .c>)`
added next to `image_document.cpp` etc. in the existing `qt_add_qml_module`
call. `target_link_libraries(viewer-ui PRIVATE Qt6::DBus Qt6::GuiPrivate
PkgConfig::WaylandClient)`.

`tests/CMakeLists.txt`: add `portal_request_builder_test.cpp`,
`portal_file_chooser_test.cpp`, `mock_portal.cpp` to the existing
`viewer-smoke` executable's source list; wrap its `add_test()` in
`dbus-run-session` (§8.1).

Documentation to update (not edited here, per instructions — listed for
implementation):
- `packaging/Dockerfile.ci`: add `wayland-protocols` to the `pacman -S` list;
  verify whether `wayland` (scanner) needs an explicit entry rather than
  relying on a transitive pull from `qt6-wayland`/`qt6-declarative`. `dbus`
  is already listed.
- `README.md`: extend the "Requires ..." line with wayland-client,
  wayland-protocols, wayland-scanner, Qt DBus.
- `docs/holonight-qt-dlg-delegation-missed.md`: append the SPEC-mandated
  post-implementation note (portal is now primary; xdg-foreign for parent
  association; Qt FileDialog fallback only on unavailable/error; provider
  delegation still deferred).
- `docs/sdd/release-readiness/VERIFICATION.md`: add the N1 checklist and
  recorded Qt/Hyprland/portal/backend versions (REQ-NF-010/011).

## 5. State machine

`Idle → Exporting → Calling → AwaitingResponse → {Idle | FallbackShown} → Idle`.

| State | Entered by | Left by | Notes |
|---|---|---|---|
| Idle | ctor; end of request | `requestOpen()` | no-op if called again while not Idle (concurrent-request Non-Goal, REQ-F-020) |
| Exporting | `requestOpen()` | synchronous return | not externally observable — completes within one call stack (§3) |
| Calling | export attempt done | `OpenFile` watcher `finished` | Response subscribed on the *predicted* path before the async call is issued |
| AwaitingResponse | `OpenFile` returned a handle | `Request.Response` | no timeout (REQ-F-023); re-subscribe here if returned handle ≠ predicted (REQ-F-011) |
| FallbackShown | call failure/timeout | `fallbackAccepted`/`fallbackRejected` | portal not retried |

Event handling:
- **Response code 0**, in `AwaitingResponse`: `finishRequest()`, emit
  `finished(urls)` → Idle. Validity (exactly one local `file://` URL) is
  decided by `ImageDocument::open()`; no fallback either way (REQ-F-012/013).
  Other states/paths: ignored (REQ-F-022).
- **Response code 1:** `finishRequest()`, emit `cancelled()` → Idle;
  `PortalFileChooser` never touches document/zoom/orientation itself, so
  cancellation changes nothing (REQ-F-014).
- **Response code 2 or any other non-zero code:** handled as code 1
  (REQ-F-015, revised during T-013). xdg-desktop-portal-gtk sends code 2 for
  Escape and window close; portal unavailability is detected by the call
  failure below instead.
- **`OpenFile` call error** (ServiceUnknown/UnknownInterface/UnknownMethod/any
  `QDBusError`/25 s timeout): `finishRequest()`, `qCWarning(...)`
  (REQ-F-016), → `FallbackShown`.
- **Response on a non-active path, or while Idle:** ignored (REQ-F-022) —
  checked before acting.
- **Window close / object destruction:**
  - Trigger: `QQuickWindow::closing`, assigned-window `QObject::destroyed`, and
    the chooser destructor (backstop). Disconnect both window connections when
    replacing the assigned window.
    Closing happens while the event loop and bus connection are still alive;
    a destructor-only, fire-and-forget message queued during engine teardown
    at process exit may never be written to the socket.
  - `AwaitingResponse` (handle known): `closeActiveRequest()` issues
    `Request.Close` as a `QDBus::Block` call with a short
    timeout (500 ms) so the message is on the wire before exit,
    then drops the export — REQ-F-021, the state the acceptance test drives.
  - `Calling` (call issued, handle unknown): mark the request abandoned. If
    the `OpenFile` reply arrives while the process is still running, send
    `Request.Close` to the returned handle and ignore its Response. If the
    process exits first, nothing can be closed. **Known risk** (§7) — the
    portal notices the caller's unique name vanish from the bus.
  - `Exporting` cannot overlap a close (synchronous). `FallbackShown`: no
    portal request is outstanding.
  - A `Request.Response` arriving after destruction cannot call back: Qt
    auto-disconnects signal connections when either endpoint `QObject` dies
    (REQ-F-021's "never open a file from a later Response").

## 6. D-Bus details

```cpp
QDBusMessage call = QDBusMessage::createMethodCall(
    "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
    "org.freedesktop.portal.FileChooser", "OpenFile");
call << parentWindow << tr("Open image") << options;   // options: QVariantMap
auto pending = QDBusConnection::sessionBus().asyncCall(call, m_callTimeoutMs);
m_watcher = new QDBusPendingCallWatcher(pending, this);
connect(m_watcher, &QDBusPendingCallWatcher::finished, this, &PortalFileChooser::onOpenFileFinished);
```

`options` keys: `handle_token` (s), `modal`=true/`multiple`=false (b, b —
REQ-F-007), `filters` (`QVariant::fromValue(filters)`, REQ-F-003),
`current_filter` (`QVariant::fromValue(filters.value(0))`, REQ-F-004),
`current_folder` (ay, omitted when `currentFolderBytes()` is empty,
REQ-F-005).

**Comment placed at this call site** (REQ-NF-005): the `Request.Response`
subscription on the *predicted* path is made before this `asyncCall`,
because the portal can in principle emit `Response` before the method reply
reaches us — subscribing only after seeing the returned handle would race it.

Subscription: `m_handleToken = newHandleToken(); const auto sender =
QDBusConnection::sessionBus().baseService(); const auto predicted =
predictedRequestPath(sender, m_handleToken);` then
`QDBusConnection::connect("org.freedesktop.portal.Desktop", predicted,
"org.freedesktop.portal.Request", "Response", this, SLOT(onResponse(uint,
QVariantMap)))`. In `onOpenFileFinished`, `QDBusPendingReply<QDBusObjectPath>
reply(*watcher)`: on error → `FallbackShown`; on success, if
`reply.value().path() != predicted`, unsubscribe and re-subscribe to the
returned path (REQ-F-011), then → `AwaitingResponse`.

`registerPortalMetaTypes()` (called once from the constructor) registers
`PortalFileFilter`/`QList<PortalFileFilter>` via `qDBusRegisterMetaType<T>()`
so they marshal as `a(sa(us))`/`(sa(us))` automatically.

## 7. Known risks (beyond SPEC's own list)

- **`Qt6::GuiPrivate` ABI pin** (§3) — accepted, same class of risk the
  project already carries for holonight-qt's private-Qt use.
- **`Calling`-state window close** (§5) — `Request.Close` is sent late if
  the reply arrives before exit; otherwise only bus disconnection closes the
  request, not verified against every backend — flag alongside REQ-NF-010's
  manual pass.
- **Running `viewer-smoke` outside ctest** on a desktop session puts the tests
  on the real session bus: the mock cannot claim
  `org.freedesktop.portal.Desktop` and the smoke fallback test would open the
  real picker. Mitigation: the mock fixture, and the smoke test, fail fast
  with an explicit message when the name is already owned by another
  connection (i.e. not running under `dbus-run-session`).
- **Bounded Wayland roundtrip** (§3) — no app-level timeout on a stalled
  compositor; accepted as no worse than today's UI-blocking-on-compositor-
  stall behavior.
- **`qt6_generate_wayland_protocol_client_sources` CMake wiring** — left
  unverified since not needed by the chosen design; re-verify before
  adopting it for a second protocol.

## 8. Test architecture

### 8.1 Private D-Bus bus

**Decision:** wrap the `viewer-smoke` ctest command in `dbus-run-session`,
mirroring the existing `xvfb-run -a` wrapper pattern already used for the
separator tests in `tests/CMakeLists.txt`:

```cmake
find_program(VIEWER_DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(NAME viewer-smoke COMMAND "${VIEWER_DBUS_RUN_SESSION}" -- $<TARGET_FILE:viewer-smoke> ...)
```

This gives every `TEST()` in the one `viewer-smoke` binary a bus with no
portal registered by default (REQ-NF-006/NF-008 hold because nothing on a
freshly-spawned bus claims `org.freedesktop.portal.Desktop` unless a test's
own fixture does). `dbus-run-session` ships in the `dbus` package already
listed in `packaging/Dockerfile.ci`.

Rejected: spawning `dbus-daemon --print-address` from a GTest fixture and
setting `DBUS_SESSION_BUS_ADDRESS` before `QGuiApplication`. Rejected because
`viewer-smoke` is one process/one `QGuiApplication` shared across many
`TEST()`s in many files; a bus spawned in `main()` would also need process
lifetime management and cleanup, which `dbus-run-session` already provides.
The separator tests keep their `xvfb-run` wrapper unchanged; they do not open
the Open flow and need no private bus.

### 8.2 Mock portal (`tests/mock_portal.h/.cpp`)

GTest-fixture-scoped (`SetUp`/`TearDown`, returning
`::testing::AssertionResult`, per the repo's fixture-tidy convention):
registers `org.freedesktop.portal.Desktop`, records each `OpenFile` call
(`parent_window`, `title`, `options`) and `Request.Close`, and scripts
replies (code 0/1/2 with `results`, a `QDBusError`, or "never reply" for the
timeout test).

**Decision:** the mock uses its own named `QDBusConnection::connectToBus
(SessionBus, "mockPortalConnection")` rather than the test process's default
`QDBusConnection::sessionBus()`. Both reach the same private bus, but get
distinct unique names, so `Request.Response` delivery and the
sender-derived predicted-path formula (§2.1) exercise real cross-connection
routing instead of a same-process shortcut that would mask a bug in how the
client tells its own unique name apart from the mock's.

### 8.3 Test files

| File | Covers |
|---|---|
| `tests/portal_request_builder_test.cpp` | filter parsing (incl. empty); `current_folder` bytes/NUL-termination/omission; `formatParentWindow`; sender sanitizing/path prediction; distinct handle tokens |
| `tests/portal_file_chooser_test.cpp` | one `OpenFile` call on the right path/interface, no `impl.portal.*`; Ctrl+O drives it; `modal`/`multiple` always true/false; response-before-call-return race; differing-handle re-subscribe + stale-path ignore; Unicode/space/percent fixture → `Ready`; empty/non-file uri → error, no dialog; cancel → no document/zoom/focus change, shortcuts resume; code 2 → cancellation, no `openDialog`; missing service and call-error → fallback once; scripted no-reply + short timeout → fallback; second Open while outstanding ignored; window close while `AwaitingResponse` → `Close` recorded, late Response ignored; offscreen → `parent_window==""`, no warning |
| `tests/smoke.cpp` (`Viewer.OpeningCanvasAndAdapters`, unchanged) | no mock started → fallback `openDialog` appears → double-click delegate → `Ready` (REQ-NF-008) |

No new QML files are introduced, so the `viewer-qml-file-tooling-lists`
memory item (CMake/format-script/Taskfile entries for new QML) does not
apply; only `.cpp`/`.h` additions to two existing `SOURCES` lists.
`task tidy`'s `file(GLOB VIEWER_CXX ...)` already covers new files under
`apps/viewer/`/`tests/` with no Taskfile change.

## 9. Traceability

| REQ | Component(s) | Test(s) |
|---|---|---|
| REQ-F-001 | `PortalFileChooser` fixed service/path/interface | one-call test |
| REQ-F-002 | `Main.qml onDialogRequestedChanged` → `requestOpen()` | Ctrl+O test; code review (menu/empty-state share `dialogRequested`) |
| REQ-F-003 | `PortalRequestBuilder::buildPortalFilters` | `portal_request_builder_test.cpp` |
| REQ-F-004 | options assembly (`filters.value(0)`) | `portal_file_chooser_test.cpp` |
| REQ-F-005 | `PortalRequestBuilder::currentFolderBytes` | both test files |
| REQ-F-006 | `newHandleToken`; `tr("Open image")` | both test files |
| REQ-F-007 | options assembly | `portal_file_chooser_test.cpp` |
| REQ-F-008 | `WaylandForeignExport(er)`; `formatParentWindow` | `portal_request_builder_test.cpp`; manual REQ-NF-011 |
| REQ-F-009 | `exportWindow` empty-return paths | offscreen test + `WarningCollector` |
| REQ-F-010 | `subscribeResponse` before `asyncCall` | pre-subscription race test |
| REQ-F-011 | `onOpenFileFinished` re-subscribe | differing-handle test |
| REQ-F-012 | `onResponse` code-0 → `finished`; `ImageDocument::open` | Unicode/space/percent fixture test |
| REQ-F-013 | `finished` → existing `ImageDocument::open` rejection | non-file/empty uri tests |
| REQ-F-014 | `onResponse` code-1; `Main.qml onCancelled` | cancellation test |
| REQ-F-015 | `onResponse` non-zero code → `cancelled()` | code-2 cancellation tests |
| REQ-F-016 | `onOpenFileFinished` error branch | missing-service, call-error tests |
| REQ-F-017 | `m_callTimeoutMs`/`setCallTimeoutMsForTesting` | scripted no-reply test |
| REQ-F-018 | `Main.qml` fallback `FileDialog` (unchanged) | `tests/smoke.cpp` |
| REQ-F-019 | `requestOpen` has no cached probe state | stop-then-reopen test |
| REQ-F-020 | non-Idle no-op; existing `modalActive`/shortcut guards | outstanding-request test; existing shortcut tests |
| REQ-F-021 | destructor `closeActiveRequest()` | window-close test |
| REQ-F-022 | `onResponse` path/state check | stale-response test |
| REQ-F-023 | `AwaitingResponse` has no timer | never-responds test |
| REQ-F-024 | no new `Accessible`/`announce()` | code review; `accessibility_test.cpp` unchanged |
| REQ-NF-001 | CMake `REQUIRED` finds (§4) | CI configure check |
| REQ-NF-002 | `WaylandForeignExporter` platform check | offscreen test |
| REQ-NF-003 | `asyncCall` + `QDBusPendingCallWatcher` | manual responsiveness check |
| REQ-NF-004 | clang-format/clang-tidy over new files | `task format-check`, `task tidy` |
| REQ-NF-005 | comments at §6/§5 call sites | code review |
| REQ-NF-006 | `MockPortal` | all mock-backed cases |
| REQ-NF-007 | offscreen empty `parent_window` | `portal_file_chooser_test.cpp` |
| REQ-NF-008 | `dbus-run-session` wrapper | `tests/smoke.cpp` |
| REQ-NF-009 | `PortalRequestBuilder` pure functions | `portal_request_builder_test.cpp` |
| REQ-NF-010 | manual Hyprland pass | `VERIFICATION.md` (post-implementation) |
| REQ-NF-011 | export/exporter lifetime | manual `WAYLAND_DEBUG=1` (post-implementation) |
| REQ-C-001 | C++23, no X11/XCB includes | `git grep -i -E 'x11\|xcb'` (CI check) |
| REQ-C-002 | new files under `apps/`/`tests/`, generated code under `build/` | code review |
| REQ-C-003 | `add_custom_command` wayland-scanner invocation (§4) | configure/build check |
| REQ-C-004 | `Loader{active: portalFileChooser.fallbackShown}` | "no openDialog until fallback" test |
| REQ-C-005 | `dialogRequested` reused unchanged into `modalActive` | existing menu/shortcut tests unchanged |

## 10. SPEC feedback

No requirement is believed infeasible. Two wording notes for implementation
review, not blocking:

- REQ-F-010's sender is derived from `QDBusConnection::baseService()`
  (e.g. `:1.42`) via `sanitizeSenderForPath` (→ `1_42`).
- REQ-F-021: `Request.Close` is guaranteed only once a handle has been
  returned; the no-handle-yet case is best-effort (§5/§7). The SPEC's
  acceptance test already drives the handle-known case.

## 11. Review fixes (2026-09-16)

REQ-F-014/015: Main.qml remembers the active focus item when an Open request
starts. Cancellation clears modal state, then restores that item if it still
exists, is visible and enabled, and no other modal UI is active. Selection keeps
the existing neutral-focus behavior. The saved item is cleared when the flow ends.

REQ-F-021/022: destruction of the assigned window uses the same cancellation
path as closing, even when the chooser survives. While Calling, the abandoned
watcher closes the eventual returned handle; while AwaitingResponse, cancellation
closes the known handle immediately. Both paths unsubscribe and release the export.
