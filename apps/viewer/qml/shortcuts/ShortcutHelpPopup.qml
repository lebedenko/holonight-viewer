pragma ComponentBehavior: Bound

import QtQuick
import HolonightViewer
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

// Modal card listing the viewer's shortcuts; Main.qml owns when it is open.
Controls.Popup {
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
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Open),
                    description: qsTr("Open image"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Paste),
                    description: qsTr("Paste image path"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Previous).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.Next)),
                    description: qsTr("Previous / next image"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.Previous).concat(ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.Next)),
                    description: qsTr("Aliases: previous / next image (single view)"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Refresh),
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
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Fit),
                    description: qsTr("Fit"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.Fit),
                    description: qsTr("Alias: Fit"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.ActualSize),
                    description: qsTr("Actual size"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomIn).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomOut)),
                    description: qsTr("Zoom"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.ZoomIn),
                    description: qsTr("Aliases: zoom in"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.ZoomOut),
                    description: qsTr("Alias: zoom out"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Fullscreen),
                    description: qsTr("Fullscreen"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Escape),
                    description: qsTr("Close dialog, grid or leave fullscreen"),
                    keycap: true
                }
            ]
        },
        {
            key: "Grid",
            label: qsTr("Grid"),
            rows: [
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Grid),
                    description: qsTr("Toggle grid view"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.GridLeft).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.GridDown), ViewerShortcuts.keyGroups(ViewerShortcuts.GridUp), ViewerShortcuts.keyGroups(ViewerShortcuts.GridRight)),
                    description: qsTr("Move selection"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.GridLeft).concat(ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.GridDown), ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.GridUp), ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.GridRight)),
                    description: qsTr("Aliases: move selection"),
                    keycap: true
                },
                {
                    key: "gg",
                    description: qsTr("First item"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.GridLast),
                    description: qsTr("Last item"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.PageUp),
                    description: qsTr("Half page up"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.PageDown),
                    description: qsTr("Half page down"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Activate),
                    description: qsTr("Open selected image"),
                    keycap: true
                }
            ]
        },
        {
            key: "Transform",
            label: qsTr("Transform"),
            rows: [
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateClockwise).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.RotateCounterclockwise)),
                    description: qsTr("Rotate clockwise/counterclockwise"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.FlipHorizontal).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.FlipVertical)),
                    description: qsTr("Flip horizontally/vertically"),
                    keycap: true
                }
            ]
        },
        {
            key: "Playback",
            label: qsTr("Playback"),
            rows: [
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Playback),
                    description: qsTr("Toggle play/pause"),
                    keycap: true
                }
            ]
        },
        {
            key: "Image",
            label: qsTr("Image"),
            rows: [
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Information),
                    description: qsTr("Image information"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyImage),
                    description: qsTr("Copy image"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyPath),
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
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Help),
                    description: qsTr("Toggle this help"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.MenuDown).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.MenuUp)),
                    description: qsTr("Move through menu"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.MenuDown).concat(ViewerShortcuts.aliasKeyGroups(ViewerShortcuts.MenuUp)),
                    description: qsTr("Aliases: move through menu"),
                    keycap: true
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.Quit),
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
                    key: qsTr("Back / Forward buttons"),
                    description: qsTr("Previous / next image or grid selection"),
                    keycap: false
                },
                {
                    key: qsTr("Left-drag"),
                    description: qsTr("Pan"),
                    keycap: false
                },
                {
                    keyGroups: ViewerShortcuts.keyGroups(ViewerShortcuts.PanLeft).concat(ViewerShortcuts.keyGroups(ViewerShortcuts.PanRight), ViewerShortcuts.keyGroups(ViewerShortcuts.PanUp), ViewerShortcuts.keyGroups(ViewerShortcuts.PanDown)),
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
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    // Same light dimming as Image Information, so the image stays visible.
    Controls.Overlay.modal: Rectangle {
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
            sequence: ViewerShortcuts.sequence(ViewerShortcuts.Help)
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
            Controls.Button {
                id: closeButton
                objectName: "shortcutHelpCloseButton"
                Layout.alignment: Qt.AlignTop
                display: Controls.AbstractButton.IconOnly
                icon.source: "../icons/close.svg"
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

        Controls.ScrollView {
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
                                Accessible.name: (modelData.keycap ? keycap.accessibleText : modelData.key) + " " + modelData.description
                                // Fixed key column so descriptions align across rows.
                                Item {
                                    objectName: rowItem.objectName + "Key"
                                    // Capped from the body width, not the row's own width, which the layout itself assigns.
                                    Layout.preferredWidth: Math.min(root.keyColumnWidth, scroll.availableWidth / 2)
                                    Layout.preferredHeight: rowItem.modelData.keycap ? keycap.implicitHeight : plainKey.implicitHeight
                                    Layout.alignment: Qt.AlignVCenter
                                    HnKeyHint {
                                        id: keycap
                                        lowercaseLetters: true
                                        objectName: rowItem.objectName + "Keycap"
                                        visible: rowItem.modelData.keycap
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Math.min(implicitWidth, parent.width)
                                        text: rowItem.modelData.key ?? ""
                                        keyGroups: rowItem.modelData.keyGroups ?? []
                                        wrap: true
                                        Accessible.ignored: true
                                    }
                                    HnLabel {
                                        id: plainKey
                                        objectName: rowItem.objectName + "Gesture"
                                        visible: !rowItem.modelData.keycap
                                        width: parent.width
                                        color: HoloniightPalette.textSecondary
                                        textFormat: Text.PlainText
                                        wrapMode: Text.Wrap
                                        rawText: rowItem.modelData.key ?? ""
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
