pragma ComponentBehavior: Bound

import QtQuick
import Holonight.Core
import Holonight.Controls

// Glyph and hint centred as one unit; the text keeps its size and the glyph gives way first.
Column {
    id: emptyStateGroup
    required property real availableWidth
    required property real availableHeight
    required property real devicePixelRatio
    objectName: "emptyStateGroup"
    spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
    // Below this the glyph reads as noise, so it hides and only the hint remains.
    readonly property real minimumGlyphSide: 48
    readonly property real hintHeight: emptyHintPrimary.implicitHeight + spacing + emptyHintSecondary.height
    readonly property real availableForGlyph: emptyStateGroup.availableHeight - hintHeight - spacing - 2 * emptyDecoration.padding

    HnIcon {
        id: emptyDecoration
        objectName: "emptyState"
        readonly property real shorterDimension: Math.min(emptyStateGroup.availableWidth, emptyStateGroup.availableHeight)
        readonly property real padding: Math.max(32, Math.min(64, shorterDimension * 0.08))
        readonly property int side: Math.max(0, Math.floor(Math.min(shorterDimension - 2 * padding, emptyStateGroup.availableForGlyph)))
        readonly property int rasterLimit: Math.max(1, Math.floor(1024 / emptyStateGroup.devicePixelRatio))
        anchors.horizontalCenter: parent.horizontalCenter
        visible: side >= emptyStateGroup.minimumGlyphSide
        source: side > 0 ? Qt.resolvedUrl("../icons/empty-viewer.svg") : ""
        rendering: HnIcon.Semantic
        // Qt scales sourceSize by DPR; hnicons accepts at most 1024 physical pixels.
        size: Math.min(side, rasterLimit)
        width: side
        height: side
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
        rawText: qsTranslate("Main", "No image open")
        Accessible.role: Accessible.StaticText
        Accessible.name: rawText
    }
    HnLabel {
        id: emptyHintSecondary
        objectName: "emptyStateHintSecondary"
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(implicitWidth, emptyStateGroup.availableWidth - 32)
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textMuted
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        rawText: qsTranslate("Main", "Ctrl+O to open · or drop an image here")
        Accessible.role: Accessible.StaticText
        Accessible.name: rawText
    }
}
