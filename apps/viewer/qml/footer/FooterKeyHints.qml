pragma ComponentBehavior: Bound

import QtQuick
import Holonight.Core
import Holonight.Controls

Flow {
    id: root
    objectName: "footer"
    spacing: 12

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

    Repeater {
        model: root.hints
        delegate: Row {
            id: hintRow
            required property var modelData
            objectName: "footerHint" + modelData.name
            spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
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
