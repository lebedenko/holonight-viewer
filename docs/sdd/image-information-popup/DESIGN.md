# Image Information Popup — DESIGN

Companion to `SPEC.md`. Grounded in `apps/viewer/Main.qml`,
`image_document.{h,cpp}`, `exif_metadata.{h,cpp}`, `window_key_router.h`, and
the `FooterKeyHints.qml` / `footer_key_hints_test.cpp` precedent for adding a
standalone, testable QML component to this module.

---

## 1. Components

### C++

| Component | Responsibility |
|---|---|
| `ExifDetails` (`exif_metadata.h`) | Formatted-EXIF-string struct. Gains `aperture`, `shutter`, `iso`, `focalLength`, replacing the combined `exposure` string. Formatting (`"f/2.2"`, `"1/20 s"`, `"ISO 1423"`, `"6.9 mm"`) stays in `exif_metadata.cpp` — the current `exposure()` body is split into four small formatters instead of one that joins with `"  "`. |
| `ExifMetadata::parse` (`exif_metadata.cpp`) | Populates the four new fields via `aperture()`, `shutterSpeed()`, `isoText()`, `focalLengthText()`. |
| `ImageDocument` (`image_document.h/.cpp`) | Owns `ImageInformation information_` (unchanged). Replaces `informationText()` with structured `Q_PROPERTY`s (§3). Owns presentation-formatting as free functions so they're unit-testable without decoding real images. |
| Free functions in `image_document.h/.cpp` (new) | `abbreviateHomePath`, `formatSummaryLine`, `formatTransformedLine`, `formatModifiedText`, `joinNonEmpty` — pure, header-declared, exercised directly from `exif_metadata_test.cpp`. |

`ImageInformation` (`decoded_image_cache.h`) is unchanged; it already carries
`format`, `encodedSize`, `modified`, `decodedSize`, `exif` — exactly the raw
material the new formatters need.

### QML

| Component | Responsibility |
|---|---|
| `apps/viewer/ImageInformationPopup.qml` (new) | Dedicated modal `Basic.Popup`. Takes `document`, renders fixed header + scrollable sections, owns its own close button, dimming and focused-path cursor scrolling. |
| `apps/viewer/Main.qml` | Adds `informationOpen: bool` and a `Loader` that instantiates `ImageInformationPopup` when true. The shared `Basic.Dialog` Loader (`detailLoader`, ~L330-440) is trimmed to serve **only** Shortcut Help; the `details.information` branch and `informationText`/`informationTitle`/`informationThumbnail` items are deleted. |

This mirrors how `FooterKeyHints.qml` was split out of `Main.qml`: a new
file, registered in `CMakeLists.txt`, `scripts/check-qml-format.sh`, and
`Taskfile.yml`, loadable standalone in a test (§7, §8).

---

## 2. Data flow

```
file bytes
  → ExifMetadata::payload()        (unchanged: locates APP1/eXIf/EXIF block)
  → ExifMetadata::parse()          (changed: fills aperture/shutter/iso/focalLength)
  → ExifDetails                    (carried inside ImageInformation.exif)
  → DecodeResult.information       (decodeImage(), unchanged plumbing)
  → ImageDocument::complete()      (unchanged: information_ = std::move(result.information))
  → ImageDocument::information_    (private member, unchanged)
  → ImageDocument Q_PROPERTYs      (new: summaryLine, transformedLine, modifiedText,
                                     displayPath, informationSections — computed on demand)
  → QML bindings in ImageInformationPopup.qml
```

Every new property is a plain getter over already-stored state, so a single
`NOTIFY changed` suffices — matching the existing convention where
`localPath`, `formattedFileSize`, `fileName`, etc. all share one `changed()`
fired from `select()`, `complete()`, `transform()`, `resetTransform()`.
`transformedLine` depends on `orientation_`, but `transform()`/
`resetTransform()`/`select()` already emit `orientationChanged()` **and**
`changed()` together — the existing `transformedDimensions` property relies
on the same fact. No new signal is introduced.

Live updates (REQ-F-020): the popup never copies document state into local
QML properties — every label binds straight to `window.document.<property>`
— so `transform()` re-evaluates bindings the instant `changed()` fires,
popup open or not.

---

## 3. Interfaces / APIs

### 3.1 `ExifDetails` (exif_metadata.h)

```cpp
struct ExifDetails {
  QString camera;
  QString lens;
  QString aperture;      // "f/2.2"
  QString shutter;       // "1/20 s" or "2 s"
  QString iso;           // "ISO 1423"
  QString focalLength;   // "6.9 mm"
  QString location;      // "49.79849° N, 24.02951° E"
  QString altitude;      // "386 m"
  [[nodiscard]] bool hasCamera() const {
    return !camera.isEmpty() || !lens.isEmpty() || !aperture.isEmpty() ||
           !shutter.isEmpty() || !iso.isEmpty() || !focalLength.isEmpty();
  }
  [[nodiscard]] bool hasLocation() const { return !location.isEmpty() || !altitude.isEmpty(); }
  bool operator==(const ExifDetails&) const = default;
};
```

`exposure` is removed entirely (REQ-F-010: fields, not a blob). The four
formatters keep the exact numeric formatting `exposure()` uses today (one
decimal via `decimal()` for aperture/focal length, rounded reciprocal for
sub-second shutter speeds, integer ISO); only the join is removed.

### 3.2 `ImageDocument` (image_document.h)

Removed: `Q_PROPERTY(QString informationText ...)`, `informationText()`.

Added:

```cpp
Q_PROPERTY(QString summaryLine READ summaryLine NOTIFY changed)
Q_PROPERTY(QString transformedLine READ transformedLine NOTIFY changed)
Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY changed)
Q_PROPERTY(QString displayPath READ displayPath NOTIFY changed)
Q_PROPERTY(QVariantList informationSections READ informationSections NOTIFY changed)
```

Free functions (declared alongside `decodeImage`/`commandLineUrl` at
namespace scope, defined in `image_document.cpp`, callable directly from
`exif_metadata_test.cpp`, which already links `image_document.cpp`):

```cpp
// "~" abbreviation. `home` defaults to QDir::homePath() but is a parameter
// so tests don't depend on the real environment's home directory.
QString abbreviateHomePath(const QString& absolutePath, const QString& home = QDir::homePath());

// "JPEG · 3072 × 4080 · 12.5 MP · 2.5 MB", omitting missing pieces;
// "Details unavailable" if format+dimensions+size are all empty/invalid.
QString formatSummaryLine(const QString& format, QSize decodedSize, qint64 encodedSize);

// "Rotated view H × W" iff orientation swaps width/height vs. decodedSize; else "".
QString formatTransformedLine(QSize decodedSize, QSize transformedSize);

// QLocale().toString(modifiedLocal, QLocale::ShortFormat), or "" if invalid.
QString formatModifiedText(const QDateTime& modifiedUtc);

// Joins non-empty parts with " · "; empty input yields "".
QString joinNonEmpty(const QStringList& parts, QLatin1StringView separator = QLatin1StringView(" · "));
```

`abbreviateHomePath` — the sibling-prefix edge case is the crux of REQ-F-012:

```cpp
QString abbreviateHomePath(const QString& absolutePath, const QString& home) {
  const QString normalizedHome = QDir::cleanPath(home);
  const QString normalizedPath = QDir::cleanPath(absolutePath);
  if (normalizedHome.isEmpty()) return absolutePath;
  if (normalizedPath == normalizedHome) return QStringLiteral("~");
  const QString prefix = normalizedHome + u'/';
  return normalizedPath.startsWith(prefix) ? u'~' + normalizedPath.sliced(normalizedHome.size())
                                            : absolutePath;
}
```

Requiring the `/` after `home` is what keeps `/home/alice2/x` from being
treated as under `/home/alice` — a plain `startsWith(normalizedHome)` would
wrongly yield `~2/x`. This is the one line most worth a dedicated unit test.

`informationSections()` builds an internal helper struct, converted to a
QML-friendly shape only at the boundary:

```cpp
struct InformationSection { QString label; QStringList lines; };  // internal only

QVariantList ImageDocument::informationSections() const {
  QVariantList sections;
  for (const auto& section : /* CAMERA, LOCATION, FILE from information_ + localPath() */) {
    if (section.lines.isEmpty()) continue;                 // REQ-F-013
    sections.append(QVariantMap{{"label", section.label}, {"lines", section.lines}});
  }
  return sections;
}
```

CAMERA lines: `exif.camera`; `joinNonEmpty({exif.lens, exif.focalLength, exif.aperture})`;
`joinNonEmpty({exif.shutter, exif.iso})` (REQ-F-009). LOCATION: one logical line
`joinNonEmpty({exif.location, altitudeText})`, section omitted if empty
(REQ-F-011). FILE: one logical line `displayPath()`, section omitted when
`localPath()` is empty (REQ-F-012/013).

**Section labels: `tr()`'d in C++, title case, upper-cased only in QML.**
`label` is `tr("Camera")` / `tr("Location")` / `tr("File")`. REQ-F-022 needs
the *accessible* name in title case; REQ-F-008 needs the *visible* text in
uppercase. Storing title case once and doing `.toUpperCase()` only in the
visual `HnLabel`'s `rawText` binding satisfies both from one translated
string. The reverse (store uppercase, lowercase for `Accessible.name`) was
rejected: case-transforming a translation is more locale-fragile than
transforming the display-only copy.

**Section shape: `QVariantList` of `QVariantMap`, not `Q_GADGET`.** A
`Q_GADGET` value type would need registration plus `QML_LIST_PROPERTY` or
manual `QVariant::fromValue` wrapping to be `Repeater`-iterable — machinery
this codebase has never introduced (no existing `Q_GADGET`/`QVariantList`
precedent, confirmed by grep). `QVariantMap{label, lines}` is directly usable
from a `Repeater` (`modelData.label`, `modelData.lines`) with zero new
registration; the whole document already recomputes fully on `changed()`, so
there's no incremental-update case to justify a real model type. Risk:
string keys aren't typo-checked at compile time — mitigated by the
objectName-per-section test coverage in §7 (F-008, F-022).

### 3.3 `ImageInformationPopup.qml` public API

```qml
Basic.Popup {
    id: root
    objectName: "informationPopup"
    required property ImageDocument document
    // open()/close()/opened/closed/visible come from Popup itself —
    // Main.qml drives it the same way it drives the shared dialog today.
}
```

- `document`: required, same object as `window.document`.
- Sections carry a stable untranslated `key` (`Camera`/`Location`/`File`)
  alongside the translated `label`. FILE is declared directly after the
  `Repeater` rather than as a delegate: Repeater delegates have no QObject
  parent, so `findChild` could not reach the selectable path text.
- The popup is not an Item, so `Accessible.role: Dialog` and
  `Accessible.name: "Image Information"` sit on its `contentItem`
  (`informationContent`).
- objectNames (test hooks): `informationPopup` (root), `informationCloseButton`,
  `informationScroll`, `informationPathText`, `informationPreview`,
  `informationFileName`, `informationSummary`, `informationTransformed`,
  `informationModified`, and per-section `"informationSection" + label`
  (e.g. `informationSectionCamera`) with `Accessible.name: modelData.label`.

### 3.4 Main.qml refactor

```qml
property bool informationOpen: false
property int detailDialog: 0   // now Help-only: 0 = closed, 2 = open
readonly property bool modalActive: dialogRequested || informationOpen || detailDialog !== 0
readonly property bool hasPath: document.localPath.length > 0 && !window.modalActive
```

```qml
Action {
    id: imageInformation
    objectName: "imageInformationAction"
    text: qsTr("Image Information")
    shortcut: "I"
    // While open, the modal popup blocks window shortcuts and handles I itself.
    enabled: window.hasPath
    onTriggered: window.informationOpen = true
}
```

`informationButton`/`informationMenuItem` also set `window.informationOpen =
true` under `enabled: window.hasPath`. The closing half of the `I` toggle is a
`Shortcut { sequence: "I"; enabled: root.opened; onActivated: root.close() }`
declared inside the popup's content (§4).

```qml
Loader {
    id: informationLoader
    active: window.informationOpen
    sourceComponent: ImageInformationPopup {
        document: window.document
        parent: Overlay.overlay
        Component.onCompleted: open()
        onClosed: window.informationOpen = false
    }
}
```

`detailLoader` drops the `information` property and every
`details.information ? … : …` branch, becoming the Help-only dialog it was
conceptually always meant to be, with `title: qsTr("Shortcut Help")`
unconditional.

---

## 4. Keyboard behavior

- **I toggles** (REQ-F-018): the window `imageInformation` Action opens the
  popup (disabled while any modal, including Help, is open); a `Shortcut` for
  `I` inside the popup closes it. Verified in implementation: Qt Quick's
  shortcut matcher ignores window-level shortcuts whose owner is outside an
  open modal popup, so the window Action cannot close the popup. Keeping the
  Action disabled while open also avoids two enabled `I` shortcuts, which Qt
  would treat as ambiguous.
- **Escape closes popup, not fullscreen** (REQ-F-016): the window-level
  `Escape` Shortcut is already `enabled: !window.modalActive`; once
  `informationOpen` folds into `modalActive`, it's disabled while the popup
  is open, so it never calls `leaveFullscreen()`. The popup sets
  `closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside`, consuming
  Escape itself. `onClosed` flips `informationOpen` false, `modalActive`
  follows on the same tick, and `window.clearImageFocus()` (already wired to
  `onModalActiveChanged`) restores window-level shortcuts with no extra click
  — identical to today's Help-close path.
- **`]`/`[` blocked** (REQ-F-019): already `enabled: !window.modalActive` —
  no change beyond folding `informationOpen` in.
- **Tab order**: the popup has exactly two focusable items (close button,
  path `TextEdit`), so Qt Quick Controls' default focus chain suffices — no
  `KeyNavigation` wiring needed (unlike the header's 3-button ring, which
  needs `WindowKeyRouter`'s custom Tab handling because those buttons sit
  outside any single Popup's tab scope). `WindowKeyRouter::eventFilter`
  already returns `false` immediately when `m_modalActive` is true, so it
  never competes with the popup's own tab handling.
- **Click-outside** (REQ-F-017): `Popup.CloseOnPressOutside`; the dimming
  rect underneath receives the press.
- **Modal popups block window shortcuts** (verified in implementation): while
  a modal popup is open, Qt Quick's shortcut matcher ignores `Shortcut`/`Action`
  items that live outside it. Escape is handled by the popup's `closePolicy`,
  `[`/`]` are blocked both by this and by `enabled: !window.modalActive`, and
  `I` needs the popup-owned shortcut above. The `enabled` gates remain as the
  explicit, testable statement of intent.

---

## 5. Layout

The path editor is nested inside section layouts, so the ScrollView does not
attach it as a direct text-editor child. On focus or cursor-rectangle changes,
the editor defers `ensureCursorVisible()` until layout settles. It maps the cursor
into viewport coordinates and clamps the required `contentY` adjustment to the
body's scroll range. Only the body scrolls; the header remains fixed. Keyboard
selection is explicitly enabled on the read-only path editor.

Root `Basic.Popup`, `parent: Overlay.overlay`, `anchors.centerIn: parent`.

```qml
width: Math.min(480, windowWidth - 24)           // REQ-NF-001
height: Math.min(implicitHeight, windowHeight - 24)
padding: 16
```

`windowWidth`/`windowHeight` mirror the host geometry (`parent.width` and
`parent.height`). This matches the implementation detail that the popup is
centered in `Overlay.overlay` and can be tested without `Main.qml` via the
standalone fixture (REQ-C-007).

```
contentItem: ColumnLayout {
    spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
    RowLayout {                                    // fixed header — REQ-NF-002
        id: headerRow
        objectName: "informationHeaderRow"
        Layout.fillWidth: true
        Rectangle {                                // 96×96 preview
            objectName: "informationPreview"
            visible: root.document.state === ImageDocument.Ready
                     && windowWidth >= 360           // REQ-NF-003
        }
        ColumnLayout {
            Layout.fillWidth: true
            HnLabel { objectName: "informationFileName"; role: Subheading; elide: ElideMiddle }
            HnLabel { objectName: "informationSummary" }
            HnLabel { objectName: "informationTransformed"; visible: rawText.length > 0 }
            HnLabel { objectName: "informationModified"; visible: rawText.length > 0 }
        }
        HnStyle.Button { objectName: "informationCloseButton"; display: IconOnly; icon.source: "icons/close.svg" }   // REQ-F-015
    }
    ScrollView {
        objectName: "informationScroll"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        ColumnLayout {
            width: parent.width                    // REQ-NF-004: forces wrap, not overflow
            Repeater {
                model: root.document.informationSections
                delegate: ColumnLayout { /* label + wrapped value lines */ }
            }
        }
    }
}
```

- **Header fixed / body scrolls** (REQ-NF-002): the header `RowLayout` is a
  sibling of the `ScrollView`, not nested inside it, so it's structurally
  never part of scrollable content — no y-position "freezing" needed.
- **Preview hidden below 360px** (REQ-NF-003): `windowWidth >= 360`,
  independent of the `state === Ready` gate (REQ-F-007) — both must hold.
- **400px correctness** (REQ-NF-004): value lines wrap (`Text.Wrap`, not
  elide) inside a `ColumnLayout` pinned to the scroll viewport's
  `availableWidth`; only the file-name heading elides (middle, REQ-F-003),
  since it's a single identifying token where truncation reads better than
  wrapping.
- **Width formula** matches REQ-NF-001 exactly: `min(480, W−24)`.

- **Close button** (REQ-F-015): `ViewerButton` is an inline component private to
  `Main.qml`, so it is not reachable from the new file. The popup declares its
  own small inline `component` using the same background recipe (hover/down/
  visualFocus ring) with `display: AbstractButton.IconOnly`, a new
  `apps/viewer/icons/close.svg` (same stroke style as `information.svg`,
  registered wherever the other icons are listed in `CMakeLists.txt`), and
  `Accessible.name: qsTr("Close Image Information")`.

---

## 6. Styling

| Element | Token |
|---|---|
| Section label ("CAMERA" etc.) | `HoloniightPalette.textSecondary` (REQ-F-008: distinct from value color) |
| Section value lines | `HoloniightPalette.textPrimary` |
| File name heading | `role: HnTypographyRole.Subheading`, `HoloniightPalette.textPrimary` |
| Summary / transformed / modified lines | `role: HnTypographyRole.Caption`, `HoloniightPalette.textSecondary` (metadata, not the primary heading) |
| Path text (FILE section) | `HoloniightPalette.textPrimary`, transparent background, focus ring `HoloniightPalette.borderFocus` — same recipe as today's `detailsText` |
| Popup background | `HoloniightPalette.surfaceRaised`, `border.color: HoloniightPalette.borderPassive`, `radius: HnMetrics.internalSpacing(HnControlSize.Compact)` — unchanged from today's information variant |
| Spacing | `HnMetrics.internalSpacing(HnControlSize.Normal)` between header/body and between sections; `...Compact` between a label and its first value line |

**Modal dimming (REQ-NF-005).** The installed Basic style's default
`Popup`/`Dialog` `Overlay.modal`
(`/usr/lib/qt6/qml/QtQuick/Controls/Basic/{Popup,Dialog}.qml`) is:

```qml
T.Overlay.modal: Rectangle { color: Color.transparent(control.palette.shadow, 0.5) }
```

alpha **0.5**. Shortcut Help never overrides `Overlay.modal`, so Help's dim
alpha is 0.5. `ImageInformationPopup.qml` overrides it explicitly:

```qml
Overlay.modal: Rectangle { color: Qt.alpha(HoloniightPalette.shadow, 0.22) }
```

(Implementation must confirm a `HoloniightPalette.shadow` token exists; if not, use `Qt.alpha(palette.shadow, 0.22)` like the Basic style.) 0.22 (using the theme's `shadow` token rather than the raw system
`palette.shadow`, for consistency with the rest of the app) is unambiguously
lower than 0.5 — REQ-NF-005's test only needs `informationAlpha <
helpAlpha`, and this leaves generous margin against future tuning of either
value.

---

## 7. Test plan

| REQ(s) | Test file | Assertion |
|---|---|---|
| F-001, C-002/003/004 | CI / manual | file exists; registered in `CMakeLists.txt`, `check-qml-format.sh`, `Taskfile.yml` |
| F-002 | `exif_metadata_test.cpp` | EXIF-rich JPEG, no-EXIF PNG, failed load, non-local document → correct `informationSections()`/`summaryLine()`/etc.; `informationText` no longer compiles |
| F-003 | `image_information_popup_test.cpp` | `informationFileName` elides middle for a long name at constrained width |
| F-004, F-014 | `exif_metadata_test.cpp` | `formatSummaryLine()`: full data; missing dimensions (MP omitted too); size-only; all-empty → `"Details unavailable"` |
| F-005 | `exif_metadata_test.cpp` | `formatTransformedLine()`: no transform → `""`; flip only → `""`; one clockwise rotate → `"Rotated view H × W"` |
| F-006 | `exif_metadata_test.cpp` | `QLocale::setDefault` × two locales; `formatModifiedText()` matches `QLocale().toString(…, ShortFormat)`; `""` for invalid `QDateTime` |
| F-007 | `image_information_popup_test.cpp` | `informationPreview` visible only when `Ready`; hidden during `Loading`/`Error` |
| F-008 | `image_information_popup_test.cpp` | section label `color == textSecondary`, differs from a value line's color |
| F-009, F-010 | `exif_metadata_test.cpp` | full EXIF fixture → 3 correct CAMERA lines; fixture missing lens+ISO → those tokens/separators cleanly omitted |
| F-011 | `exif_metadata_test.cpp` | location+altitude joined; altitude-only-missing; all-missing → section absent |
| F-012 | `exif_metadata_test.cpp` | `abbreviateHomePath` under home → `~/...`; outside home → unchanged; sibling `/home/alice2/x` vs. home `/home/alice` → unchanged |
| F-013 | `exif_metadata_test.cpp` | no-EXIF image → no CAMERA entry; non-local `open()` → no FILE entry |
| F-015 | `image_information_popup_test.cpp` | click `informationCloseButton` closes; Tab reaches it; `Accessible.name == "Close Image Information"` |
| F-016 | `static_workflow_test.cpp` | Escape while open closes popup only, `visibility` stays `FullScreen`; `]` works immediately after |
| F-017 | `image_information_popup_test.cpp` | click on `Overlay.overlay` outside popup closes it; click inside content does not |
| F-018 | `static_workflow_test.cpp` | `I` twice → opens then closes; `I` while Help open (`detailDialog === 2`) → stays closed |
| F-019 | `static_workflow_test.cpp` (≥2-image folder) | popup open + `]` → index unchanged; closed + `]` → index advances |
| F-020 | `image_information_popup_test.cpp` | popup open, `document.transform(1)` → `informationTransformed` updates live, no `qWarning` |
| F-021 | `accessibility_test.cpp` | `informationPopup.Accessible.name == "Image Information"` |
| F-022 | `accessibility_test.cpp` | section `Accessible.name`s == `"Camera"`/`"Location"`/`"File"` for an EXIF-rich local image |
| F-023 | `accessibility_test.cpp` | Tab reaches `informationCloseButton`; Space closes; role/name checks |
| F-024 | `accessibility_test.cpp` | focus `informationPathText`, Ctrl+A, Ctrl+C → clipboard equals displayed (abbreviated) text |
| NF-001 | `image_information_popup_test.cpp` | widths 1920/400/500 → popup width 480/376/476 |
| NF-002 | `image_information_popup_test.cpp` | 400×300, EXIF-rich with long path: `informationPathText` initially has a clipped cursor, `Tab` then `Ctrl+End`/`Ctrl+Home` keep cursor visible, and `informationHeaderRow.y` stays fixed |
| NF-003 | `image_information_popup_test.cpp` | 340px → preview not visible; 420px → visible |
| NF-004 | `image_information_popup_test.cpp` | 400px: every text item's `x + width <= content width` |
| NF-005 | `image_information_popup_test.cpp` | `Overlay.modal` alpha: Information < Help |
| C-001, C-005, C-006 | build + `grep` + full suite | no new deps; no bare user-visible literal outside `qsTr`/`tr`; full `ctest` passes |
| C-007 | `image_information_popup_test.cpp` | `StandaloneInformationPopup` fixture (same shape as `StandaloneFooter` in `footer_key_hints_test.cpp`): `loadFromModule("HolonightViewer", "ImageInformationPopup")`, no `ApplicationWindow`/`minimumWidth`, resize to 400px, assert no errors and nothing clipped |

New file `tests/image_information_popup_test.cpp` is added to the
`qt_add_executable(viewer-smoke …)` source list in `tests/CMakeLists.txt`
alongside `footer_key_hints_test.cpp`.

---

## 8. Key decisions, alternatives, risks

1. **Popup, not Dialog.** `Basic.Dialog` bundles a title bar and
   standard-button footer this design doesn't need — the file name *is* the
   heading, and the only footer-like element is the × button. A bare
   `Basic.Popup` gives full control over the fixed-header/scrolling-body
   split (§5) without fighting Dialog's `header`/`footer` slots.

2. **`QVariantList`/`QVariantMap` over `Q_GADGET`**, and **title-case labels
   upper-cased only for display** — both covered with rationale in §3.2.

3. **`I` toggle split between the window Action (open) and a popup-owned
   `Shortcut` (close).** The originally planned `canToggleInformation` gate,
   keeping the window Action enabled while open, cannot work: the modal popup
   blocks window shortcuts, so that Action never fires while the popup is open.
   A double toggle is impossible: the window Action is disabled and blocked
   while open, and the popup (inside an inactive `Loader`) does not exist while
   closed.

4. **Modal dimming alpha 0.22 vs. Help's 0.5** — verified against the
   installed Basic style sources (§6), not assumed. The test compares the
   two alphas relatively, so it stays correct even if either baseline moves.

5. **`Accessible.ignored` promotes children (project memory).** Setting it
   on the close button itself would promote its `contentItem` (the "×"
   glyph) and silently discard the button's `Accessible.name` — so the close
   button must **not** set it. Only decorative-only items (preview
   `Rectangle`/`ImageCanvas`, matching today's `informationThumbnail`) get
   `Accessible.ignored`, the same way `FooterKeyHints.qml`'s
   `HnKeyHint`/`Binding` pattern already silences a Control's children on
   purpose.

6. **Popup modality vs. window-level `Shortcut`s** — see §4; modal popups do
   block window shortcuts, which is why `I` closes via a popup-owned shortcut.

7. **New-QML-file tooling checklist (project memory).** `CMakeLists.txt`
   `QML_FILES`, `check-qml-format.sh`'s file loop, and `Taskfile.yml`'s
   `format` task's `qml-format.sh -i …` line all need the new filename.
   Missed entries pass a local build but fail `task format-check` — its own
   checklist item in the task breakdown, not folded into "write the QML
   file."

8. **`informationSections` rebuilds the whole `Repeater` on every
   `changed()`**, even for a pure `transform()` that doesn't touch EXIF.
   Acceptable: section counts are small (≤3), and it matches today's
   behavior where `detailsText.text` is fully recomputed on every
   `changed()` — an inherited property of `ImageDocument`'s single-signal
   design, not a regression.
