pragma ComponentBehavior: Bound

import QtQuick
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
    readonly property var hints: [
        {
            name: "Navigate",
            keyGroups: [[Qt.Key_BracketLeft], [Qt.Key_BracketRight]],
            label: qsTr("Navigate")
        },
        {
            name: "Zoom",
            keyGroups: [[Qt.Key_Control, Qt.Key_Plus], [Qt.Key_Control, Qt.Key_Minus]],
            label: qsTr("Zoom")
        },
        {
            name: "Fit",
            keyGroups: [[Qt.Key_Control, Qt.Key_0]],
            label: qsTr("Fit")
        },
        {
            name: "ActualSize",
            keyGroups: [[Qt.Key_1]],
            label: qsTr("100%")
        },
        {
            name: "Rotate",
            keyGroups: [[Qt.Key_R]],
            label: qsTr("Rotate")
        },
        {
            name: "Fullscreen",
            keyGroups: [[Qt.Key_F]],
            label: qsTr("Fullscreen")
        }
    ].concat(root.animated ? [
        {
            name: "PlayPause",
            keyGroups: [[Qt.Key_Space]],
            label: qsTr("Play/Pause")
        }
    ] : []).concat([
        {
            name: "Help",
            keyGroups: [[Qt.Key_Question]],
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
                    objectName: hintRow.objectName + "Keycap"
                    anchors.verticalCenter: parent.verticalCenter
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
