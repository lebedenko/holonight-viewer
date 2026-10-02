pragma ComponentBehavior: Bound

import QtQuick
import HolonightViewer
import Holonight.Core
import Holonight.Controls

Item {
    id: root
    objectName: "footer"
    implicitHeight: row.implicitHeight
    // Gap between hints; each keycap/label pair uses a tighter internal spacing.
    property real spacing: 12

    // Set while the current image is an animation that can be paused.
    property bool animated: false
    // Grid view shows navigation hints instead of the picture actions.
    property bool gridMode: false
    readonly property var hints: gridMode ? gridHints : singleViewHints
    readonly property var gridHints: [
        {
            name: "GridNavigate",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.GridLeft).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.GridDown), ViewerShortcuts.keyGroups(ViewerShortcuts.GridUp), ViewerShortcuts.keyGroups(ViewerShortcuts.GridRight)),
            label: qsTr("Move")
        },
        {
            name: "First",
            keyGroups: [],
            text: "gg",
            label: qsTr("First")
        },
        {
            name: "Last",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.GridLast),
            label: qsTr("Last")
        },
        {
            name: "GridPage",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.PageUp).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.PageDown)),
            label: qsTr("Page")
        },
        {
            name: "GridOpen",
            // Key_Enter is spelled "Enter"; Key_Return would read "Return".
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Activate),
            label: qsTr("Open")
        },
        {
            name: "GridToggle",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Grid),
            label: qsTr("Grid")
        },
        {
            name: "Fullscreen",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Fullscreen),
            label: qsTr("Fullscreen")
        },
        {
            name: "Help",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Help),
            label: qsTr("Help")
        }
    ]
    readonly property var singleViewHints: [
        {
            name: "Navigate",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Previous).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.Next)),
            label: qsTr("Navigate")
        },
        {
            name: "Zoom",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomIn).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomOut)),
            label: qsTr("Zoom")
        },
        {
            name: "Fit",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Fit),
            label: qsTr("Fit")
        },
        {
            name: "ActualSize",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.ActualSize),
            label: qsTr("100%")
        },
        {
            name: "Rotate",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateClockwise),
            label: qsTr("Rotate")
        },
        {
            name: "Fullscreen",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Fullscreen),
            label: qsTr("Fullscreen")
        }
    ].concat(root.animated ? [
        {
            name: "PlayPause",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Playback),
            label: qsTr("Play/Pause")
        }
    ] : []).concat([
        {
            name: "Help",
            keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Help),
            label: qsTr("Help")
        }
    ])

    // Measured width of each hint, indexed like `hints`.
    property var hintWidths: []
    // Longest prefix of hints that fits; the last hint (Help) is always shown.
    readonly property int fittingCount: {
        const last = root.hints.length - 1;
        let used = root.hintWidths[last] ?? 0;
        let count = 0;
        while (count < last) {
            const next = used + root.spacing + (root.hintWidths[count] ?? 0);
            if (next > root.width) {
                break;
            }
            used = next;
            ++count;
        }
        return count;
    }

    function reportWidth(index: int, width: real): void {
        const widths = root.hintWidths.slice();
        widths[index] = width;
        root.hintWidths = widths;
    }

    Row {
        id: row
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: root.spacing

        Repeater {
            id: repeater
            model: root.hints
            delegate: Row {
                id: hintRow
                required property var modelData
                required property int index
                readonly property real fullWidth: keycap.implicitWidth + spacing + hintLabel.implicitWidth
                objectName: "footerHint" + modelData.name
                visible: index === root.hints.length - 1 || index < root.fittingCount
                spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
                onFullWidthChanged: root.reportWidth(index, fullWidth)
                Component.onCompleted: root.reportWidth(index, fullWidth)
                Accessible.role: Accessible.StaticText
                Accessible.name: keycap.accessibleText + " " + modelData.label

                HnKeyHint {
                    id: keycap
                    lowercaseLetters: true
                    objectName: hintRow.objectName + "Keycap"
                    anchors.verticalCenter: parent.verticalCenter
                    text: hintRow.modelData.text ?? ""
                    keyGroups: hintRow.modelData.keyGroups
                    font.pointSize: hintLabel.font.pointSize
                    Accessible.ignored: true
                }
                HnLabel {
                    id: hintLabel
                    objectName: hintRow.objectName + "Label"
                    anchors.verticalCenter: parent.verticalCenter
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                    textFormat: Text.PlainText
                    rawText: hintRow.modelData.label
                    Accessible.ignored: true
                }
            }
        }
    }
}
