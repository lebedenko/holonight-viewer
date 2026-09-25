pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import "footer"
import "information"
import "shortcuts"

HnApplicationWindow {
    id: window
    objectName: "viewerWindow"
    required property ImageDocument document
    width: 1000
    height: 700
    minimumWidth: 420
    minimumHeight: 280
    visible: true
    title: document.fileName ? qsTr("%1 — HoloNight Viewer").arg(document.fileName) : qsTr("HoloNight Viewer")

    component ViewerButton: Controls.Button {
        id: control
        property bool floating: false
        property real cornerRadius: HnMetrics.internalSpacing(HnControlSize.Compact)
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, HnMetrics.controlHeight(HnControlSize.Normal))
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : control.floating ? Qt.alpha(HoloniightPalette.surface, 0.85) : "transparent"
            border.width: control.visualFocus ? HnMetrics.focusBorderWidth : control.floating ? HnMetrics.borderWidth : 0
            border.color: control.visualFocus ? HoloniightPalette.borderFocus : control.floating ? HoloniightPalette.borderPassive : "transparent"
            radius: control.cornerRadius
        }
    }
    component ViewerHeaderButton: ViewerButton {
        display: Controls.AbstractButton.IconOnly
        icon.width: HnMetrics.iconSize(HnControlSize.Hero)
        icon.height: HnMetrics.iconSize(HnControlSize.Hero)
        implicitWidth: HnMetrics.controlHeight(HnControlSize.Large)
        implicitHeight: HnMetrics.controlHeight(HnControlSize.Large)
    }
    component ViewerMenuItem: Controls.MenuItem {
        id: control
        // Keeps the trailing shortcut clear of the menu's overlay scroll bar.
        rightPadding: 12 + ((control.ListView.view as ViewerMenuList)?.scrollBarReserve ?? 0)
        // Presentation only; the real sequence stays on the bound Action or Shortcut.
        property var shortcutKeys: []
        contentItem: RowLayout {
            spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
            HnLabel {
                id: menuLabel
                objectName: "menuItemLabel"
                Layout.fillWidth: true
                rawText: control.text
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: control.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textDisabled
            }
            HnKeySequenceLabel {
                objectName: "menuItemShortcut"
                font: menuLabel.font
                color: control.enabled ? HoloniightPalette.textMuted : HoloniightPalette.textDisabled
                visible: control.shortcutKeys.length > 0
                keyGroups: control.shortcutKeys
                Accessible.ignored: true
            }
        }
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : "transparent"
            border.width: control.enabled && (control.visualFocus || control.highlighted) ? HnMetrics.focusBorderWidth : 0
            border.color: control.enabled && (control.visualFocus || control.highlighted) ? HoloniightPalette.borderFocus : "transparent"
        }
    }
    component ViewerMenuList: ListView {
        property real scrollBarReserve: 0
        // Menu owns navigation and skips separators and disabled actions.
        keyNavigationEnabled: false
    }
    // Preserve Viewer's physical hairline at fractional menu/scroll positions.
    component ViewerMenuSeparator: Controls.MenuSeparator {
        contentItem: HnSeparator {
            color: HoloniightPalette.borderPassive
        }
    }
    property bool rendered: false
    property bool actionsMenuOpen: false
    readonly property bool fullscreen: visibility === Window.FullScreen
    readonly property bool controlsHovered: previousButton.hovered || nextButton.hovered || playPauseButton.hovered || headerHover.hovered || footerHover.hovered
    // Transient overlays: the flags are the display target, the timers only end a reveal.
    property bool arrowsShown: false
    property bool countdownActive: false
    property bool detailsShown: false
    Timer {
        id: arrowTimer
        objectName: "arrowTimer"
        interval: 3000
        running: window.countdownActive && !window.controlsHovered && !window.actionsMenuOpen
        onTriggered: {
            window.arrowsShown = false;
            window.countdownActive = false;
        }
    }
    Timer {
        id: detailsTimer
        objectName: "detailsTimer"
        interval: 4000
        onTriggered: window.detailsShown = false
    }
    function showArrows(): void {
        window.arrowsShown = true;
        window.countdownActive = true;
        if (arrowTimer.running)
            arrowTimer.restart();
    }
    function hideArrows(): void {
        window.arrowsShown = false;
        if (!window.fullscreen)
            window.countdownActive = false;
    }
    onFullscreenChanged: {
        if (fullscreen)
            showArrows();
        else {
            arrowsShown = false;
            countdownActive = false;
        }
    }
    function togglePlayback(): void {
        window.document.animation.toggle();
        // The keyboard hides the arrows, so the strip is what shows the new state.
        window.revealDetails();
        window.clearImageFocus();
    }
    function revealDetails(): void {
        if (window.documentState !== ImageDocument.Ready)
            return;
        window.detailsShown = true;
        detailsTimer.restart();
    }
    function concealDetails(): void {
        window.detailsShown = false;
        detailsTimer.stop();
    }
    function toggleFullscreen(): void {
        WindowState.setFullscreen(window, window.visibility !== Window.FullScreen);
    }

    property bool dialogRequested: false
    property Item dialogFocusItem: null
    property bool informationOpen: false
    property bool helpOpen: false
    readonly property bool modalActive: dialogRequested || informationOpen || helpOpen
    readonly property bool canInspect: document.state === ImageDocument.Ready && !modalActive
    readonly property bool hasPath: document.localPath.length > 0 && !modalActive
    property int inputEpoch: 0
    onModalActiveChanged: {
        ++window.inputEpoch;
        if (!modalActive)
            window.clearImageFocus();
    }

    function clearImageFocus(): void {
        if (!window.modalActive && !window.actionsMenuOpen)
            neutralFocus.forceActiveFocus(Qt.OtherFocusReason);
    }

    WindowKeyRouter {
        id: keyRouter
        objectName: "windowKeyRouter"
        target: window
        imageReady: window.canInspect
        modalActive: window.modalActive
        menuOpen: window.actionsMenuOpen
        playbackAvailable: window.document.animation.canToggle && window.canInspect
        onPlaybackToggleRequested: window.togglePlayback()
        onPanRequested: (horizontal, vertical) => {
            canvas.pan(Qt.point(horizontal, vertical));
            window.clearImageFocus();
        }
    }

    Item {
        id: neutralFocus
        objectName: "neutralFocus"
        parent: window.contentItem
        focus: true
        activeFocusOnTab: false
        width: 1
        height: 1
        Accessible.ignored: true
    }

    readonly property int documentState: document.state
    onDocumentStateChanged: {
        if (documentState !== ImageDocument.Ready)
            window.concealDetails();
        if (documentState === ImageDocument.Ready)
            canvas.Accessible.announce(qsTr("Loaded %1.").arg(window.document.fileName));
        else if (documentState === ImageDocument.Error)
            canvas.Accessible.announce(qsTr("Error opening %1: %2").arg(window.document.fileName).arg(window.document.error));
    }

    // Playback pauses while a dialog covers the image or the window is hidden; focus loss alone does not pause.
    Binding {
        target: window.document.animation
        property: "suspendedByModal"
        value: window.modalActive
    }
    Binding {
        target: window.document.animation
        property: "suspendedByWindow"
        value: !window.visible || window.visibility === Window.Minimized
    }
    // Each frame replaces the picture in place: zoom, pan and fit stay as they are.
    Connections {
        target: window.document
        function onFrameChanged(): void {
            canvas.replaceFrame(window.document.image);
        }
    }
    Connections {
        target: window.document.animation
        function onFailureNoticeChanged(notice: string): void {
            window.revealDetails();
            canvas.Accessible.announce(notice);
        }
    }

    Connections {
        target: window.document.clipboard
        function onChanged(): void {
            if (!window.document.clipboard.busy && window.document.clipboard.feedback.length > 0)
                canvas.Accessible.announce(window.document.clipboard.feedback);
        }
    }

    function transformImage(operation: int): void {
        window.document.transform(operation);
        window.fitImage();
        ++window.inputEpoch;
    }

    Controls.Action {
        id: rotateClockwise
        objectName: "rotateClockwiseAction"
        text: qsTr("Rotate Clockwise")
        shortcut: "R"
        enabled: window.canInspect
        onTriggered: window.transformImage(1)
    }
    Controls.Action {
        id: rotateCounterclockwise
        objectName: "rotateCounterclockwiseAction"
        text: qsTr("Rotate Counterclockwise")
        shortcut: "Shift+R"
        enabled: window.canInspect
        onTriggered: window.transformImage(3)
    }
    Controls.Action {
        id: flipHorizontal
        objectName: "flipHorizontalAction"
        text: qsTr("Flip Horizontally")
        shortcut: "X"
        enabled: window.canInspect
        onTriggered: window.transformImage(4)
    }
    Controls.Action {
        id: flipVertical
        objectName: "flipVerticalAction"
        text: qsTr("Flip Vertically")
        shortcut: "Shift+X"
        enabled: window.canInspect
        onTriggered: window.transformImage(6)
    }
    Controls.Action {
        id: resetTransform
        objectName: "resetTransformAction"
        text: qsTr("Reset Transform")
        enabled: window.canInspect
        onTriggered: {
            window.document.resetTransform();
            window.fitImage();
            ++window.inputEpoch;
        }
    }
    Controls.Action {
        id: copyImage
        objectName: "copyImageAction"
        text: qsTr("Copy Image")
        shortcut: "Ctrl+C"
        enabled: window.canInspect && !window.document.clipboard.busy
        onTriggered: {
            window.document.copyImage();
            window.clearImageFocus();
        }
    }
    Controls.Action {
        id: copyPath
        objectName: "copyPathAction"
        text: qsTr("Copy Path")
        shortcut: "Ctrl+Shift+C"
        enabled: window.hasPath && !window.document.clipboard.busy
        onTriggered: {
            window.document.copyPath();
            window.clearImageFocus();
        }
    }
    Controls.Action {
        id: imageInformation
        objectName: "imageInformationAction"
        text: qsTr("Image Information")
        shortcut: "I"
        // While open, the modal popup blocks window shortcuts and handles I itself.
        enabled: window.hasPath
        onTriggered: window.informationOpen = true
    }
    Controls.Action {
        id: shortcutHelp
        objectName: "shortcutHelpAction"
        text: qsTr("Shortcut Help")
        shortcut: "?"
        // While open, the modal popup blocks window shortcuts and handles ? itself.
        enabled: !window.modalActive
        onTriggered: window.helpOpen = true
    }

    Shortcut {
        sequence: "Ctrl+0"
        enabled: window.canInspect
        onActivated: window.fitImage()
    }
    Shortcut {
        sequence: "1"
        enabled: window.canInspect
        onActivated: window.actualSizeImage()
    }
    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        enabled: window.canInspect
        onActivated: window.zoomImage(1)
    }
    Shortcut {
        sequence: "Ctrl+-"
        enabled: window.canInspect
        onActivated: window.zoomImage(-1)
    }

    Shortcut {
        sequence: "["
        enabled: !window.modalActive && window.document.canPrevious
        onActivated: window.browse(-1)
    }
    Shortcut {
        sequence: "]"
        enabled: !window.modalActive && window.document.canNext
        onActivated: window.browse(1)
    }
    Shortcut {
        sequence: "Ctrl+R"
        enabled: !window.modalActive && window.document.localPath.length > 0
        onActivated: window.refreshFolder()
    }

    function browse(direction: int): void {
        if (direction < 0)
            window.document.previous();
        else
            window.document.next();
        window.clearImageFocus();
    }

    function refreshFolder(): void {
        window.document.refresh();
        window.clearImageFocus();
    }

    function fitImage(): void {
        canvas.fit();
        window.revealDetails();
        window.clearImageFocus();
    }

    function actualSizeImage(): void {
        canvas.actualSize();
        window.revealDetails();
        window.clearImageFocus();
    }

    function zoomImage(steps: real): void {
        canvas.zoomSteps(steps, Qt.point(canvas.width / 2, canvas.height / 2));
        window.revealDetails();
        window.clearImageFocus();
    }

    function leaveFullscreen(): void {
        if (window.visibility === Window.FullScreen)
            WindowState.setFullscreen(window, false);
    }

    Shortcut {
        sequences: [StandardKey.Open]
        enabled: !window.modalActive
        onActivated: window.dialogRequested = true
    }
    Shortcut {
        sequence: "F"
        enabled: !window.modalActive
        onActivated: window.toggleFullscreen()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !window.modalActive
        onActivated: window.leaveFullscreen()
    }
    Shortcut {
        sequence: "Q"
        enabled: !window.modalActive
        onActivated: window.close()
    }

    // Every Open trigger sets dialogRequested; the portal picker runs first and the
    // FileDialog below appears only when the portal cannot serve the request.
    onDialogRequestedChanged: {
        if (window.dialogRequested) {
            window.dialogFocusItem = window.activeFocusItem;
            portalFileChooser.requestOpen();
        } else {
            window.dialogFocusItem = null;
            portalFileChooser.cancel();
        }
    }

    PortalFileChooser {
        id: portalFileChooser
        objectName: "portalFileChooser"
        window: window
        nameFilters: window.document.nameFilters
        currentLocalPath: window.document.localPath
        onFinished: urls => {
            window.document.open(urls);
            window.dialogRequested = false;
        }
        onCancelled: {
            const previousFocus = window.dialogFocusItem;
            window.dialogRequested = false;
            if (!window.modalActive && previousFocus && previousFocus.visible && previousFocus.enabled)
                previousFocus.forceActiveFocus(Qt.OtherFocusReason);
        }
    }

    Loader {
        active: portalFileChooser.fallbackShown
        sourceComponent: Item {
            FileDialog {
                id: fileDialog
                objectName: "openDialog"
                title: qsTr("Open image")
                fileMode: FileDialog.OpenFile
                nameFilters: window.document.nameFilters
                onAccepted: portalFileChooser.fallbackAccepted(fileDialog.selectedFile)
                onRejected: portalFileChooser.fallbackRejected()
                Component.onCompleted: fileDialog.open()
            }
        }
    }

    Loader {
        id: informationLoader
        active: window.informationOpen
        sourceComponent: ImageInformationPopup {
            document: window.document
            parent: Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: window.informationOpen = false
        }
    }

    Loader {
        id: shortcutHelpLoader
        active: window.helpOpen
        sourceComponent: ShortcutHelpPopup {
            parent: Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: window.helpOpen = false
        }
    }

    Item {
        anchors.fill: parent

        Rectangle {
            objectName: "fullscreenHeaderBackground"
            anchors.fill: viewerHeader
            color: HoloniightPalette.surface
            visible: window.fullscreen && viewerHeader.visible
            z: 1
            Accessible.ignored: true
        }
        HnHeaderBar {
            id: viewerHeader
            objectName: "viewerHeader"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: !window.fullscreen || window.arrowsShown || window.actionsMenuOpen
            z: 2
            HoverHandler {
                id: headerHover
            }
            content: Item {
                HnLabel {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - headerActions.width * 2)
                    horizontalAlignment: Text.AlignHCenter
                    textFormat: Text.PlainText
                    rawText: window.document.fileName || qsTr("HoloNight Viewer")
                    elide: Text.ElideMiddle
                }
                Row {
                    id: headerActions
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    ViewerHeaderButton {
                        id: informationButton
                        objectName: "informationButton"
                        KeyNavigation.tab: fullscreenButton
                        KeyNavigation.backtab: actionsButton
                        icon.source: "icons/information.svg"
                        Accessible.name: qsTr("Image Information")
                        enabled: window.hasPath
                        onClicked: window.informationOpen = true
                    }
                    ViewerHeaderButton {
                        id: fullscreenButton
                        objectName: "fullscreenButton"
                        KeyNavigation.tab: actionsButton
                        KeyNavigation.backtab: informationButton
                        icon.source: "icons/fullscreen.svg"
                        Accessible.name: qsTr("Fullscreen")
                        enabled: !window.modalActive
                        onClicked: window.toggleFullscreen()
                    }
                    ViewerHeaderButton {
                        id: actionsButton
                        objectName: "actionsButton"
                        KeyNavigation.tab: informationButton
                        KeyNavigation.backtab: fullscreenButton
                        icon.source: "icons/menu.svg"
                        Accessible.name: qsTr("Menu")
                        enabled: !window.modalActive
                        onClicked: actionsMenu.open()
                        Controls.Menu {
                            id: actionsMenu
                            objectName: "actionsMenu"
                            popupType: Controls.Popup.Item
                            margins: 12
                            x: actionsButton.width - width
                            width: Math.min(380, window.width - 24)
                            height: Math.min(implicitHeight, window.height - 24)
                            onOpened: window.actionsMenuOpen = true
                            onClosed: {
                                window.actionsMenuOpen = false;
                                window.clearImageFocus();
                            }
                            y: actionsButton.height + HnMetrics.internalSpacing(HnControlSize.Normal)
                            contentItem: ViewerMenuList {
                                objectName: "actionsMenuList"
                                implicitHeight: contentHeight
                                model: actionsMenu.contentModel
                                currentIndex: actionsMenu.currentIndex
                                interactive: contentHeight > height
                                clip: true
                                scrollBarReserve: interactive ? menuScrollBar.width : 0
                                Controls.ScrollBar.vertical: Controls.ScrollBar {
                                    id: menuScrollBar
                                    active: true
                                }
                            }

                            ViewerMenuItem {
                                objectName: "openButton"
                                text: qsTr("Open…")
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_O]]
                                enabled: !window.modalActive
                                onTriggered: window.dialogRequested = true
                            }
                            ViewerMenuItem {
                                text: qsTr("Refresh")
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_R]]
                                enabled: window.hasPath
                                onTriggered: window.refreshFolder()
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                text: qsTr("Previous")
                                shortcutKeys: [[Qt.Key_BracketLeft]]
                                enabled: !window.modalActive && window.document.canPrevious
                                onTriggered: window.browse(-1)
                            }
                            ViewerMenuItem {
                                text: qsTr("Next")
                                shortcutKeys: [[Qt.Key_BracketRight]]
                                enabled: !window.modalActive && window.document.canNext
                                onTriggered: window.browse(1)
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                objectName: "fitButton"
                                text: qsTr("Fit")
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_0]]
                                enabled: window.canInspect
                                onTriggered: window.fitImage()
                            }
                            ViewerMenuItem {
                                objectName: "actualSizeButton"
                                text: qsTr("Actual Size")
                                shortcutKeys: [[Qt.Key_1]]
                                enabled: window.canInspect
                                onTriggered: window.actualSizeImage()
                            }
                            ViewerMenuItem {
                                objectName: "zoomInButton"
                                text: qsTr("Zoom In")
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_Plus]]
                                enabled: window.canInspect
                                onTriggered: window.zoomImage(1)
                            }
                            ViewerMenuItem {
                                objectName: "zoomOutButton"
                                text: qsTr("Zoom Out")
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_Minus]]
                                enabled: window.canInspect
                                onTriggered: window.zoomImage(-1)
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                action: rotateClockwise
                                shortcutKeys: [[Qt.Key_R]]
                            }
                            ViewerMenuItem {
                                action: rotateCounterclockwise
                                shortcutKeys: [[Qt.Key_Shift, Qt.Key_R]]
                            }
                            ViewerMenuItem {
                                action: flipHorizontal
                                shortcutKeys: [[Qt.Key_X]]
                            }
                            ViewerMenuItem {
                                action: flipVertical
                                shortcutKeys: [[Qt.Key_Shift, Qt.Key_X]]
                            }
                            ViewerMenuItem {
                                action: resetTransform
                                objectName: "resetTransformMenuItem"
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                action: copyImage
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_C]]
                            }
                            ViewerMenuItem {
                                action: copyPath
                                shortcutKeys: [[Qt.Key_Control, Qt.Key_Shift, Qt.Key_C]]
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                action: imageInformation
                                objectName: "informationMenuItem"
                                shortcutKeys: [[Qt.Key_I]]
                            }
                            ViewerMenuItem {
                                action: shortcutHelp
                                shortcutKeys: [[Qt.Key_Question]]
                            }
                            ViewerMenuSeparator {}
                            ViewerMenuItem {
                                text: qsTr("Fullscreen")
                                shortcutKeys: [[Qt.Key_F]]
                                onTriggered: window.toggleFullscreen()
                            }
                            ViewerMenuItem {
                                text: qsTr("Quit")
                                shortcutKeys: [[Qt.Key_Q]]
                                onTriggered: window.close()
                            }
                        }
                    }
                }
            }
        }
        HnLabel {
            id: folderError
            objectName: "folderError"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: viewerHeader.bottom
            anchors.topMargin: 6
            visible: window.document.scanning || window.document.folderError.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            rawText: window.document.scanning ? qsTr("Scanning…") : window.document.folderError
            Accessible.name: rawText
        }

        Item {
            id: canvasArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: window.fullscreen ? parent.top : folderError.visible ? folderError.bottom : viewerHeader.bottom
            anchors.bottom: window.fullscreen ? parent.bottom : viewerFooter.top

            ImageCanvas {
                id: canvas
                objectName: "imageCanvas"
                anchors.fill: parent
                image: window.document.image
                svgRenderer: window.document.svgRenderer
                svgSize: window.document.svgSize
                orientation: window.document.orientation
                onOrientationChanged: ++window.inputEpoch
                displayPixelRatio: window.devicePixelRatio
                activeFocusOnTab: false
                onImageChanged: {
                    ++window.inputEpoch;
                    window.rendered = false;
                    window.concealDetails();
                }
                onFirstRendered: {
                    window.rendered = true;
                    window.revealDetails();
                }
                onMouseMoved: {
                    window.showArrows();
                    if (window.rendered)
                        window.revealDetails();
                }
                onKeyboardInput: window.hideArrows()
                onViewportChanged: ++window.inputEpoch
                Accessible.role: Accessible.Graphic
                Accessible.name: window.document.state === ImageDocument.Empty ? qsTr("No image open") : window.document.fileName || qsTr("Image canvas")

                DragHandler {
                    id: drag
                    target: null
                    enabled: window.canInspect
                    acceptedButtons: Qt.LeftButton
                    property int gestureEpoch: -1
                    readonly property bool validGesture: active && gestureEpoch === window.inputEpoch
                    cursorShape: validGesture && canvas.canPan ? Qt.ClosedHandCursor : canvas.canPan ? Qt.OpenHandCursor : Qt.ArrowCursor
                    onActiveChanged: {
                        gestureEpoch = active ? window.inputEpoch : -1;
                        if (active)
                            window.clearImageFocus();
                    }
                    onTranslationChanged: delta => {
                        if (drag.validGesture)
                            canvas.pan(Qt.point(delta.x, delta.y));
                    }
                }
                WheelHandler {
                    target: null
                    enabled: window.canInspect
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: event => {
                        const steps = event.pixelDelta.y !== 0 ? event.pixelDelta.y / 40 : event.angleDelta.y / 120;
                        if (steps !== 0) {
                            canvas.zoomSteps(steps, Qt.point(event.x, event.y));
                            window.revealDetails();
                            window.clearImageFocus();
                            event.accepted = true;
                        } else {
                            event.accepted = false;
                        }
                    }
                }
            }
            ViewerButton {
                id: previousButton
                objectName: "previousButton"
                // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
                readonly property bool shown: window.arrowsShown && window.document.count > 1
                floating: true
                implicitHeight: 48
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("‹")
                Accessible.name: qsTr("Previous image")
                // Stays visible only while fading, so a faded arrow is neither clickable nor announced.
                opacity: shown ? 1 : 0
                visible: shown || opacity > 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
                enabled: !window.modalActive && window.document.canPrevious
                onClicked: window.browse(-1)
            }
            ViewerButton {
                id: playPauseButton
                objectName: "playPauseButton"
                readonly property bool shown: window.arrowsShown
                readonly property bool playing: window.document.animation.playing
                floating: true
                display: Controls.AbstractButton.IconOnly
                implicitWidth: 48
                implicitHeight: 48
                cornerRadius: 24
                anchors.centerIn: parent
                icon.source: playing ? "icons/pause.svg" : "icons/play.svg"
                // Both sizes come from one value: icon.height reading icon.width is a binding loop on the group.
                readonly property real glyphSize: Math.min(HnMetrics.iconSize(HnControlSize.Hero), 32)
                icon.width: glyphSize
                icon.height: glyphSize
                // The name is the action the button performs.
                Accessible.name: playing ? qsTr("Pause") : qsTr("Play")
                enabled: window.document.animation.canToggle && !window.modalActive
                opacity: shown ? 1 : 0
                visible: window.document.animation.animated && (shown || opacity > 0)
                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
                onClicked: window.togglePlayback()
            }
            ViewerButton {
                id: nextButton
                objectName: "nextButton"
                // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
                readonly property bool shown: window.arrowsShown && window.document.count > 1
                floating: true
                implicitHeight: 48
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("›")
                Accessible.name: qsTr("Next image")
                // Stays visible only while fading, so a faded arrow is neither clickable nor announced.
                opacity: shown ? 1 : 0
                visible: shown || opacity > 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
                enabled: !window.modalActive && window.document.canNext
                onClicked: window.browse(1)
            }
            Rectangle {
                objectName: "detailsStrip"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: window.fullscreen && viewerFooter.visible ? viewerFooter.height + 12 : 12
                width: Math.min(parent.width - 24, stripFilename.implicitWidth + metadata.naturalWidth + 36)
                height: detailsFlow.height + 16
                radius: HnMetrics.internalSpacing(HnControlSize.Compact)
                color: Qt.alpha(HoloniightPalette.surface, 0.85)
                border.color: HoloniightPalette.borderPassive
                readonly property bool shown: window.detailsShown
                opacity: shown ? 1 : 0
                visible: shown || opacity > 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
                Flow {
                    id: detailsFlow
                    x: 12
                    y: 8
                    width: parent.width - 24
                    spacing: 12
                    HnLabel {
                        id: stripFilename
                        width: Math.min(implicitWidth, Math.max(80, detailsFlow.width - metadata.naturalWidth - 12))
                        rawText: window.document.fileName
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                    }
                    Flow {
                        id: metadata
                        readonly property real naturalWidth: {
                            let total = Math.max(0, metadataSections.count - 1) * spacing;
                            for (let i = 0; i < metadataSections.count; ++i) {
                                const section = metadataSections.itemAt(i);
                                if (section)
                                    total += section.implicitWidth;
                            }
                            return total;
                        }
                        width: Math.min(naturalWidth, detailsFlow.width)
                        spacing: 12
                        Repeater {
                            id: metadataSections
                            model: [qsTr("%1 × %2").arg(window.document.transformedDimensions.width).arg(window.document.transformedDimensions.height), window.document.formattedFileSize, qsTr("%1%").arg(Number(canvas.magnification * 100).toLocaleString(Qt.locale(), 'f', canvas.magnification < 0.01 ? 4 : 1)), qsTr("%1 / %2").arg(window.document.position).arg(window.document.count)].concat(window.document.animation.animated ? [qsTr("Animated"), window.document.animation.playing ? qsTr("Playing") : qsTr("Paused")] : []).concat(window.document.animation.animated && window.document.animation.frameCount > 0 ? [qsTr("%1 frames").arg(window.document.animation.frameCount)] : [])
                            RowLayout {
                                id: section
                                required property string modelData
                                width: Math.min(implicitWidth, metadata.width)
                                spacing: 12
                                HnSeparator {
                                    orientation: Qt.Vertical
                                    Layout.fillHeight: true
                                }
                                HnLabel {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    rawText: section.modelData
                                    textFormat: Text.PlainText
                                    Accessible.role: Accessible.StaticText
                                    Accessible.name: rawText
                                }
                            }
                        }
                    }
                    HnLabel {
                        objectName: "playbackNotice"
                        width: detailsFlow.width
                        visible: window.document.animation.failureNotice.length > 0
                        color: HoloniightPalette.warning
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        rawText: window.document.animation.failureNotice
                        Accessible.role: Accessible.StaticText
                        Accessible.name: rawText
                    }
                }
            }
            // Glyph and hint centred as one unit; the text keeps its size and the glyph gives way first.
            Column {
                id: emptyStateGroup
                objectName: "emptyStateGroup"
                anchors.centerIn: parent
                visible: window.document.state === ImageDocument.Empty
                spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
                // Below this the glyph reads as noise, so it hides and only the hint remains.
                readonly property real minimumGlyphSide: 48
                readonly property real hintHeight: emptyHintPrimary.implicitHeight + spacing + emptyHintSecondary.height
                readonly property real availableForGlyph: canvasArea.height - hintHeight - spacing - 2 * emptyDecoration.padding

                HnIcon {
                    id: emptyDecoration
                    objectName: "emptyState"
                    readonly property real shorterDimension: Math.min(canvasArea.width, canvasArea.height)
                    readonly property real padding: Math.max(32, Math.min(64, shorterDimension * 0.08))
                    readonly property int side: Math.max(0, Math.floor(Math.min(shorterDimension - 2 * padding, emptyStateGroup.availableForGlyph)))
                    readonly property int rasterLimit: Math.max(1, Math.floor(1024 / window.devicePixelRatio))
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: side >= emptyStateGroup.minimumGlyphSide
                    source: side > 0 ? Qt.resolvedUrl("icons/empty-viewer.svg") : ""
                    rendering: HnIcon.Semantic
                    // Qt scales sourceSize by DPR; hnicons accepts at most 1024 physical pixels.
                    size: Math.min(side, rasterLimit)
                    width: side
                    height: side
                    normalColor: HoloniightPalette.surface
                    Accessible.ignored: true
                }
                HnLabel {
                    id: emptyHintPrimary
                    objectName: "emptyStateHintPrimary"
                    anchors.horizontalCenter: parent.horizontalCenter
                    role: HnTypographyRole.Body
                    color: HoloniightPalette.textMuted
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    rawText: qsTr("No image open")
                    Accessible.role: Accessible.StaticText
                    Accessible.name: rawText
                }
                HnLabel {
                    id: emptyHintSecondary
                    objectName: "emptyStateHintSecondary"
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth, canvasArea.width - 32)
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    rawText: qsTr("Ctrl+O to open · or drop an image here")
                    Accessible.role: Accessible.StaticText
                    Accessible.name: rawText
                }
            }
            HnLabel {
                objectName: "documentFeedback"
                textFormat: Text.PlainText
                anchors.centerIn: parent
                width: canvasArea.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: window.document.state === ImageDocument.Loading || window.document.state === ImageDocument.Error
                rawText: window.document.state === ImageDocument.Loading ? qsTr("Loading %1…").arg(window.document.fileName) : window.document.error
                Accessible.role: Accessible.StaticText
                Accessible.name: rawText
            }
        }

        Rectangle {
            objectName: "fullscreenFooterBackground"
            anchors.fill: viewerFooter
            color: HoloniightPalette.surface
            visible: window.fullscreen && viewerFooter.visible
            z: 1
            Accessible.ignored: true
        }
        ColumnLayout {
            id: viewerFooter
            objectName: "viewerFooter"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: implicitHeight
            spacing: 0
            visible: !window.fullscreen || window.arrowsShown
            z: 2
            HoverHandler {
                id: footerHover
            }
            HnLabel {
                Layout.fillWidth: true
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                visible: window.document.clipboard.feedback.length > 0
                rawText: window.document.clipboard.feedback
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                Accessible.name: rawText
            }
            HnSeparator {
                objectName: "footerSeparator"
                Layout.fillWidth: true
                fadeMode: HnSeparator.Solid
            }
            FooterKeyHints {
                animated: window.document.animation.canToggle
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 8
                Layout.bottomMargin: 8
            }
        }
    }

    DropArea {
        objectName: "imageDropArea"
        anchors.fill: parent
        enabled: !window.modalActive
        onDropped: drop => {
            window.document.open(drop.urls);
            drop.acceptProposedAction();
        }
    }
}
