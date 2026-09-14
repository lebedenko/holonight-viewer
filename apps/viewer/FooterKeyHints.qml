pragma ComponentBehavior: Bound

import QtQuick
import Holonight.Core
import Holonight.Controls

Flow {
    id: root
    objectName: "footer"
    spacing: 12

    readonly property var hints: [
        {
            name: "Navigate",
            key: qsTr("[ / ]"),
            label: qsTr("Navigate")
        },
        {
            name: "Zoom",
            key: qsTr("Ctrl++/−"),
            label: qsTr("Zoom")
        },
        {
            name: "Fit",
            key: qsTr("Ctrl+0"),
            label: qsTr("Fit")
        },
        {
            name: "ActualSize",
            key: qsTr("1"),
            label: qsTr("100%")
        },
        {
            name: "Rotate",
            key: qsTr("R"),
            label: qsTr("Rotate")
        },
        {
            name: "Fullscreen",
            key: qsTr("F"),
            label: qsTr("Fullscreen")
        },
        {
            name: "Help",
            key: qsTr("?"),
            label: qsTr("Help")
        }
    ]

    Repeater {
        model: root.hints
        delegate: Row {
            id: hintRow
            required property var modelData
            objectName: "footerHint" + modelData.name
            spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
            Accessible.role: Accessible.StaticText
            Accessible.name: modelData.key + " " + modelData.label

            HnKeyHint {
                id: keycap
                objectName: hintRow.objectName + "Keycap"
                anchors.verticalCenter: parent.verticalCenter
                text: hintRow.modelData.key
                Accessible.ignored: true

                // Ignoring a control promotes its children, so hide the keycap text too.
                Binding {
                    target: keycap.contentItem
                    property: "Accessible.ignored"
                    value: true
                }
            }
            HnLabel {
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
