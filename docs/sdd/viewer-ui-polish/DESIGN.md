# HoloNight Viewer UI Polish — DESIGN

Companion to `SPEC.md`. Grounded in `apps/viewer/Main.qml`,
`apps/viewer/ImageInformationPopup.qml`, `apps/viewer/FooterKeyHints.qml`,
`apps/viewer/image_canvas.h/.cpp`, `apps/viewer/window_key_router.h`, the
installed Qt Quick Templates headers (`qquickmenu_p_p.h`,
`qquickmenuseparator_p.h`), and the existing test suite
(`tests/image_inspection_test.cpp`, `tests/static_workflow_test.cpp`,
`tests/accessibility_test.cpp`, `tests/image_information_popup_test.cpp`).
Line numbers refer to the pre-change tree.

**Spec-conflict check (REQ-C-006):** none found. Every behavior this feature
needs — HUD reveal on zoom/fit/rotate/flip, arrow hover-pause, the `X` /
`Shift+X` remap — is reachable by consuming signals `ImageCanvas` already
emits (`viewChanged`, `firstRendered`, `mouseMoved`, `keyboardInput`) and by
changing `Action.shortcut` strings in QML. `WindowKeyRouter` needs no change:
its `eventFilter` only special-cases Tab/Backtab, J/K-while-menu-open, and
arrow keys while `imageReady`; it has no H/V/X handling to unwire (matches
the spec's own pre-check). This design touches **zero** production `.cpp`/`.h`
files; the diff is `apps/viewer/*.qml` (existing + one new file), test
`.cpp` files, and the four tooling registries (CMake, format script,
Taskfile, and the `viewer-smoke` test source list).

---

## 1. Components

### QML — new

| File | Responsibility |
|---|---|
| `apps/viewer/ShortcutHelpPopup.qml` | Modal `Basic.Popup` replacing the `detailsDialog` `Basic.Dialog`. Fixed header ("Shortcuts" title + × close), scrolling body of six labeled sections, each a repeated list of `{key, description, keycap}` rows. Exposes its content model as a plain QML property (`sections`) so tests can assert row content without walking a `Repeater` (project memory: Repeater delegates aren't `findChild`-reachable). Styled identically to `ImageInformationPopup.qml` (same background, border, dimmer alpha, close-button recipe) — the two are now visually one family instead of "styled popup" + "plain `Basic.Dialog`". |

### QML — changed

| File | Change |
|---|---|
| `apps/viewer/Main.qml` | (a) `ViewerMenuItem` inline component gains a `shortcutText` property and a two-label `RowLayout` content item (label left, shortcut caption right). (b) The `actionsMenu` item list is reordered per REQ-F-005 with six `Basic.MenuSeparator` items inserted; each `ViewerMenuItem` gets a literal `shortcutText`; `zoomOutButton`/`zoomInButton` labels become "Zoom Out"/"Zoom In"; `actionsButton`'s `Accessible.name` becomes `qsTr("Menu")`. (c) `flipHorizontal`/`flipVertical` `Action.shortcut` change from `"H"`/`"V"` to `"X"`/`"Shift+X"`. (d) `detailDialog: int` is replaced by `helpOpen: bool`; the `detailLoader`/`Basic.Dialog` block is deleted and replaced by a `Loader` instantiating `ShortcutHelpPopup`, mirroring the existing `informationLoader`. (e) The empty-state glyph (`emptyState`) is wrapped in a new `Column` (`emptyStateGroup`) that also holds two new `HnLabel` hint lines, replacing the glyph's manual `x`/`y` centering with the Column's layout. (f) A small "transient controller" block (two booleans, two functions per transient element) is added near the existing `arrowTimer`/`detailsTimer` declarations; `previousButton`, `nextButton`, and `detailsStrip` gain `shown`/`opacity`/`Behavior`/`visible` wiring in place of their current `visible: <timer>.running` bindings; `canvas`'s `onMouseMoved`/`onKeyboardInput`/`onFirstRendered` handlers are rewired to call the controller functions, and a new `onViewChanged` handler is added. |

No other production file changes. `FooterKeyHints.qml` is explicitly **not**
touched (REQ-F-030); `ImageInformationPopup.qml` is explicitly **not**
touched (out of scope) — it is only read as the styling/structure precedent
for `ShortcutHelpPopup.qml`.

### Why no C++ changes are needed (grounding for the "single hook" question)

`apps/viewer/image_canvas.cpp` today:

```cpp
void ImageCanvas::refresh() { update(); emit viewChanged(); }   // L27-30
void ImageCanvas::setImage(const QImage& image) { ...; refresh(); emit imageChanged(); }        // L31-40
void ImageCanvas::setOrientation(int o) { ...; view_.fit(); refresh(); emit orientationChanged(); } // L41-50
void ImageCanvas::setDisplayPixelRatio(qreal r) { ...; refresh(); emit viewportChanged(); }      // L51-59 (no viewChanged)
void ImageCanvas::geometryChange(...) { if (size changed) { ...; refresh(); emit viewportChanged(); } } // L60-67 (no viewChanged directly... but refresh() DOES emit viewChanged)
void ImageCanvas::fit()  { view_.fit();  refresh(); }   // viewChanged
void ImageCanvas::actualSize() { view_.actualSize(); refresh(); } // viewChanged
void ImageCanvas::zoom(qreal f, QPointF a) { view_.zoom(f, a); refresh(); } // viewChanged (zoomSteps calls zoom())
void ImageCanvas::pan(QPointF d) { view_.pan(d); refresh(); } // viewChanged
```

**Correction on resize:** `geometryChange()` calls `refresh()` too, so a
resize *also* emits `viewChanged()` (via `refresh()`), not just
`viewportChanged()`. This matters: `viewChanged` alone is **not**
resize-safe, contradicting the naive reading of the spec's Open Question 5
("hook the viewport/orientation change… while not firing on resize"). The
correct signal is **not** a raw `viewChanged` hookup on its own — see the
gating rule in §2, which excludes resize by tying the reveal to a
`window`-level function called only from the explicit user-action call
sites (`zoomImage`, `fitImage`, `actualSizeImage`, `transformImage`) plus
`onFirstRendered`/`onMouseMoved`, and **not** from a blanket
`canvas.onViewChanged` handler. This is the key decision in §4.1.

---

## 2. Data flow

```
trigger source                          →  controller call            →  shown  →  opacity  →  visible
──────────────────────────────────────────────────────────────────────────────────────────────────────
pointer move over canvas (mouseMoved)   →  window.showArrows()        →  arrowsShown=true   (arrows)
                                         →  window.revealDetails()     →  detailsShown=true  (HUD, gated on Ready)
hover enter on previous/nextButton      →  window.pauseArrows()       →  arrowTimer.stop() (shown unchanged)
hover exit  on previous/nextButton      →  window.resumeArrows()      →  arrowTimer.restart() if arrowsShown
keyboard input on canvas (keyboardInput)→  window.hideArrows()        →  arrowsShown=false  (immediate)
arrowTimer 2s elapsed                   →  onTriggered                →  arrowsShown=false
detailsTimer 3s elapsed                 →  onTriggered                →  detailsShown=false
canvas.onFirstRendered                  →  window.revealDetails()     →  detailsShown=true  (gated on Ready)
zoomImage()/fitImage()/actualSizeImage()/transformImage() [keyboard, menu, mouse-wheel — see §2.1]
                                         →  window.revealDetails()     →  detailsShown=true  (gated on Ready)
document leaves Ready (onDocumentStateChanged / canvas.onImageChanged)
                                         →  detailsShown=false; detailsTimer.stop()  (immediate, REQ-F-026)
```

Every `shown` flip is a **synchronous property write**, so REQ-F-025's "same
frame" requirement is automatic — no binding indirection sits between the
trigger and the flag. `opacity` is a plain `shown ? 1 : 0` binding with a
150 ms `Behavior`; `visible: opacity > 0 || shown` keeps the item interactive
and accessible for the whole fade-out, then removes it once `opacity` settles
at 0 (REQ-F-021/-024).

### 2.1 Why `revealDetails()` is called from functions, not from `canvas.onViewChanged` directly

`canvas.onViewChanged` fires on: `setImage` (navigation), `setOrientation`
(rotate/flip via the `document.orientation` → `canvas.orientation` binding),
`fit()`, `actualSize()`, `zoom()`/`zoomSteps()`, `pan()`, **and** on resize
(`geometryChange` → `refresh()` → `viewChanged`, confirmed in §1). Wiring
`onViewChanged: window.revealDetails()` directly would violate REQ-F-023's
implicit "not on resize" requirement (stated explicitly for arrows in
REQ-F-018's framing and carried into the Open Questions for the HUD).

Instead, `revealDetails()` is called from the **five existing call sites**
that already fully cover REQ-F-023's trigger list, each of which is a
`window`-level function invoked identically whether triggered by keyboard
`Shortcut`, menu `Action.onTriggered`, or (for zoom) the `WheelHandler`:

```qml
function fitImage(): void { canvas.fit(); window.revealDetails(); window.clearImageFocus(); }
function actualSizeImage(): void { canvas.actualSize(); window.revealDetails(); window.clearImageFocus(); }
function zoomImage(steps: real): void { canvas.zoomSteps(steps, ...); window.revealDetails(); window.clearImageFocus(); }
function transformImage(operation: int): void { document.transform(operation); window.fitImage(); /* fitImage already reveals */ ++window.inputEpoch; }
function browse(direction: int): void { /* unchanged — reveal happens later, via onFirstRendered */ }
```

`transformImage()` (rotate/flip) already calls `window.fitImage()`, which
now reveals — no separate call needed there. `zoomImage`/`fitImage`/
`actualSizeImage` cover every keyboard and menu path for their canvas
methods. The one other caller is mouse-wheel zoom: `WheelHandler.onWheel`
calls `canvas.zoomSteps()` directly (not through `zoomImage()`), so it gets
an explicit `window.revealDetails()` call at that site. This gives full coverage of
"keyboard, menu, or mouse" (REQ-F-023) without ever touching `onViewChanged`,
so resize is structurally excluded — resize never calls any of these
functions.

Navigation is different on purpose: `browse()` does **not** call
`revealDetails()` — the HUD must wait for the *next* image's first paint
(REQ-F-023: "pressing `]` shows the HUD once the next image reaches Ready
(first render)"), which is exactly what `onFirstRendered` already provides,
unchanged from today's mechanism (`onFirstRendered: { rendered = true;
detailsTimer.restart(); }` → becomes `onFirstRendered: {rendered = true;
window.revealDetails();}`).

### 2.2 Ordering hazard: `setImage`'s internal `refresh()` vs `imageChanged`

`ImageDocument::complete()` sets `state_` and `image_` together, then emits
one `changed()` (image_document.cpp:388-410). QML's `canvas.image:
window.document.image` binding re-evaluates, calling `ImageCanvas::setImage`,
which internally calls `refresh()` (→ `viewChanged`, *not* wired to anything
in this design per §2.1) **before** it emits its own `imageChanged()` — which
*is* wired: `onImageChanged: { ++inputEpoch; rendered=false; detailsShown=false;
detailsTimer.stop(); }`. Because `revealDetails()` is never attached to
`viewChanged`, this ordering is a non-issue for this design (unlike an
earlier draft that considered hooking `viewChanged` directly — see §4.1's
rejected alternative). `detailsShown` therefore goes: (previous state) →
forced `false` on `imageChanged` → `true` once `firstRendered` fires
asynchronously after the new image paints. No premature reveal is possible.

---

## 3. Interfaces

### 3.1 New objectNames / properties / functions

| Name | Kind | Location | Notes |
|---|---|---|---|
| `shortcutHelpPopup` | objectName | `ShortcutHelpPopup` root | replaces `detailsDialog` |
| `shortcutHelpCloseButton` | objectName | close button inside popup | `Accessible.name: qsTr("Close Shortcut Help")` |
| `shortcutHelpContent` | objectName | popup `contentItem` (ColumnLayout) | carries `Accessible.role: Dialog`, `Accessible.name: qsTr("Shortcut Help")` |
| `shortcutHelpSection<Key>` | objectName | per-section `ColumnLayout` (Repeater delegate) | `Accessible.role: Grouping`, e.g. `shortcutHelpSectionNavigation` |
| `sections` | property `var` | `ShortcutHelpPopup.qml` root | the row model, §3.3 — read directly by tests (no `findChild` needed) |
| `window.helpOpen` | property `bool` | `Main.qml` | replaces `detailDialog: int`; `modalActive` becomes `dialogRequested \|\| informationOpen \|\| helpOpen` |
| `window.arrowsShown` | property `bool` | `Main.qml` | reveal target; `previousButton`/`nextButton` `shown` is `arrowsShown && document.count > 1` (REQ-F-022), while `playPauseButton` uses `arrowsShown` alone |
| `window.detailsShown` | property `bool` | `Main.qml` | drives `detailsStrip` `shown` |
| `window.showArrows()` | function | `Main.qml` | `arrowsShown = true`; restarts `arrowTimer` unless an arrow is hovered |
| `window.hideArrows()` | function | `Main.qml` | `arrowsShown = false`; stops `arrowTimer` (immediate, REQ-F-020) |
| `window.pauseArrows()` / `resumeArrows()` | functions | `Main.qml` | hover pause/resume, §4.2 |
| `window.revealDetails()` | function | `Main.qml` | no-ops unless `documentState === ImageDocument.Ready`; else `detailsShown = true; detailsTimer.restart()` |
| `previousButton.shown` / `nextButton.shown` / `detailsStrip.shown` | readonly property `bool` | `Main.qml` | REQ-F-025's observable state; each is a one-line alias to the window flag |
| `emptyStateGroup` | objectName | `Main.qml`, new `Column` wrapping the glyph + hint lines | not required by spec but needed as a layout anchor; no test depends on its name specifically |
| `emptyStateHintPrimary` / `emptyStateHintSecondary` | objectName | the two new `HnLabel`s | line 1 / line 2 of REQ-F-001 |

`emptyState` (the glyph), `actionsButton`, `actionsMenu`, `previousButton`,
`nextButton`, `detailsStrip`, `footer`, `informationPopup` keep their exact
objectNames (REQ-C-007).

### 3.2 `ViewerMenuItem` extension

```qml
component ViewerMenuItem: HnStyle.MenuItem {
    id: control
    property string shortcutText: ""
    contentItem: RowLayout {
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
        HnLabel {
            Layout.fillWidth: true
            rawText: control.text
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: control.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textDisabled
        }
        HnLabel {
            visible: control.shortcutText.length > 0
            role: HnTypographyRole.Caption
            rawText: control.shortcutText
            textFormat: Text.PlainText
            horizontalAlignment: Text.AlignRight
            color: control.enabled ? HoloniightPalette.textMuted : HoloniightPalette.textDisabled
        }
    }
    background: Rectangle { /* unchanged from today */ }
}
```

`actionsMenu.width` grows from `Math.min(340, window.width - 24)` to
`Math.min(380, window.width - 24)`: the longest label/shortcut pair,
"Rotate Counterclockwise" + "Shift+R", needs headroom the current 340px
budget was never asked to hold (today no item shows a shortcut caption at
all). 380 keeps the same `window.width - 24` floor used everywhere else in
this feature (popup, hint line 2) and stays comfortably under the 420
minimum-window budget (396).

### 3.3 Menu item order, labels and shortcut text (REQ-F-005/006/028)

| # | Label (`text`) | `shortcutText` | existing objectName / action |
|---|---|---|---|
| 1 | Open… | Ctrl+O | `openButton` |
| 2 | Refresh | Ctrl+R | *(new: was unnamed `text: "Refresh"`)* |
| — | *separator 1* | | |
| 3 | Previous | [ | *(unnamed today)* |
| 4 | Next | ] | *(unnamed today)* |
| — | *separator 2* | | |
| 5 | Fit | Ctrl+0 | `fitButton` |
| 6 | Actual Size | 1 | `actualSizeButton` |
| 7 | Zoom In | Ctrl++ | `zoomInButton` (label changes "Zoom in"→"Zoom In") |
| 8 | Zoom Out | Ctrl+− | `zoomOutButton` (label changes "Zoom out"→"Zoom Out") |
| — | *separator 3* | | |
| 9 | Rotate Clockwise | R | `action: rotateClockwise` |
| 10 | Rotate Counterclockwise | Shift+R | `action: rotateCounterclockwise` |
| 11 | Flip Horizontally | X | `action: flipHorizontal` (shortcut H→X) |
| 12 | Flip Vertically | Shift+X | `action: flipVertical` (shortcut V→Shift+X) |
| 13 | Reset Transform | *(empty — no caption shown)* | `resetTransformMenuItem` |
| — | *separator 4* | | |
| 14 | Copy Image | Ctrl+C | `action: copyImage` |
| 15 | Copy Path | Ctrl+Shift+C | `action: copyPath` |
| — | *separator 5* | | |
| 16 | Image Information | I | `informationMenuItem` |
| 17 | Shortcut Help | ? | `action: shortcutHelp` |
| — | *separator 6* | | |
| 18 | Fullscreen | F | *(unnamed today)* |
| 19 | Quit | Q | *(unnamed today)* |

`shortcutText` values are **literal `qsTr()`-wrapped strings**, not derived
from `action.shortcut.toString()` — see §4.3 for why. The `Ctrl+−` value
uses the same U+2212 minus glyph the footer already uses for `"Ctrl++/−"`
(`FooterKeyHints.qml:20`), so REQ-F-006's "menu, Help and footer spell
shared shortcuts identically" holds by construction, not by coincidence.

Existing objectNames that move position (`openButton`, `fitButton`,
`actualSizeButton`, `zoomInButton`, `zoomOutButton`, `resetTransformMenuItem`,
`informationMenuItem`) keep their objectName; only their index in
`actionsMenu.contentModel`/`currentIndex` changes (REQ-F-009's "adjusted
only for label text" — index shift is unavoidable and must be treated as
part of that same adjustment for any test reading `currentIndex` directly,
see §5's risk note).

### 3.4 Shortcut Help content model (`ShortcutHelpPopup.qml`)

```qml
readonly property var sections: [
    { key: "Navigation", label: qsTr("Navigation"), rows: [
        { key: qsTr("Ctrl+O"), description: qsTr("Open image"), keycap: true },
        { key: qsTr("[ / ]"), description: qsTr("Previous / next image"), keycap: true },
        { key: qsTr("Ctrl+R"), description: qsTr("Refresh folder and image"), keycap: true },
    ]},
    { key: "View", label: qsTr("View"), rows: [
        { key: qsTr("Ctrl+0"), description: qsTr("Fit"), keycap: true },
        { key: qsTr("1"), description: qsTr("Actual size"), keycap: true },
        { key: qsTr("Ctrl++ / Ctrl+−"), description: qsTr("Zoom"), keycap: true },
        { key: qsTr("F"), description: qsTr("Fullscreen"), keycap: true },
        { key: qsTr("Esc"), description: qsTr("Close dialog or leave fullscreen"), keycap: true },
    ]},
    { key: "Transform", label: qsTr("Transform"), rows: [
        { key: qsTr("R / Shift+R"), description: qsTr("Rotate clockwise/counterclockwise"), keycap: true },
        { key: qsTr("X / Shift+X"), description: qsTr("Flip horizontally/vertically"), keycap: true },
    ]},
    { key: "Image", label: qsTr("Image"), rows: [
        { key: qsTr("I"), description: qsTr("Image information"), keycap: true },
        { key: qsTr("Ctrl+C"), description: qsTr("Copy image"), keycap: true },
        { key: qsTr("Ctrl+Shift+C"), description: qsTr("Copy path"), keycap: true },
    ]},
    { key: "Application", label: qsTr("Application"), rows: [
        { key: qsTr("?"), description: qsTr("Toggle this help"), keycap: true },
        { key: qsTr("Q"), description: qsTr("Quit"), keycap: true },
    ]},
    { key: "Mouse", label: qsTr("Mouse"), rows: [
        { key: qsTr("Wheel/touchpad scroll"), description: qsTr("Zoom at pointer"), keycap: false },
        { key: qsTr("Left-drag"), description: qsTr("Pan"), keycap: false },
        { key: qsTr("Arrow keys"), description: qsTr("Pan"), keycap: true },
        { key: qsTr("Drop an image"), description: qsTr("Open image"), keycap: false },
    ]},
]
```

Section `key` is a stable, untranslated id (objectName suffix, like
`ImageInformationPopup`'s `Camera`/`Location`/`File`); `label` is the
translated display string, upper-cased only for the visible caption
(`SectionLabel { rawText: section.label.toUpperCase() }`) — the accessible
`Grouping` name stays title case (`"Navigation"`, not `"NAVIGATION"`),
matching REQ-F-017 and the same title-case/upper-case split
`ImageInformationPopup.qml` already established for its own sections.

**Wording note (design decision, not literal spec text):** REQ-F-013 gives
gesture bullets ("wheel/touchpad scroll zoom at pointer", "drop an image to
open") without an unambiguous key/description split. The table above is the
canonical split this design commits to; implementers must not improvise a
different one, since REQ-F-013's acceptance test does exact string
comparison against "the list above" (i.e., this table, which is this
design's interpretation of the spec bullets).

Total: 19 rows across 6 sections, matching REQ-F-012/013 exactly
(NAVIGATION 3, VIEW 5, TRANSFORM 2, IMAGE 3, APPLICATION 2, MOUSE 4).

### 3.5 Row rendering

```qml
Repeater {
    model: section.modelData.rows
    delegate: RowLayout {
        id: rowItem
        required property var modelData
        Layout.fillWidth: true
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
        Accessible.role: Accessible.StaticText
        Accessible.name: modelData.key + " " + modelData.description
        // Fixed key column so descriptions align across rows; capped so narrow cards keep room for text.
        Item {
            Layout.preferredWidth: Math.min(root.keyColumnWidth, scroll.availableWidth / 2)
            Layout.preferredHeight: rowItem.modelData.keycap ? keycap.implicitHeight : plainKey.implicitHeight
            HnKeyHint {
                id: keycap
                visible: rowItem.modelData.keycap
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width)
                text: rowItem.modelData.key
                Accessible.ignored: true
                contentItem: HnLabel {
                    role: HnTypographyRole.Code
                    rawText: keycap.text
                    font: keycap.font
                    color: HoloniightPalette.textSecondary
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    Accessible.ignored: true
                }
            }
            HnLabel {
                id: plainKey
                visible: !rowItem.modelData.keycap
                width: parent.width
                anchors.verticalCenter: parent.verticalCenter
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                rawText: rowItem.modelData.key
                color: HoloniightPalette.textSecondary
                Accessible.ignored: true
            }
        }
        HnLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap           // REQ-F-015: wrap, never overflow
            textFormat: Text.PlainText
            color: HoloniightPalette.textPrimary
            rawText: rowItem.modelData.description
            Accessible.ignored: true
        }
    }
}
```

Both key renderings are declared as siblings with `visible` toggles rather
than a `Loader` with inline components: inline components cannot see the
delegate's `rowItem` id, and two cheap items per row are simpler than
passing `modelData` through a `Loader`. `keyColumnWidth` is a root-level
constant (140), capped at half the scroll body's available width. This keeps
description columns aligned without depending on a particular theme font size.
Each keycap is constrained to that column; its themed code label wraps and its
implicit height expands the row. Width depends on the scroll viewport, not on
the row's assigned width, to avoid layout feedback. The review correction
replaces the earlier assumption that every shortcut fits within 140 px: the
repository's 12 pt theme renders the Zoom keycap at about 156 px.
The accessibility wiring retains the `FooterKeyHints.qml` single-row pattern;
both the keycap and its customized text content are ignored.

Each row is exposed exactly once, as `Accessible.StaticText` named
`"<key> <description>"` — e.g. `"Ctrl+O Open image"` — satisfying
REQ-F-017's "keycap internals ignored, same pattern as FooterKeyHints".

---

## 4. Key decisions, alternatives, risks

### 4.1 HUD reveal: call from window functions, not a blanket `canvas.onViewChanged`

**Decision:** wire `revealDetails()` into `zoomImage`/`fitImage`/
`actualSizeImage` (covering keyboard, menu, and — with one added call — mouse
wheel) plus the pre-existing `onFirstRendered` (navigation) and
`onMouseMoved` (pointer) hooks.

**Alternative considered (rejected):** `canvas.onViewChanged:
window.revealDetails()`. Rejected because — contrary to the spec's Open
Question 5 framing — `viewChanged` is **not** resize-safe:
`geometryChange()` calls `refresh()`, which emits `viewChanged` on every
resize (§1). A blanket hookup would show the HUD on every window resize,
directly violating the "not on resize" requirement implicit in REQ-F-023's
list (resize is conspicuously absent from the trigger list) and the arrow
timer's explicit precedent (arrows never react to resize either). Because
this is a *correction* of the spec's own suggested approach (not just a
choice among two valid options), it is called out here rather than only in
§2.1.

**Alternative considered (rejected):** hook `orientationChanged` +
`viewportChanged` separately instead of going through window functions.
Rejected: `viewportChanged` fires on resize too (`setDisplayPixelRatio` and
`geometryChange` both emit it), so it has the identical resize problem, and
using two signals plus the three window functions is strictly more surface
than using the three window functions alone (`zoomImage`/`fitImage`
already cover the orientation-via-transform path since `transformImage`
calls `fitImage`).

### 4.2 Arrow hover-pause: explicit `onHoveredChanged`, not a `Timer.running` binding

**Decision:** `previousButton`/`nextButton` each get
`onHoveredChanged: hovered ? window.pauseArrows() : window.resumeArrows()`.
`pauseArrows()` is `arrowTimer.stop()`; `resumeArrows()` is
`if (window.arrowsShown) arrowTimer.restart()`.

**Alternative considered (rejected):** `Timer.running: window.arrowsShown &&
!(previousButton.hovered || nextButton.hovered)`, a pure declarative
binding. Rejected: QML's `Timer.running`, when driven by a binding that
stays `true` across repeated triggers (e.g. two pointer moves 1.5 s apart
while `arrowsShown` never goes false), does not reset elapsed time — only an
explicit `restart()` does. REQ-F-018's own acceptance criterion ("a second
pointer move at 1.5s keeps them shown until at least 3.3s after the first
move") requires that reset, so `mouseMoved` must still call
`arrowTimer.restart()` imperatively; mixing that imperative call with a
declarative `running:` binding on the same `Timer` creates a binding/
imperative-write conflict (the imperative `restart()` implicitly assigns
`running = true`, which silently breaks the binding the next time the
hover state changes). Keeping `arrowTimer` entirely imperative (`.stop()`
/`.restart()` from named functions, never a bound `running:`) avoids that
class of bug outright.

**Why `hovered`, not `HoverHandler`:** `ViewerButton`/`HnStyle.Button`
already expose `hovered` (used today for the button's own background
color, `Main.qml:31`); reusing it is zero-cost and consistent with how the
rest of the file reads pointer state from `AbstractButton`, rather than
adding a second, independent `HoverHandler` that could disagree with the
button's own `hovered` (e.g. during a press-drag that leaves the button's
`hoverEnabled` region without the `HoverHandler`'s hover region, depending
on `acceptedDevices`/`grabPermissions` defaults).

### 4.3 Shortcut captions: literal `qsTr()` strings, not `Action.shortcut.toString()`

**Decision:** `ViewerMenuItem.shortcutText` is a hand-written literal per
item (§3.3), not derived from the bound `Action`'s `shortcut` property.

**Alternative considered (rejected):** `shortcutText:
control.action ? control.action.shortcut : ""`. Rejected for two reasons:
(1) `QKeySequence`'s native string form is platform- and locale-sensitive
(e.g. it may render `"Ctrl+-"` as `"Ctrl+-"` or with a different dash
glyph depending on Qt's key-sequence-to-string tables), which cannot be
relied on to produce the exact ASCII/typographic strings REQ-F-006 and
REQ-F-013 require verbatim, and which the Help popup and footer must match
identically. (2) Several items (Previous/Next/Refresh/Fullscreen/Quit) are
plain `ViewerMenuItem`s with no backing `Action` at all — they'd need a
separate mechanism regardless, and duplicating the mechanism per-item is
worse than one consistent literal-string field on every item. This mirrors
`FooterKeyHints.qml`'s existing precedent of literal `qsTr()` key strings
independent of the real `Shortcut` sequences (`"Ctrl++/−"` there is not
derived from the two real `"Ctrl++"`/`"Ctrl+-"` sequences either).

### 4.4 Menu separator skipping: rely on Qt Quick Templates' existing behavior

**Decision:** insert `ViewerMenuSeparator` items based on `Basic.MenuSeparator` at the six
positions; **no custom `KeyNavigation` or key-event handling is added**.

**Grounding:** `qquickmenu_p_p.h` (installed Qt 6.11.2 headers) declares
`QQuickMenuPrivate::activateNextItem()`, `activatePreviousItem()`, and
`firstEnabledMenuItem() const` returning `QQuickMenuItem*` specifically —
not a generic `QQuickItem*`. `QQuickMenuSeparator` (`qquickmenuseparator_p.h`)
derives directly from `QQuickControl`, **not** from `QQuickMenuItem`, and
overrides `accessibleRole()`. Since `Menu`'s own Up/Down/Home/End keyboard
navigation is implemented in terms of `QQuickMenuItem*` (via
`activateNextItem`/`activatePreviousItem`/`firstEnabledMenuItem`), a
`MenuSeparator` is structurally invisible to that iteration — it is simply
never a candidate `currentItem`. This resolves the spec's Open Question 1
concretely: **no viewer-side override is needed**; `HnStyle.Menu` (a thin
wrapper over `T.Menu`, confirmed by reading `holonight-qt/qml/Menu.qml`) and
the existing `WindowKeyRouter` J/K→Down/Up remap (`window_key_router.h:45-51`,
unchanged) both ride on this Qt-internal behavior for free.

`MenuSeparator`'s `accessibleRole()` override means it is exposed with its
own accessible role (not `MenuItem` or `Button` — the two roles REQ-F-007
forbids); the design does not need to set `Accessible.ignored` on the
separators. The exact enum value should still be asserted empirically by
the REQ-F-007 accessibility test rather than assumed, since the header only
proves the override exists, not its return value in this Qt version.

**Consequence for existing tests:** `actionsMenu.currentIndex` is a raw
index into `contentModel`, which **does** include separators as model
entries (they still occupy a slot; they are just never made *current*).
Crossing a separator boundary therefore advances `currentIndex` by **2**,
not 1 (one slot for the separator, one for the next real item). Every
existing test that hard-codes a `currentIndex` value (`static_workflow_test.cpp`
lines 268-289, 375-386; `accessibility_test.cpp` lines 77-86) must be
recomputed against the new 25-slot model (19 items + 6 separators) using
this rule — see §5.

### 4.5 Themed, pixel-aligned menu separator content

The approved fractional-scale correction retains `ViewerMenuSeparator` based on
`Basic.MenuSeparator` for padding, navigation and accessibility. Its `contentItem`
is the installed `HnSeparator` using `HoloniightPalette.borderPassive`, default
thickness and default solid rendering. The shared geometry snaps thickness and
scene position to implement the selected one-physical-pixel hairline policy.
No provider or public API changes are needed.

The rendered regression samples all six production separators at five display
scales in both themes, with fractional menu offsets and fractional ListView
scroll offsets. It checks physical pixels in a window capture rather than
inferring rendering from QML heights. The matrix uses RHI/OpenGL and whole-scene
`grabToImage` captures, with Xvfb in headless CI. The software adaptation's
remaining extra-row behavior is recorded as a qualification limitation.
Existing navigation/accessibility tests
remain in place. Captures and logs live under `build/`.

### 4.6 Empty-state hint: nested inside a `Column` with the glyph, not a sibling `ColumnLayout`

**Decision:** wrap `emptyState` (the `HnIcon`) and the two new `HnLabel`s in
one `Column`, centered as a unit in `canvasArea`, replacing the icon's
current manual `x`/`y` centering formula.

**Sizing formula:**

```qml
Column {
    id: emptyStateGroup
    anchors.centerIn: canvasArea
    visible: window.document.state === ImageDocument.Empty
    spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
    readonly property real minGlyphSize: 48
    readonly property real hintBlockHeight: emptyHintPrimary.implicitHeight + spacing + emptyHintSecondary.implicitHeight
    readonly property real availableForGlyph: canvasArea.height - hintBlockHeight - spacing - 2 * emptyDecoration.padding
    HnIcon {
        id: emptyDecoration
        objectName: "emptyState"
        anchors.horizontalCenter: parent.horizontalCenter   // Column manages y, leaves x free (Row/Column precedent, §1 of footer-key-hints DESIGN)
        readonly property real shorterDimension: Math.min(canvasArea.width, canvasArea.height)
        readonly property real padding: Math.max(32, Math.min(64, shorterDimension * 0.08))
        readonly property real cappedSide: Math.max(0, Math.min(shorterDimension - 2 * padding, emptyStateGroup.availableForGlyph))
        visible: cappedSide >= emptyStateGroup.minGlyphSize
        width: cappedSide
        height: cappedSide
        size: Math.min(cappedSide, Math.max(1, Math.floor(1024 / window.devicePixelRatio)))
        source: cappedSide > 0 ? Qt.resolvedUrl("icons/empty-viewer.svg") : ""
        normalColor: HoloniightPalette.surface
        Accessible.ignored: true
    }
    HnLabel {
        id: emptyHintPrimary
        objectName: "emptyStateHintPrimary"
        anchors.horizontalCenter: parent.horizontalCenter
        role: HnTypographyRole.Body
        color: HoloniightPalette.textMuted
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignHCenter
        rawText: qsTr("No image open")
        Accessible.role: Accessible.StaticText
        Accessible.name: rawText
    }
    HnLabel {
        id: emptyHintSecondary
        objectName: "emptyStateHintSecondary"
        anchors.horizontalCenter: parent.horizontalCenter
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textMuted
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        width: Math.min(implicitWidth, canvasArea.width - 32)
        rawText: qsTr("Ctrl+O to open · or drop an image here")
        Accessible.role: Accessible.StaticText
        Accessible.name: rawText
    }
}
```

- **"Text keeps its size, glyph shrinks first" (REQ-F-002):** the two
  `HnLabel`s have no size-shrinking logic at all — their `role`/font are
  fixed regardless of canvas size, matching "font size equals its size at
  1000×700" verbatim. Only `emptyDecoration.cappedSide` shrinks, as a
  `Math.min` against `availableForGlyph`, which itself shrinks as
  `canvasArea.height` shrinks.
- **"Glyph hides, text remains" below a threshold:** `cappedSide >=
  minGlyphSize` (48px) is the hide condition — chosen so the glyph never
  renders illegibly small; below it, `visible: false` removes it entirely
  while the `Column`'s two labels are unaffected by the glyph's visibility
  (a `Column` still lays out remaining visible children correctly when one
  child hides).
- **`anchors.horizontalCenter` inside `Column`:** `Column` manages the `y`
  axis only, leaving `x` free for anchoring — the exact same precedent
  `footer-key-hints/DESIGN.md` §1 documents for `Row` (which manages `x`
  and leaves `y` free). This keeps three differently-sized children
  (variable glyph width, two label widths) centered without a `ColumnLayout`
  and its `Layout.alignment` ceremony.
- **REQ-F-004 (accessibility):** `emptyDecoration` keeps
  `Accessible.ignored: true`; both hint labels get an explicit
  `Accessible.role: Accessible.StaticText` / `Accessible.name: rawText` pair
  (an `HnLabel`/`Label` does not self-expose a name by default the way a
  `Control` does, so this must be explicit — same pattern as the existing
  `documentFeedback`/`folderError` labels in the same file).

### 4.7 `helpOpen: bool` replacing `detailDialog: int`

**Decision:** `detailDialog` (0/2, `Main.qml:76`) becomes `helpOpen: bool`,
exactly mirroring `informationOpen`'s existing shape. `modalActive` becomes
`dialogRequested || informationOpen || helpOpen`.

This is the same toggle pattern `ImageInformationPopup.qml`'s `I` shortcut
already established (see that feature's `DESIGN.md` §3.4/§4): the window
`Action` (`shortcutHelp`, shortcut `"?"`) only ever performs the **open**
half (`onTriggered: window.helpOpen = true`), because Qt Quick's shortcut
matcher ignores window-level `Shortcut`/`Action` items while a modal
`Popup` holds focus — so the window's own `"?"` cannot be the thing that
closes the popup. The **close** half is a `Shortcut { sequence: "?";
enabled: root.opened; onActivated: root.close() }` declared inside
`ShortcutHelpPopup.qml`'s content, per REQ-F-011's explicit acceptance
criterion ("declared inside the modal to prevent window-level routing").
`closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside`
handles Escape and click-outside without extra code, identical to
`ImageInformationPopup.qml:34`.

### 4.8 Dimmer alpha: Shortcut Help now matches Image Information (0.22)

REQ-F-010 requires the new popup's dimmer to equal
`Qt.alpha(HoloniightPalette.shadow, 0.22)` — the same value
`ImageInformationPopup.qml:36-39` already uses. This **changes** Shortcut
Help's dimmer from the Basic style's default 0.5 (today's `Basic.Dialog`
never overrides `Overlay.modal`, so it inherits the installed style's
`Color.transparent(control.palette.shadow, 0.5)`) down to 0.22.

**Risk:** `tests/image_information_popup_test.cpp`'s
`ImageInformationPopup.DimsLessThanShortcutHelp` (line 403) asserts
`information ≈ 0.22` **and** `help ≈ 0.5`, i.e. a strict inequality between
the two. Once Shortcut Help also uses 0.22, that assertion is false — the
two dimmers are now equal, by design (REQ-F-010 explicitly asks for
"the same"). This existing test **must** be updated (or removed, since its
premise — that the two popups intentionally differ — no longer holds) as
part of this change. Flagged again in §5/§6.

---

## 5. Test plan

| REQ(s) | Test file | New/changed test |
|---|---|---|
| F-001, F-002, F-003 | new `tests/empty_state_test.cpp` | load `Main` with/without an image; assert `emptyStateHintPrimary`/`Secondary` text, `HnTypographyRole`, color, glyph-above-text ordering at 1000×700; resize to 420×280 and assert both lines inside canvas and font size unchanged; construct a canvas-area height below `hintBlockHeight + minGlyphSize + padding` (via a standalone `canvasArea`-equivalent load, or by shrinking a real window as far as 280 allows and asserting the *formula*, since 420×280 itself may still fit the glyph — see risk below) and assert glyph `visible == false`, both lines still visible; assert hidden during Loading/Ready/Error |
| F-004 | `tests/accessibility_test.cpp` (extend `NamesRolesEnabledFocusAndDialogs`) | find two `StaticText` nodes named the hint lines; assert no accessible node exists for `emptyState` |
| F-005, F-006, F-009 | new `tests/menu_layout_test.cpp` | enumerate `actionsMenu.contentModel` (or `contentData`), assert 19 `ViewerMenuItem` + 6 `MenuSeparator` in the exact order/positions of §3.3; assert no two adjacent separators, none first/last; assert each item's `text`/`shortcutText` against §3.3; assert `fitButton`'s label+shortcut both use `textDisabled` with no image open; re-run existing `openButton`/`fitButton`/.../`informationMenuItem` trigger tests, adjusted for the new labels |
| F-007 | `tests/static_workflow_test.cpp` (rewrite the `currentIndex` sequence, §4.4) + `tests/accessibility_test.cpp` (rewrite lines 77-86) | recompute expected `currentIndex` values for the new 25-slot model (§4.4's "+2 across a separator" rule); assert Down from "Refresh" lands on "Previous"; assert repeated Down from "Open…" visits all 19 items and never stops on a separator; accessibility tree has no separator with role `MenuItem`/`Button` |
| F-008 | `tests/accessibility_test.cpp` | `actionsButton`'s accessible name equals `"Menu"` |
| F-010 | new `tests/shortcut_help_popup_test.cpp` (mirrors `image_information_popup_test.cpp`'s `StandalonePopup` fixture) | file exists, registered in CMake/format-script/Taskfile (CI/manual); `Main.qml` has no `detailsDialog`; width 480/396/376 at 1000/420/(standalone 400); dimmer color `Qt.alpha(shadow, 0.22)`; `shortcutHelpCloseButton` present, top-right, closes on click; no `"Close"`-labeled footer button |
| F-011 | `shortcut_help_popup_test.cpp` + `static_workflow_test.cpp` | `?` opens, `?` again closes (toggle); `?` then Escape closes; click on `Overlay.overlay` outside the popup closes it; grep confirms the `?` `Shortcut` is declared inside `ShortcutHelpPopup.qml` |
| F-012, F-013, F-014 | `shortcut_help_popup_test.cpp` | read `root.sections` directly (no `Repeater` `findChild`); assert 6 sections in order with the exact labels; assert every row's `key`/`description` against §3.4's table; assert no row `key` contains `"Tab"`/`"J"`/`"K"`, no `description` contains `"header"`/`"menu"` |
| F-015 | `shortcut_help_popup_test.cpp` | resize standalone popup to 420×280 host; `Flickable.contentHeight > height`; scroll to end; header `y` unchanged; every row `x + width <= body width`; `contentWidth == availableWidth` |
| F-016 | `static_workflow_test.cpp` | ≥2-image folder at position 2; open Help, press `[`, assert position unchanged; close Help, press `[`, assert position 1; assert `modalActive` true while open, arrow keys don't pan |
| F-017 | `tests/accessibility_test.cpp` | `shortcutHelpContent`'s `Accessible.name == "Shortcut Help"`, role `Dialog`; each section found as `Grouping`; each row `StaticText` named `"key description"` exactly once |
| F-018, F-019, F-020, F-021, F-022 | new `tests/transient_overlay_test.cpp` (replaces the `arrowTimer`/`detailsTimer.running` reads in `image_inspection_test.cpp`'s `Viewer.IndependentOverlayTimers`, §6 risk) | assert `previousButton.shown`/`nextButton.shown` (not `arrowTimer.running`) become true within one frame of a pointer move; still true at 1.8s, false by 2.2s with no further movement; a second move at 1.5s keeps them shown to ≥3.3s; hover on `nextButton` for 3s keeps `shown==true`; hover-exit restarts the 2s window; keyboard input flips `shown=false` within one frame and keeps it false ≥2.5s across a subsequent `]`; opacity strictly between 0 and 1 at ~75ms into a hide, 0 by 300ms with `visible==false`; a click at the former button position 300ms after hide doesn't change position; accessibility tree has no `"Next image"`/`"Previous image"` node while `visible==false`; Error-state document in a multi-image folder + pointer move still shows arrows and `nextButton` click still navigates; Empty state and a single-image folder never show the arrows (`SingleImageNeverShowsBrowseArrows`) |
| F-023, F-024, F-025, F-026 | `transient_overlay_test.cpp` | `detailsStrip.shown` (not `detailsTimer.running`) true after first render, at 2.8s, false by 3.2s; each of Ctrl++/Ctrl+−/Ctrl+0/1/R/Shift+R/X/Shift+X/menu-Zoom-In sets `shown=true` within one frame with no pointer movement; `]` shows the HUD only once the next image's `firstRendered` fires; a trigger at 2s after a previous one keeps `shown` to ≥4.8s after the first; opacity strictly between 0 and 1 at ~75ms into a hide, visible false by 300ms; drag starting on the HUD's former area 300ms after hiding pans the canvas; no-image pointer move leaves `shown=false`; Error-state Ctrl++ / pointer move leaves `shown=false`; navigating away from Ready while shown flips `shown=false` before the next Ready |
| F-027, F-028, F-029 | `static_workflow_test.cpp` (rewrite lines 246-248's `Key_H`/`Key_V`) | `X` flips horizontally, `Shift+X` flips vertically; `H`, `V`, `Shift+H`, `Shift+V` leave orientation unchanged; menu shows "Flip Horizontally"/"X" and "Flip Vertically"/"Shift+X"; Help TRANSFORM row equals `"X / Shift+X"` / `"Flip horizontally/vertically"`; no menu shortcut or Help key equals `"H"` or `"V"`; no image open → `X` does nothing; Help open → `Shift+X` does nothing; Ready + no modal → `X` flips |
| F-030 | `footer_key_hints_test.cpp` | seven hints remain in model order; the Zoom hint shows Ctrl++ and Ctrl+-; narrow widths keep one row, hide a suffix of hints, and always show Help; widening restores all hints |
| C-008 | `static_workflow_test.cpp` | header button focused, `X` flips; menu open, `X` flips and closes the menu (same as `R`) |
| NF-001 | `empty_state_test.cpp`, `shortcut_help_popup_test.cpp`, `menu_layout_test.cpp` | 420×280 checks for hint/menu/popup; standalone 400px popup row-width check |
| NF-002 | grep + visual check | no `#…`/`Qt.rgba(`/named-color/`font.family`/`font.pixelSize` in new/changed QML outside `HnStyle` defaults |
| NF-003 | `transient_overlay_test.cpp` | drag started on canvas while HUD fades still pans by the full delta |
| NF-004 | `tests/accessibility_test.cpp` | non-empty name on `shortcutHelpCloseButton` and every enabled menu item; no duplicate `StaticText` for hint lines or Help rows |
| NF-005 | grep | no bare user-visible string literal outside `qsTr()` in new/changed QML |
| C-001, C-002, C-003 | CI / manual | `ShortcutHelpPopup.qml` in `CMakeLists.txt` `QML_FILES`, `check-qml-format.sh`'s loop, and `Taskfile.yml:68`'s `qml-format.sh -i` list |
| C-004 | grep + full suite | no test references `detailsDialog`/`detailsText`/`closeDetailsButton`; full `ctest` preset passes |
| C-005, C-006, C-007 | build + grep + full suite | one new test per feature area (table above); diff touches no non-test `.cpp`/`.h` and no `holonight-qt` files; `actionsButton`/`actionsMenu`/`previousButton`/`nextButton`/`detailsStrip`/`emptyState`/`footer`/`informationPopup` objectNames grep-confirmed present |

New files added to `qt_add_executable(viewer-smoke …)` in `tests/CMakeLists.txt`:
`empty_state_test.cpp`, `menu_layout_test.cpp`, `shortcut_help_popup_test.cpp`,
`transient_overlay_test.cpp` (four new files, one per feature cluster,
following the existing one-file-per-concern convention rather than growing
`static_workflow_test.cpp`/`accessibility_test.cpp` further — both already
large per the `footer-key-hints/DESIGN.md` §4 precedent for the same
decision).

**Existing tests requiring rework (not just "pass unchanged"):**
- `tests/image_inspection_test.cpp`'s `Viewer.IndependentOverlayTimers`
  (lines 267-348) reads `arrowTimer`/`detailsTimer`'s `interval`/`running`
  properties directly — this is exactly what REQ-F-025 forbids
  ("No viewer test reads `arrowTimer.running` or `detailsTimer.running` to
  decide visibility"). Its visibility assertions move to the new
  `transient_overlay_test.cpp` reading `shown`; anything in it that isn't
  about visibility (e.g. the `VIEWER_CAPTURE_PREFIX` screenshot block,
  lines 332-345) can stay.
- `tests/static_workflow_test.cpp`'s `StaticWorkflowControls` (lines 246-248,
  268-289, 375-386) and `tests/accessibility_test.cpp`'s
  `NamesRolesEnabledFocusAndDialogs` (lines 77-93, 100-105): `Key_H`/`Key_V`
  → `Key_X`/`Shift+Key_X`; every hard-coded `currentIndex` recomputed for
  the new order (§4.4); `detailsText`/`closeDetailsButton`/`detailDialog`
  references replaced with `shortcutHelpContent` (or the popup's exposed
  `sections`)/`shortcutHelpCloseButton`/`helpOpen`.
- `tests/image_information_popup_test.cpp`'s `DimsLessThanShortcutHelp`
  (line 403) asserts `help ≈ 0.5`; per §4.8 this becomes `help ≈ 0.22`
  (equal to Information's dimmer) — the test's premise changes, not just
  its numbers; consider renaming it (e.g. `DimsSameAsShortcutHelp`) to keep
  the test name honest about what it now asserts.

---

## 6. Known risks and mitigations

| # | Risk | Mitigation |
|---|---|---|
| 1 | `viewChanged` also fires on resize (§1) — anyone implementing from the spec's Open-Question-5 wording alone might wire `canvas.onViewChanged` directly and reintroduce a resize-triggered HUD. | §4.1 documents the corrected approach explicitly; the `NF-003`/`transient_overlay_test.cpp` should include a resize-during-idle assertion (`detailsStrip.shown` stays `false` across a `window.resize()` with no other trigger) even though the spec doesn't ask for it directly, as a regression guard for this exact mistake. |
| 2 | Recomputing every `currentIndex` in `static_workflow_test.cpp`/`accessibility_test.cpp` against the new 25-slot menu model (§4.4) is mechanical but error-prone (off-by-one per separator crossed). | Compute expected indices from the §3.3 table programmatically in the test (a small `constexpr`/lambda counting real items + separators up to a named target) rather than hand-typing magic numbers, so a future menu reorder only touches §3.3, not scattered literals. |
| 3 | `ImageInformationPopup`'s `DimsLessThanShortcutHelp` test (§4.8) encodes an inequality this feature intentionally removes. | Explicitly listed in §5's "existing tests requiring rework"; must not be silently left failing — treat as a required edit, cross-referenced from both designs (this one and, if revisited, `image-information-popup/DESIGN.md`, which is not otherwise touched). |
| 4 | `MenuSeparator`'s exact `accessibleRole()` return value is confirmed only by header presence of the override (§4.4), not by reading Qt's compiled behavior directly (no `.cpp` source available on this machine, only compiled headers). | REQ-F-007's own acceptance criterion is the safety net — if the assumption is wrong, the new accessibility test fails loudly at implementation time rather than silently passing; no design step depends on a *specific* role, only "not MenuItem/Button". |
| 5 | REQ-F-002's "canvas height below (hint text height + minimum glyph size + padding)" scenario cannot be reached via the real window (420×280 minimum) if that sum is ever less than 280 minus header/footer chrome. | Per project memory ("standalone load for <420px tests"), exercise this branch via a standalone load of just `canvasArea`'s layout logic (or, more simply, by constructing `emptyStateGroup`'s formula inputs directly in a unit-style QML test) rather than trying to force it through a real `HnApplicationWindow`, which cannot shrink below its `minimumHeight: 280`. |
| 6 | `shortcutText` is hand-duplicated from each `Action`'s real `shortcut` (§4.3) — the two can drift (e.g. someone changes `flipHorizontal.shortcut` without updating the menu's `shortcutText: "X"`). | REQ-F-006's "menu, Help and footer spell shared shortcuts identically" acceptance criterion, plus F-027/028's cross-checks, catch drift at test time; no runtime mechanism prevents it (accepted, matching the pre-existing footer precedent of the same risk, §4.3). |
| 7 | Widening `actionsMenu` from 340 to 380px (§3.2) is an estimate, not a measured fit at every DPI scale (spec's own Open Question 2 flags font-scale risk for the 480px Help popup; the same risk applies to the menu). | Follow the spec's own suggested mitigation: visual regression capture at 96/120/144 DPI (existing `scripts/check-visual.sh` pattern, `image_inspection_test.cpp`'s `VIEWER_CAPTURE_PREFIX` block) before considering the menu width final; REQ-F-006's "no overlap" acceptance criterion is the automated backstop. |
| 8 | The MOUSE-section key/description split (§3.4) is this design's interpretation of ambiguous spec prose, not literal spec text. | Documented explicitly as a design decision (§3.4's "Wording note") so implementers copy this design's table rather than re-deriving their own split that could disagree with a reviewer's reading of the spec. |

---

## 7. File change list

**New:**
- `apps/viewer/ShortcutHelpPopup.qml`
- `tests/empty_state_test.cpp`
- `tests/menu_layout_test.cpp`
- `tests/shortcut_help_popup_test.cpp`
- `tests/transient_overlay_test.cpp`

**Changed (QML):**
- `apps/viewer/Main.qml` — menu reorder/separators/shortcut captions,
  `flipHorizontal`/`flipVertical` shortcuts, `detailDialog`→`helpOpen` +
  `detailLoader`→`ShortcutHelpPopup` loader, empty-state hint, transient
  controller (`arrowsShown`/`detailsShown`/functions), `actionsButton`
  accessible name.

**Changed (tests):**
- `tests/image_inspection_test.cpp` — `Viewer.IndependentOverlayTimers`
  visibility assertions moved off `arrowTimer.running`/`detailsTimer.running`.
- `tests/static_workflow_test.cpp` — `Key_H`/`Key_V` → `X`/`Shift+X`;
  `currentIndex` literals recomputed; `detailDialog`/`detailsText`/
  `closeDetailsButton` references replaced.
- `tests/accessibility_test.cpp` — same reference replacements;
  `currentIndex` literals recomputed; `actionsButton` name assertion.
- `tests/image_information_popup_test.cpp` — `DimsLessThanShortcutHelp`
  reworked per §4.8/§6 risk 3.

**Changed (tooling registries, REQ-C-002/003):**
- `apps/viewer/CMakeLists.txt` — add `ShortcutHelpPopup.qml` to `QML_FILES`.
- `scripts/check-qml-format.sh` — add `ShortcutHelpPopup.qml` to the file loop.
- `Taskfile.yml` — add `ShortcutHelpPopup.qml` to line 68's
  `qml-format.sh -i` list.
- `tests/CMakeLists.txt` — add the four new test files to
  `qt_add_executable(viewer-smoke …)`'s source list.

**Not changed:** `apps/viewer/FooterKeyHints.qml`, `ImageInformationPopup.qml`,
`image_canvas.h`/`.cpp`, `image_document.h`/`.cpp`, `window_key_router.h`,
and everything under `holonight-qt/`.
