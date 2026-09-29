pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightViewer

Item {
    id: root
    objectName: "thumbnailCell"

    required property int index
    required property string fileName
    required property url url
    property bool isSelected: false
    // Set by the grid to image://thumbnail/... for this file; empty until then.
    property url source
    property real padding: 8
    // Free space around the tinted tile, half on each side, so neighbouring tiles do not touch.
    property real spacing: 0
    property real labelGap: 6
    readonly property real boxSize: ThumbnailMetrics.boxSize
    readonly property real radius: HnAppearance.roundedRadius(HnSurfaceRole.Control, width, height, HnAppearance.revision)

    signal clicked
    signal doubleClicked

    implicitWidth: boxSize + 2 * padding + spacing
    implicitHeight: padding + boxSize + labelGap + label.implicitHeight + padding + spacing

    Accessible.role: Accessible.ListItem
    Accessible.name: root.fileName
    Accessible.selected: root.isSelected

    Rectangle {
        id: background
        objectName: "cellBackground"
        anchors.fill: parent
        anchors.margins: root.spacing / 2
        radius: root.radius
        color: root.isSelected ? HoloniightPalette.surfaceSelected : (hover.hovered ? HoloniightPalette.surfaceHover : "transparent")
        Accessible.ignored: true
    }

    Item {
        id: thumbnailBox
        x: root.spacing / 2 + root.padding
        y: root.spacing / 2 + root.padding
        width: root.boxSize
        height: root.boxSize
        Accessible.ignored: true

        Rectangle {
            id: placeholder
            objectName: "cellPlaceholder"
            anchors.fill: parent
            radius: root.radius
            color: HoloniightPalette.surfaceElevated
            visible: thumbnail.status !== Image.Ready && thumbnail.status !== Image.Error
            Accessible.ignored: true
        }

        Image {
            id: thumbnail
            objectName: "cellThumbnail"
            anchors.centerIn: parent
            // The provider reports the logical size as the implicit size, and a small source is never enlarged.
            width: Math.min(implicitWidth, thumbnailBox.width)
            height: Math.min(implicitHeight, thumbnailBox.height)
            fillMode: Image.PreserveAspectFit
            // The provider's cache is the only thumbnail cache.
            cache: false
            asynchronous: true
            smooth: true
            source: root.source
            visible: status === Image.Ready
            Accessible.ignored: true
        }

        HnIcon {
            id: brokenGlyph
            objectName: "cellBrokenGlyph"
            anchors.centerIn: parent
            visible: thumbnail.status === Image.Error
            source: Qt.resolvedUrl("../icons/broken-image.svg")
            rendering: HnIcon.Semantic
            iconState: HnIcon.Muted
            size: root.boxSize / 4
            Accessible.ignored: true
        }
    }

    HnLabel {
        id: label
        objectName: "cellLabel"
        x: root.spacing / 2 + root.padding
        y: root.spacing / 2 + root.padding + root.boxSize + root.labelGap
        width: root.boxSize
        horizontalAlignment: Text.AlignHCenter
        textFormat: Text.PlainText
        elide: Text.ElideMiddle
        maximumLineCount: 1
        rawText: root.fileName
        color: root.isSelected ? HoloniightPalette.textPrimary : HoloniightPalette.textSecondary
        Accessible.ignored: true
    }

    Rectangle {
        id: focusRing
        objectName: "cellFocusRing"
        anchors.fill: parent
        anchors.margins: root.spacing / 2
        radius: root.radius
        color: "transparent"
        border.width: HnMetrics.focusBorderWidth
        border.color: HoloniightPalette.borderFocus
        visible: root.isSelected
        Accessible.ignored: true
    }

    HoverHandler {
        id: hover
    }

    Controls.ToolTip.text: root.fileName
    Controls.ToolTip.delay: 500
    Controls.ToolTip.visible: hover.hovered && label.truncated

    TapHandler {
        onTapped: root.clicked()
        onDoubleTapped: root.doubleClicked()
    }
}
