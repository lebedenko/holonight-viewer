pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

HnHeaderBar {
    id: header
    required property string title
    required property Controls.Action informationAction
    required property Controls.Action fullscreenAction
    required property bool modalActive
    readonly property bool hovered: headerHover.hovered
    readonly property bool controlFocused: (contentItem as HeaderContent)?.controlFocused ?? false
    readonly property Item menuAnchor: (contentItem as HeaderContent)?.menuAnchor ?? null
    readonly property list<Item> focusTargets: (contentItem as HeaderContent)?.focusTargets ?? []
    signal menuRequested
    component HeaderButtonBase: Controls.Button {
        id: control
        property real cornerRadius: HnMetrics.internalSpacing(HnControlSize.Compact)
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, HnMetrics.controlHeight(HnControlSize.Normal))
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : "transparent"
            border.width: control.visualFocus ? HnMetrics.focusBorderWidth : 0
            border.color: control.visualFocus ? HoloniightPalette.borderFocus : "transparent"
            radius: control.cornerRadius
        }
    }
    component ViewerHeaderButton: HeaderButtonBase {
        id: headerButton
        // A button natively activates on Space only; Enter does the same when it has focus.
        Keys.onPressed: event => {
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !event.isAutoRepeat) {
                headerButton.click();
                event.accepted = true;
            }
        }
        display: Controls.AbstractButton.IconOnly
        padding: 2
        horizontalPadding: 2
        icon.width: 24
        icon.height: 24
        implicitWidth: HnMetrics.controlHeight(HnControlSize.Compact)
        implicitHeight: HnMetrics.controlHeight(HnControlSize.Compact)
    }
    objectName: "viewerHeader"
    sizeRole: HnControlSize.Xs
    HoverHandler {
        id: headerHover
    }
    component HeaderContent: Item {
        readonly property bool controlFocused: informationButton.activeFocus || fullscreenButton.activeFocus || actionsButton.activeFocus
        readonly property Item menuAnchor: actionsButton
        readonly property list<Item> focusTargets: [informationButton, fullscreenButton, actionsButton]
        HnLabel {
            objectName: "headerTitle"
            anchors.centerIn: parent
            width: Math.max(0, parent.width - headerActions.width * 2)
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            rawText: header.title
            elide: Text.ElideMiddle
        }
        Row {
            id: headerActions
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
            ViewerHeaderButton {
                id: informationButton
                objectName: "informationButton"
                KeyNavigation.tab: fullscreenButton
                KeyNavigation.backtab: actionsButton
                icon.source: "../icons/information.svg"
                Accessible.name: qsTranslate("Main", "Image Information")
                action: header.informationAction
            }
            ViewerHeaderButton {
                id: fullscreenButton
                objectName: "fullscreenButton"
                KeyNavigation.tab: actionsButton
                KeyNavigation.backtab: informationButton
                icon.source: "../icons/fullscreen.svg"
                Accessible.name: qsTranslate("Main", "Fullscreen")
                action: header.fullscreenAction
            }
            ViewerHeaderButton {
                id: actionsButton
                objectName: "actionsButton"
                KeyNavigation.tab: informationButton
                KeyNavigation.backtab: fullscreenButton
                icon.source: "../icons/menu.svg"
                Accessible.name: qsTranslate("Main", "Menu")
                enabled: !header.modalActive
                onClicked: header.menuRequested()
            }
        }
    }
    content: HeaderContent {}
}
