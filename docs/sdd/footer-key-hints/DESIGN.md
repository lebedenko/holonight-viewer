# HoloNight Viewer Footer Key Hints — Design

Companion to `docs/sdd/footer-key-hints/SPEC.md`. Line refs are to
`apps/viewer/Main.qml` on the pre-change tree unless noted.

The component extraction below records the original implementation. The
current footer uses an `Item` with a centered `Row`: it stays on one line,
hides a suffix of hints as width decreases, and always shows Help. The
current layout and its tests are described by REQ-F-005 in the companion spec.

## 1. Components

### Where the footer lives

Today it's inline: `Main.qml:807-822`, a `Flow` inside the root
`ColumnLayout` (442), sibling to `HnHeaderBar` (447). No `objectName`; its
`Repeater` model is a flat array of nine pre-joined `qsTr()` strings (814),
one bare `HnLabel` per string (815-820).

**Decision: extract `apps/viewer/FooterKeyHints.qml`**, added to
`apps/viewer/CMakeLists.txt`: `QML_FILES Main.qml FooterKeyHints.qml`.

Rationale:
- REQ-C-002 explicitly permits "an optional new footer QML file registered
  in `apps/viewer/CMakeLists.txt`" while keeping the rest of the diff
  footer-only — a component is the smallest change that satisfies this.
- **Testability for the 400 px check (REQ-F-005).** `Main.qml:21-22` sets
  `minimumWidth: 420` on `HnApplicationWindow`; a real `QQuickWindow` can't
  be resized below that via `resize()`. A standalone `FooterKeyHints.qml`
  has no such floor — a test can instantiate it directly (same
  `QQmlComponent`/`loadFromModule` pattern as `tests/smoke.cpp:328-334` and
  `tests/accessibility_test.cpp:34-36`) and set `width: 400` on the root.
- Matches project convention: every other testable concern (canvas, header
  buttons, details strip) is a named component; an anonymous `Flow` is the
  outlier.
- Both files sit in the same directory, so `Main.qml` sees `FooterKeyHints`
  via QML's implicit directory import — no new `import` line needed.

Main.qml's footer block (807-822) becomes:

```qml
FooterKeyHints {
    Layout.fillWidth: true
    Layout.leftMargin: 12
    Layout.rightMargin: 12
    Layout.bottomMargin: 6
}
```

`objectName: "footer"` is set inside `FooterKeyHints.qml` itself (not at the
call site), so a test loading the component standalone gets the same name
as one loading the full window.

### Hint delegate structure

```qml
Repeater {
    model: root.hints
    delegate: Row {
        id: hintRow
        required property var modelData
        objectName: "footerHint" + hintRow.modelData.name
        spacing: HnMetrics.internalSpacing(HnControlSize.Compact)   // 4px, intra-hint
        Accessible.role: Accessible.StaticText
        Accessible.name: hintRow.modelData.key + " " + hintRow.modelData.label

        HnKeyHint {
            objectName: hintRow.objectName + "Keycap"
            text: hintRow.modelData.key
            anchors.verticalCenter: parent.verticalCenter
            Accessible.ignored: true
        }
        HnLabel {
            objectName: hintRow.objectName + "Label"
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textMuted
            rawText: hintRow.modelData.label
            anchors.verticalCenter: parent.verticalCenter
            Accessible.ignored: true
        }
    }
}
```

`Row`, not `RowLayout` or an extra wrapper `Item`:
- `Row` only positions children along `x`, leaving `y` alone, so each child
  can freely bind `anchors.verticalCenter: parent.verticalCenter` (Row only
  rejects anchors on the axis it manages) — an exact 0 px center match,
  comfortably inside REQ-F-002's ≤1 px tolerance.
- Precedent already in the file: `headerActions` (`Main.qml:458`) is a plain
  `Row` used the same way for a tight, fixed pair of controls.
- `Row` extends `Item`, so it carries `Accessible.role`/`name` directly.

Each `Row` is one indivisible `Flow` child, so wrapping can never split a
keycap from its label (REQ-F-005).

### objectNames

| Element | objectName |
|---|---|
| Root `Flow` | `footer` |
| Per-hint `Row` | `footerHintNavigate`, `footerHintZoom`, `footerHintFit`, `footerHintActualSize`, `footerHintRotate`, `footerHintFullscreen`, `footerHintHelp` |
| Keycap | `<hint objectName>Keycap` |
| Label | `<hint objectName>Label` |

The `name` field (`navigate`, `zoom`, …) is a stable, non-translated id kept
alongside the translatable `key`/`label` so objectNames don't depend on
locale.

### Spacing tokens

- Intra-hint: `HnMetrics.internalSpacing(HnControlSize.Compact)` = 4 px
  (`holonight-qt/palette/holonight/metrics.h:32`), the same token already
  used for compact-control spacing elsewhere in `Main.qml` (e.g. line 34).
- Inter-hint: unchanged literal `Flow.spacing: 12` (`Main.qml:812`).
  REQ-F-006 bans color/font literals, not spacing literals — the file
  already uses bare numbers for `Layout.*Margin` (809-811) and `spacing`
  (812), so this is consistent with existing style. 4 < 12 satisfies
  REQ-F-002's "inter-hint spacing greater than intra-hint spacing".
- No generic "layout spacing" token exists beyond `internalSpacing(size)`
  and the header-specific `appTitleIconSpacing`/`appTitleTextSpacing`, which
  are semantically tied to the app title and not reused here.

## 2. Data model

A plain JS array of objects, not a `ListModel`:

```qml
readonly property var hints: [
    { name: "navigate",   key: qsTr("[ / ]"),    label: qsTr("Navigate") },
    { name: "zoom",       key: qsTr("Ctrl++/−"), label: qsTr("Zoom") },
    { name: "fit",        key: qsTr("Ctrl+0"),   label: qsTr("Fit") },
    { name: "actualSize", key: qsTr("1"),        label: qsTr("100%") },
    { name: "rotate",     key: qsTr("R"),        label: qsTr("Rotate") },
    { name: "fullscreen", key: qsTr("F"),        label: qsTr("Fullscreen") },
    { name: "help",       key: qsTr("?"),        label: qsTr("Help") },
]
```

**Why not `ListModel`:** `ListElement` property values must be compile-time
literals; a function call like `qsTr(...)` is not a legal `ListElement`
value. Since REQ-F-004 requires every string wrapped in `qsTr()`,
`ListModel` can't satisfy it without hand-rolled retranslation (repopulating
on `Qt.application.LanguageChange`). A plain array bound to a property is an
ordinary QML binding, so Qt's retranslation pass re-evaluates it and re-runs
`qsTr()` on locale change, same as the array literal being replaced
(`Main.qml:814`). It's also the smaller diff — same shape, split into
`{name, key, label}` per entry.

`Repeater.model` binds to `root.hints`; each delegate gets one object via
`required property var modelData`.

## 3. Data flow / behaviour

- **Persistent visibility (REQ-NF-001).** Neither the `Flow` nor any hint
  `Row` gets a `visible:` binding, an `opacity` animation, or a reference to
  `arrowTimer`/`detailsTimer` (`Main.qml:60-67`) — unlike
  `previousButton`/`nextButton` (689, 702) and `detailsStrip` (716), which
  are intentionally timer-gated. This preserves current behaviour rather
  than adding new logic.
- **Wrapping.** Unchanged `Flow` mechanism; the only change is that each
  wrapped unit is a `Row` (keycap+label) instead of one `HnLabel`, which is
  what makes "never split a pair" automatic.
- **Vertical centering.** Per-hint via `anchors.verticalCenter`, as in §1;
  no cross-hint alignment needed since `Flow` doesn't stretch rows to a
  common height.
- **Focus.** `HnKeyHint` (`holonight-qt/qml/controls/HnKeyHint.qml:7-19`) is
  a bare `Control` with no `focusPolicy`/`activeFocusOnTab` override.
  Confirmed empirically (`qml6`, offscreen, against
  `QtQuick.Controls.Basic.Control`): default `focusPolicy` is `Qt.NoFocus`
  (0), default `activeFocusOnTab` is `false`. **No explicit override is
  needed** for REQ-NF-003 — both `HnKeyHint` and `HnLabel` (a `Label`) are
  already non-focusable, and the existing Tab-cycle test
  (`tests/accessibility_test.cpp:127-217`) is unaffected since the footer
  was never in that chain. §4 asserts this invariant explicitly rather than
  trusting it silently.
- **Accessibility tree shape (REQ-NF-002).** Two defaults work against "one
  StaticText node per hint" and must be overridden:
  - `HnKeyHint` sets its own `Accessible.role: StaticText` /
    `Accessible.name: root.text` (`HnKeyHint.qml:18-19`) — unchanged, every
    keycap would surface as its own node named just the key (e.g.
    `"Ctrl+0"`), which REQ-NF-002 forbids.
  - `Label`-derived `HnLabel` similarly defaults to its own StaticText
    exposure.
  - Both children get `Accessible.ignored: true` (§1) so only the
    containing `Row` is exposed, named `"<key> <label>"`.

## 4. Test design

**New file `tests/footer_key_hints_test.cpp`**, added to `viewer-smoke` in
`tests/CMakeLists.txt`'s source list alongside the other per-concern files
(`folder_browsing_test.cpp`, `view_geometry_test.cpp`,
`exif_metadata_test.cpp`, …) rather than growing
`accessibility_test.cpp`/`static_workflow_test.cpp` (already 217/558
lines).

- **`ContentOrderAndCount`** (REQ-F-001, -003, -004). Load `Main` via
  `loadFromModule` (as `accessibility_test.cpp:34-36`). Find `footer`;
  assert exactly 7 `footerHint*` children in the known objectName order.
  Compare each keycap `text` and label `rawText` against the REQ-F-003/001
  tables. Assert no descendant text equals `"Q"`, `"Quit"`, `"I"`, or
  `"Information"`. REQ-F-004 ("wrapped in `qsTr()`") is a source-shape
  requirement, not runtime-observable under the default locale — verified
  by reading `FooterKeyHints.qml` (every literal passes through `qsTr(...)`,
  same as verified today for `Main.qml:814`); no `QTranslator` fixture
  exists in this suite and none is added solely for this check.
- **`KeycapLabelPairingAndSpacing`** (REQ-F-002). Per hint: `label->x() >
  keycap->x()`; `|keycap centerY − label centerY| ≤ 1 px`. Read
  `Flow.spacing` and one hint `Row.spacing` via `QQmlProperty::read` and
  assert `12 > 4`.
- **`NarrowWidthWraps`** (REQ-F-005, the 400 px check). Don't resize the
  real window (blocked by `minimumWidth: 420`). Instead
  `engine.loadFromModule("HolonightViewer", "FooterKeyHints")` standalone
  (mirrors how `accessibility_test.cpp` loads `Main`), set
  `item->setWidth(400)`, process one event loop turn. Assert every hint
  `x()+width() <= 400`; at least two distinct hint `y()` values; each
  keycap/label pair shares a row (same 1 px tolerance as above).
- **`PersistentVisibility`** (REQ-NF-001). Load the full window; `footer`
  visible in the empty state and after `sample.png` loads
  (`accessibility_test.cpp:105-106` pattern). For "survives mouse movement +
  wait" without a real 6 s sleep, reuse the project's existing pattern for
  these exact timers (`image_inspection_test.cpp:287-288`): shorten
  `arrowTimer`/`detailsTimer` intervals via `setProperty("interval", …)`,
  `QTest::mouseMove` to start them, `qWaitFor` until idle, assert `footer`
  stays visible throughout. The footer isn't wired to either timer at all
  (§3), so the assertion's strength doesn't depend on the wait's real
  duration — a literal `QTest::qWait(6000)` would test the same thing more
  slowly. The "no timer/opacity bound" half of REQ-NF-001 is otherwise
  enforced by code review of `FooterKeyHints.qml`.
- **`AccessibleNaming`** (REQ-NF-002). `QAccessible::setActive(true)`
  (as `accessibility_test.cpp:32`). For each hint, query the interface on
  the `Row`: `role() == QAccessible::StaticText`,
  `text(QAccessible::Name) == "<key> <label>"`. Walk the keycap/label
  children and assert none produce a non-empty accessible `Name` (handle
  both `nullptr` and "non-null, empty name" as pass — `Accessible.ignored`
  behavior can differ slightly by Qt version).
- **`NotFocusable`** (REQ-NF-003). Per hint and its children:
  `activeFocusOnTab` false/absent; where present (`HnKeyHint` is a
  `Control`), `QQmlProperty::read(item, "focusPolicy").toInt() == 0`.
  Re-run the existing Tab-cycle assertions
  (`accessibility_test.cpp:127-217`) unchanged — no footer objectName should
  ever become `window->activeFocusItem()` (REQ-C-001).

## 5. Visual verification

- **`scripts/check-visual.sh`**: runs `viewer-smoke` offscreen with
  `VIEWER_CAPTURE_PREFIX` set, across dark/light × 4 scale factors, against
  a fixed `--gtest_filter` list of tests that call `grabWindow()` when the
  prefix is set (pattern at `accessibility_test.cpp:155-164`,
  `static_workflow_test.cpp:344-363`). Add a capture-oriented footer test
  (e.g. `FooterKeyHints.VisualCapture`, guarded by
  `qEnvironmentVariable("VIEWER_CAPTURE_PREFIX")` like
  `static_workflow_test.cpp:344`) to that filter list, capturing the full
  window at the sizes already used there (420×280, 1000×700) plus a
  standalone 400 px-wide `FooterKeyHints` grab for SPEC's literal narrow
  scenario (real window can't go below 420 — §1).
- **`scripts/screenshot.py`**: captures a live Hyprland window via `grim`
  after actually running `hn-viewer` — a human sanity check (real GPU
  rendering, font hinting), not part of the offscreen suite. Recommended
  one-off: `scripts/screenshot.py --delay 1 --output build/footer-default.png`
  at default size, then again after manually narrowing the window, to
  eyeball keycap/label contrast and wrap before trusting the automated
  `grabWindow()` captures alone.

## 6. Key decisions, alternatives, risks

| # | Decision | Alternative | Why rejected |
|---|---|---|---|
| 1 | New `FooterKeyHints.qml` | Keep inline in `Main.qml` | Blocks the 400 px test (`minimumWidth: 420`); inline also means 4 nested elements per hint instead of 1 |
| 2 | `Row` per hint | `RowLayout` | More ceremony than a fixed 2-child pair needs; `headerActions` (`Main.qml:458`) already sets the plain-`Row` precedent |
| 3 | JS array of `{name,key,label}` | `ListModel` | `ListElement` values must be literals; `qsTr()` calls aren't legal there |
| 4 | Explicit `Accessible.ignored: true` on both children | Rely on defaults | Both control types self-expose as StaticText by default; unchanged, each hint would surface 3 nodes instead of 1 |
| 5 | No explicit `focusPolicy` override | Defensive `focusPolicy: Qt.NoFocus` on every `HnKeyHint` | Confirmed `Control`'s default is already `NoFocus`/non-tab-focusable; a redundant override would mask (not test) a future regression, so a pinning test is used instead |
| 6 | Shortened timers instead of literal 6 s wait | `QTest::qWait(6000)` | Matches existing pattern (`image_inspection_test.cpp:287-288`); footer isn't wired to either timer, so wait duration doesn't add confidence |
| 7 | README unchanged | Rewrite `README.md:78`'s footer sentence to enumerate the seven hints | That sentence never listed hint content/count, only that the footer wraps and stays visible — still true. Revisit if reviewers want the Quit/Information removal called out |

**Risks:**
- **Height growth (REQ-C-001).** `HnKeyHint`'s padding (`padding: 4`,
  `leftPadding`/`rightPadding: 6`) and monospace font make each hint taller
  than the current bare Caption label, growing `Flow.implicitHeight` and the
  `ColumnLayout`'s total height. No existing test asserts an exact footer or
  window height for this row, so this should be a no-op per REQ-C-001's
  carve-out — but re-run the full suite after implementation in case some
  other geometry assertion (e.g. `detailsStrip`) implicitly assumed the old
  footer height.
- **Standalone-component load path.** Prefer
  `QQmlApplicationEngine::loadFromModule("HolonightViewer", "FooterKeyHints")`
  over a hand-written `qrc:/qt/qml/...` URL, to avoid coupling the 400 px
  test to `qt_add_qml_module`'s generated `OUTPUT_DIRECTORY` layout
  (`apps/viewer/CMakeLists.txt:7`).
- **Accessible-tree walk brittleness.** Whether an `Accessible.ignored` item
  returns `nullptr` or a non-null interface with an empty name can vary by
  Qt version; the `AccessibleNaming` test should accept either.
