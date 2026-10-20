pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

HnHeaderBar {
    id: header
    property bool titleVisible: true
    property bool closeVisible: true
    required property Controls.Action quitAction
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
        background: HnButtonSurface {
            pressed: control.down
            hovered: control.hovered
            focusVisible: control.visualFocus
            cornerRadius: control.cornerRadius
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
    dividerVisible: false
    HoverHandler {
        id: headerHover
    }
    component HeaderContent: Item {
        id: headerContent
        property bool closeShown: true
        function updateCloseVisibility(): void {
            if (!header.closeVisible && closeButton.activeFocus && !header.modalActive)
                actionsButton.forceActiveFocus(Qt.TabFocusReason);
            closeShown = header.closeVisible;
        }
        Component.onCompleted: updateCloseVisibility()
        readonly property bool controlFocused: informationButton.activeFocus || fullscreenButton.activeFocus || actionsButton.activeFocus || closeButton.activeFocus
        readonly property Item menuAnchor: actionsButton
        readonly property list<Item> focusTargets: closeShown ? [informationButton, fullscreenButton, actionsButton, closeButton] : [informationButton, fullscreenButton, actionsButton]
        Connections {
            target: header
            function onCloseVisibleChanged(): void {
                headerContent.updateCloseVisibility();
            }
        }
        HnLabel {
            objectName: "headerTitle"
            visible: header.titleVisible
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.width - headerActions.width - HnMetrics.internalSpacing(HnControlSize.Normal))
            horizontalAlignment: Text.AlignLeft
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
                KeyNavigation.backtab: headerContent.closeShown ? closeButton : actionsButton
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
                KeyNavigation.tab: headerContent.closeShown ? closeButton : informationButton
                KeyNavigation.backtab: fullscreenButton
                icon.source: "../icons/menu.svg"
                Accessible.name: qsTranslate("Main", "Menu")
                enabled: !header.modalActive
                onClicked: header.menuRequested()
            }
            HnHeaderCloseButton {
                id: closeButton
                objectName: "closeButton"
                visible: headerContent.closeShown
                KeyNavigation.tab: informationButton
                KeyNavigation.backtab: actionsButton
                Accessible.name: qsTranslate("Main", "Close")
                action: header.quitAction
            }
        }
    }
    content: HeaderContent {}
}
