pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import Holonight as HnStyle
import Holonight.Core

// Modal card with structured facts about the current image; Main.qml owns when it is open.
Basic.Popup {
    id: root
    objectName: "informationPopup"
    required property ImageDocument document
    // The popup is centered in its parent (the window overlay), so the parent stands in for the window.
    readonly property real windowWidth: parent ? parent.width : 0
    readonly property real windowHeight: parent ? parent.height : 0
    // FILE is laid out separately from the text-only sections.
    readonly property var textSections: document.informationSections.filter(section => section.key !== "File")
    readonly property var fileSection: document.informationSections.find(section => section.key === "File") ?? null
    anchors.centerIn: parent
    width: Math.min(480, windowWidth - 24)
    height: Math.min(implicitHeight, windowHeight - 24)
    padding: 16
    component SectionLabel: HnLabel {
        Layout.fillWidth: true
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textSecondary
        textFormat: Text.PlainText
        font.letterSpacing: 1.5
        Accessible.ignored: true
    }
    modal: true
    focus: true
    closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
    // Light dimming, shared with Shortcut Help, so the image stays visible.
    Basic.Overlay.modal: Rectangle {
        objectName: "informationDimmer"
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
        objectName: "informationContent"
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Image Information")

        // A modal popup blocks the window's I action, so the toggle's closing half lives here.
        Shortcut {
            sequence: "I"
            enabled: root.opened
            onActivated: root.close()
        }

        // Siblings of the scroll view, so the header never scrolls away.
        RowLayout {
            objectName: "informationHeaderRow"
            Layout.fillWidth: true
            spacing: 16
            Rectangle {
                objectName: "informationPreview"
                visible: root.document.state === ImageDocument.Ready && root.windowWidth >= 360
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                color: HoloniightPalette.surface
                border.width: HnMetrics.borderWidth
                border.color: HoloniightPalette.borderPassive
                Accessible.ignored: true
                ImageCanvas {
                    anchors.fill: parent
                    anchors.margins: HnMetrics.borderWidth
                    image: root.document.image
                    orientation: root.document.orientation
                    displayPixelRatio: Screen.devicePixelRatio
                    enabled: false
                    Accessible.ignored: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 2
                HnLabel {
                    objectName: "informationFileName"
                    Layout.fillWidth: true
                    role: HnTypographyRole.Subheading
                    textFormat: Text.PlainText
                    rawText: root.document.fileName
                    elide: Text.ElideMiddle
                }
                HnLabel {
                    objectName: "informationSummary"
                    Layout.fillWidth: true
                    role: HnTypographyRole.Caption
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    rawText: root.document.summaryLine
                }
                HnLabel {
                    objectName: "informationTransformed"
                    Layout.fillWidth: true
                    visible: rawText.length > 0
                    role: HnTypographyRole.Caption
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    rawText: root.document.transformedLine
                }
                HnLabel {
                    objectName: "informationModified"
                    Layout.fillWidth: true
                    visible: rawText.length > 0
                    role: HnTypographyRole.Caption
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    rawText: root.document.modifiedText
                }
            }
            HnStyle.Button {
                id: closeButton
                objectName: "informationCloseButton"
                Layout.alignment: Qt.AlignTop
                display: Basic.AbstractButton.IconOnly
                icon.source: "icons/close.svg"
                icon.width: HnMetrics.iconSize(HnControlSize.Normal)
                icon.height: HnMetrics.iconSize(HnControlSize.Normal)
                implicitWidth: HnMetrics.controlHeight(HnControlSize.Normal)
                implicitHeight: HnMetrics.controlHeight(HnControlSize.Normal)
                Accessible.name: qsTr("Close Image Information")
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
            objectName: "informationScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: sections.count > 0 || root.fileSection !== null
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth
                spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
                Repeater {
                    id: sections
                    model: root.textSections
                    delegate: ColumnLayout {
                        id: section
                        required property var modelData
                        objectName: "informationSection" + modelData.key
                        Layout.fillWidth: true
                        spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
                        Accessible.role: Accessible.Grouping
                        Accessible.name: modelData.label
                        SectionLabel {
                            objectName: "informationSectionLabel" + section.modelData.key
                            rawText: section.modelData.label.toUpperCase()
                        }
                        Repeater {
                            model: section.modelData.lines
                            delegate: HnLabel {
                                required property string modelData
                                Layout.fillWidth: true
                                color: HoloniightPalette.textPrimary
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                rawText: modelData
                            }
                        }
                    }
                }
                // Declared directly rather than by the Repeater, so the selectable path is a plain child item.
                ColumnLayout {
                    objectName: "informationSectionFile"
                    visible: root.fileSection !== null
                    Layout.fillWidth: true
                    spacing: HnMetrics.internalSpacing(HnControlSize.Compact)
                    Accessible.role: Accessible.Grouping
                    Accessible.name: root.fileSection ? root.fileSection.label : ""
                    SectionLabel {
                        objectName: "informationSectionLabelFile"
                        rawText: root.fileSection ? root.fileSection.label.toUpperCase() : ""
                    }
                    HnStyle.TextArea {
                        id: pathText
                        objectName: "informationPathText"
                        Layout.fillWidth: true
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 2
                        bottomPadding: 2
                        background: Rectangle {
                            color: "transparent"
                            border.width: pathText.activeFocus ? HnMetrics.focusBorderWidth : 0
                            border.color: pathText.activeFocus ? HoloniightPalette.borderFocus : "transparent"
                            radius: HnMetrics.internalSpacing(HnControlSize.Compact)
                        }
                        readOnly: true
                        selectByMouse: true
                        selectByKeyboard: true
                        activeFocusOnTab: root.fileSection !== null
                        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
                        textFormat: TextEdit.PlainText
                        color: HoloniightPalette.textPrimary
                        Accessible.name: qsTr("File path")
                        text: root.fileSection ? root.fileSection.lines[0] : ""
                        onActiveFocusChanged: Qt.callLater(ensureCursorVisible)
                        onCursorRectangleChanged: Qt.callLater(ensureCursorVisible)

                        // Nested in section layouts, so ScrollView cannot attach this editor directly.
                        function ensureCursorVisible(): void {
                            const flickable = scroll.contentItem as Flickable;
                            if (!activeFocus || !flickable)
                                return;
                            const top = mapToItem(flickable, 0, cursorRectangle.y).y;
                            const bottom = top + cursorRectangle.height;
                            let offset = flickable.contentY;
                            if (top < 0)
                                offset += top;
                            else if (bottom > flickable.height)
                                offset += bottom - flickable.height;
                            flickable.contentY = Math.max(0, Math.min(offset, flickable.contentHeight - flickable.height));
                        }
                    }
                }
            }
        }
    }
}
