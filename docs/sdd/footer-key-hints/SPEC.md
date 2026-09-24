# HoloNight Viewer Footer Key Hints — Requirements Specification

## Overview

This specification defines the restyle and content update of the shortcut footer in HoloNight Viewer (QML, apps/viewer/Main.qml ~line 807). The footer displays seven keyboard-hint controls, each pairing a keycap visual with a translatable label. This work addresses design review findings that the footer reads as documentation rather than intuitive UI; keycaps will visually distinguish keys from descriptions, and redundant hints (Quit, Information) will be removed.

## Scope & Non-Goals

**In scope:**
- Restyle footer hints using HnKeyHint keycap component + caption labels
- Content: exactly seven hints in the specified order
- Accessibility exposure and focus policies
- Automated verification test

**Out of scope:**
- Transient or auto-hide footer behaviour
- Header height or layout changes
- Menu, help, or information dialog redesign
- Keyboard shortcut remapping
- Pixel-exact visual regression tests

## Functional Requirements

**REQ-F-001:** The footer shall contain exactly seven keyboard hints in this order: Navigate, Zoom, Fit, 100%, Rotate, Fullscreen, Help. Narrow layouts may hide hints as specified in REQ-F-005.

- **Acceptance:** Automated test verifies element count, text content, and order match the specification list; no Quit or Information hints present.

**REQ-F-002:** Each hint shall consist of one HnKeyHint keycap containing the whole key combination, immediately followed by a muted caption label, vertically centred with the keycap. The spacing between a keycap and its label shall be smaller than the spacing between adjacent hints.

- **Acceptance:** Automated test finds, per hint, exactly one keycap whose text is the key string and one label whose text is the label string, with label x > keycap x, and |keycap vertical centre − label vertical centre| ≤ 1 px; the footer's inter-hint spacing is greater than the intra-hint spacing.

**REQ-F-003:** Keycap text shall match the actual keyboard shortcuts: "[ / ]", "Ctrl++ / Ctrl+-", "Ctrl+0", "1", "R", "F", "?".

- **Acceptance:** Automated test compares keycap text to the authoritative list; no deviation detected.

**REQ-F-004:** Label text shall be capitalized and wrapped with `qsTr()` for translation support (e.g., qsTr("Navigate")).

- **Acceptance:** Every key and label string in the footer model is wrapped in `qsTr()`.

**REQ-F-005:** The footer shall stay on one line so it does not consume more canvas height in a narrow window. When all hints do not fit, it shall hide whole hints from the right, excluding Help, until the visible hints fit. Help shall remain visible at every supported window width. Widening the window shall restore hidden hints in their original order.

- **Acceptance:** A standalone footer narrowed from 1000 px to 400 px stays on one line, keeps Help visible, and hides a suffix of the other hints without splitting keycaps from labels. Visible hints fit within the footer width. Widening it to 1000 px restores all seven hints.

**REQ-F-006:** All colours and fonts shall be sourced from the shared theme (`HoloniightPalette`, `HnTypographyRole`, HnKeyHint defaults), never hard-coded.

- **Acceptance:** The footer region of Main.qml contains no colour literals (`#…`, `Qt.rgba(`, `"white"` etc.) and no `font.family`/`font.pixelSize`/`font.pointSize` assignments.

## Non-Functional Requirements

**REQ-NF-001:** The footer shall remain persistently visible (not transient or auto-hidden).

- **Acceptance:** Automated test asserts the footer is visible in the empty state, after an image loads, and after mouse movement plus a 6 s wait; no timer or opacity animation is bound to it.

**REQ-NF-002:** Each visible hint shall be exposed to the accessibility tree as a single StaticText node; its accessible name shall be "<key> <label>" (e.g. "Ctrl+0 Fit"). Hidden hints shall not be announced.

- **Acceptance:** At a width that fits all hints, an automated test via QAccessible finds seven footer StaticText nodes named "[ / ] Navigate" … "? Help"; no accessible node named just a key string (e.g. "Ctrl+0") or just a label (e.g. "Fit") exists under the footer. At narrow widths, only visible hints are announced and Help remains available.

**REQ-NF-003:** Hints shall never be keyboard-focusable; Tab and Shift+Tab shall continue to cycle only header buttons.

- **Acceptance:** Automated test asserts no footer item has `activeFocusOnTab` or a focus policy other than NoFocus; the existing Tab-cycle test still passes.

## Constraints

**REQ-C-001:** The footer may grow in height because HnKeyHint is taller than a caption line; this shall not break existing behaviour.

- **Acceptance:** The full existing test suite passes unchanged (tests may only be adjusted if they asserted the removed Quit/Information footer text or an exact footer height).

**REQ-C-002:** The implementation shall not modify any keyboard shortcut definition, the header, the canvas, the menu, or the dialogs.

- **Acceptance:** `git diff` of `apps/viewer/` touches only the footer block of Main.qml (plus an optional new footer QML file registered in apps/viewer/CMakeLists.txt).

**REQ-C-003:** HnKeyHint component from holonight-qt library must exist and export its expected properties (text, Accessible.role, Accessible.name).

- **Acceptance:** Build and qmllint succeed with no unresolved-type or missing-property warnings for HnKeyHint; no new holonight-qt dependency version is required.

## Verification

- **Automated test:** Located in tests/, asserts footer objectName, exactly seven hints with correct key/label text/order, absence of Quit/Information, no focusable hints, accessible names pattern.
- **Visual verification:** Screenshot at normal and narrow window widths; keycaps visually distinct, labels readable, no overflow.
- **Regression:** Existing geometry and functional tests pass; no unintended changes to canvas or header.
- **Documentation:** README updated if any user-facing footer behaviour documented.
