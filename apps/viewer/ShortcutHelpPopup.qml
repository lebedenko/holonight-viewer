pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Holonight as HnStyle
import Holonight.Core
import Holonight.Controls

// Modal card listing the viewer's shortcuts; Main.qml owns when it is open.
Basic.Popup {
    id: root
    objectName: "shortcutHelpPopup"
    // The popup is centered in its parent (the window overlay), so the parent stands in for the window.
    readonly property real windowWidth: parent ? parent.width : 0
    readonly property real windowHeight: parent ? parent.height : 0
    // Shared column for aligned descriptions; long keycaps wrap within it.
    readonly property real keyColumnWidth: 140
    // Section keys are stable ids for objectNames; labels and row text are display strings.
    readonly property var sections: [
        {
            key: "Navigation",
            label: qsTr("Navigation"),
            rows: [
                {
                    key: qsTr("Ctrl+O"),
                    description: qsTr("Open image"),
                    keycap: true
                },
                {
                    key: qsTr("[ / ]"),
                    description: qsTr("Previous / next image"),
                    keycap: true
                },
                {
                    key: qsTr("Ctrl+R"),
                    description: qsTr("Refresh folder and image"),
                    keycap: true
                }
            ]
        },
        {
            key: "View",
            label: qsTr("View"),
            rows: [
                {
                    key: qsTr("Ctrl+0"),
                    description: qsTr("Fit"),
                    keycap: true
                },
                {
                    key: qsTr("1"),
                    description: qsTr("Actual size"),
                    keycap: true
                },
                {
                    key: qsTr("Ctrl++ / Ctrl+−"),
                    description: qsTr("Zoom"),
                    keycap: true
                },
                {
                    key: qsTr("F"),
                    description: qsTr("Fullscreen"),
                    keycap: true
                },
                {
                    key: qsTr("Esc"),
                    description: qsTr("Close dialog or leave fullscreen"),
                    keycap: true
                }
            ]
        },
        {
            key: "Transform",
            label: qsTr("Transform"),
            rows: [
                {
                    key: qsTr("R / Shift+R"),
                    description: qsTr("Rotate clockwise/counterclockwise"),
                    keycap: true
                },
                {
                    key: qsTr("X / Shift+X"),
                    description: qsTr("Flip horizontally/vertically"),
                    keycap: true
                }
            ]
        },
        {
            key: "Image",
            label: qsTr("Image"),
            rows: [
                {
                    key: qsTr("I"),
                    description: qsTr("Image information"),
                    keycap: true
                },
                {
                    key: qsTr("Ctrl+C"),
                    description: qsTr("Copy image"),
                    keycap: true
                },
                {
                    key: qsTr("Ctrl+Shift+C"),
                    description: qsTr("Copy path"),
                    keycap: true
                }
            ]
        },
        {
            key: "Application",
            label: qsTr("Application"),
            rows: [
                {
                    key: qsTr("?"),
                    description: qsTr("Toggle this help"),
                    keycap: true
                },
                {
                    key: qsTr("Q"),
                    description: qsTr("Quit"),
                    keycap: true
                }
            ]
        },
        {
            key: "Mouse",
            label: qsTr("Mouse"),
            rows: [
                {
                    key: qsTr("Wheel/touchpad scroll"),
                    description: qsTr("Zoom at pointer"),
                    keycap: false
                },
                {
                    key: qsTr("Left-drag"),
                    description: qsTr("Pan"),
                    keycap: false
                },
                {
                    key: qsTr("Arrow keys"),
                    description: qsTr("Pan"),
                    keycap: true
                },
                {
                    key: qsTr("Drop an image"),
                    description: qsTr("Open image"),
                    keycap: false
                }
            ]
        }
    ]
    anchors.centerIn: parent
    width: Math.min(480, windowWidth - 24)
    height: Math.min(implicitHeight, windowHeight - 24)
    padding: 16
    modal: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    // Same light dimming as Image Information, so the image stays visible.
    Basic.Overlay.modal: Rectangle {
        objectName: "shortcutHelpDimmer"
        color: Qt.alpha(HoloniightPalette.shadow, 0.22)
    }
    background: Rectangle {
        color: HoloniightPalette.surfaceRaised
        border.width: HnMetrics.borderWidth
        border.color: HoloniightPalette.borderPassive
        radius: HnMetrics.internalSpacing(HnControlSize.Compact)
    }
    // Popup is not an Item, so the dialog's accessible identity lives on its content.
    contentItem: ColumnLayout {
        objectName: "shortcutHelpContent"
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Shortcut Help")

        // A modal popup blocks the window's ? action, so the toggle's closing half lives here.
        Shortcut {
            sequence: "?"
            enabled: root.opened
            onActivated: root.close()
        }

        // Sibling of the scroll view, so the header never scrolls away.
        RowLayout {
            objectName: "shortcutHelpHeaderRow"
            Layout.fillWidth: true
            spacing: 16
            HnLabel {
                objectName: "shortcutHelpTitle"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                role: HnTypographyRole.Subheading
                textFormat: Text.PlainText
                rawText: qsTr("Shortcuts")
                elide: Text.ElideRight
            }
            HnStyle.Button {
                id: closeButton
                objectName: "shortcutHelpCloseButton"
                Layout.alignment: Qt.AlignTop
                display: Basic.AbstractButton.IconOnly
                icon.source: "icons/close.svg"
                icon.width: HnMetrics.iconSize(HnControlSize.Normal)
                icon.height: HnMetrics.iconSize(HnControlSize.Normal)
                implicitWidth: HnMetrics.controlHeight(HnControlSize.Normal)
                implicitHeight: HnMetrics.controlHeight(HnControlSize.Normal)
                Accessible.name: qsTr("Close Shortcut Help")
                background: Rectangle {
                    color: closeButton.down ? HoloniightPalette.surface : closeButton.hovered ? HoloniightPalette.surfaceHover : "transparent"
                    border.width: closeButton.visualFocus ? HnMetrics.focusBorderWidth : 0
                    border.color: closeButton.visualFocus ? HoloniightPalette.borderFocus : "transparent"
                    radius: HnMetrics.internalSpacing(HnControlSize.Compact)
                }
                onClicked: root.close()
            }
        }

        Basic.ScrollView {
            id: scroll
            objectName: "shortcutHelpScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                objectName: "shortcutHelpBody"
                width: scroll.availableWidth
                spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
                Repeater {
                    model: root.sections
                    delegate: ColumnLayout {
                        id: section
                        required property var modelData
                        objectName: "shortcutHelpSection" + modelData.key
                        Layout.fillWidth: true
                        spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
                        Accessible.role: Accessible.Grouping
                        Accessible.name: modelData.label
                        HnLabel {
                            objectName: "shortcutHelpSectionLabel" + section.modelData.key
                            Layout.fillWidth: true
                            role: HnTypographyRole.Caption
                            color: HoloniightPalette.textSecondary
                            textFormat: Text.PlainText
                            font.letterSpacing: 1.5
                            rawText: section.modelData.label.toUpperCase()
                            Accessible.ignored: true
                        }
                        Repeater {
                            model: section.modelData.rows
                            delegate: RowLayout {
                                id: rowItem
                                required property var modelData
                                required property int index
                                objectName: "shortcutHelpRow" + section.modelData.key + index
                                Layout.fillWidth: true
                                spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
                                Accessible.role: Accessible.StaticText
                                Accessible.name: modelData.key + " " + modelData.description
                                // Fixed key column so descriptions align across rows.
                                Item {
                                    objectName: rowItem.objectName + "Key"
                                    // Capped from the body width, not the row's own width, which the layout itself assigns.
                                    Layout.preferredWidth: Math.min(root.keyColumnWidth, scroll.availableWidth / 2)
                                    Layout.preferredHeight: rowItem.modelData.keycap ? keycap.implicitHeight : plainKey.implicitHeight
                                    Layout.alignment: Qt.AlignVCenter
                                    HnKeyHint {
                                        id: keycap
                                        objectName: rowItem.objectName + "Keycap"
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
                                        objectName: rowItem.objectName + "Gesture"
                                        visible: !rowItem.modelData.keycap
                                        width: parent.width
                                        color: HoloniightPalette.textSecondary
                                        textFormat: Text.PlainText
                                        wrapMode: Text.Wrap
                                        rawText: rowItem.modelData.key
                                        Accessible.ignored: true
                                    }
                                }
                                HnLabel {
                                    objectName: rowItem.objectName + "Description"
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    color: HoloniightPalette.textPrimary
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    rawText: rowItem.modelData.description
                                    Accessible.ignored: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
