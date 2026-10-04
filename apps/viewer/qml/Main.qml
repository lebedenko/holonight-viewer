pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import "header"
import "overlays"
import "footer"
import "grid"
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
    property bool rendered: false
    property bool actionsMenuOpen: false
    readonly property bool fullscreen: visibility === Window.FullScreen
    readonly property bool controlsHovered: previousButton.hovered || nextButton.hovered || playPauseButton.hovered || viewerHeader.hovered || footerHover.hovered
    property alias arrowsShown: overlayController.arrowsShown
    property alias countdownActive: overlayController.countdownActive
    property alias detailsShown: overlayController.detailsShown
    ViewerOverlayController {
        id: overlayController
        fullscreen: window.fullscreen
        controlsHovered: window.controlsHovered
        menuOpen: window.actionsMenuOpen
        imageReady: window.documentState === ImageDocument.Ready
    }
    function showArrows(): void {
        overlayController.showArrows();
    }
    function hideArrows(): void {
        overlayController.hideArrows();
    }
    function togglePlayback(): void {
        controller.togglePlayback();
        // The keyboard hides the arrows, so the strip is what shows the new state.
        window.revealDetails();
        window.clearImageFocus();
    }
    function revealDetails(): void {
        overlayController.revealDetails();
    }
    function concealDetails(): void {
        overlayController.concealDetails();
    }
    function toggleFullscreen(): void {
        WindowState.setFullscreen(window, window.visibility !== Window.FullScreen);
    }

    property bool dialogRequested: false
    property Item dialogFocusItem: null
    property bool informationOpen: false
    property bool helpOpen: false
    readonly property bool modalActive: dialogRequested || informationOpen || helpOpen
    // Grid view replaces the canvas; everything that acts on the picture is gated by canInspect.
    ViewerController {
        id: controller
        objectName: "viewerController"
        document: window.document
        modalActive: window.modalActive
        windowSuspended: !window.visible || window.visibility === Window.Minimized
    }
    readonly property bool gridMode: controller.gridMode
    readonly property bool canEnterGrid: controller.canEnterGrid
    readonly property bool canToggleGrid: controller.canToggleGrid
    readonly property bool canInspect: controller.canInspect
    readonly property bool hasPath: controller.hasPath
    readonly property bool canShowInformation: controller.canShowInformation
    // While a scan runs the count is the transient one-item listing, so only the folder is named.
    readonly property string headerTitle: {
        if (!gridMode)
            return document.fileName || qsTr("HoloNight Viewer");
        if (document.scanning)
            return document.folderName;
        // Both source forms keep English correct without a catalog; numerus enables all translated plural forms.
        const count = document.folder.count;
        return (count === 1 ? qsTr("%1 — %n image", "", count) : qsTr("%1 — %n images", "", count)).arg(document.folderName);
    }
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
        headerFocusTargets: viewerHeader.focusTargets
        playbackButton: playPauseButton
        imageReady: window.canInspect
        modalActive: window.modalActive
        menuOpen: window.actionsMenuOpen
        playbackAvailable: window.document.animation.canToggle && window.canInspect
        gridActive: window.gridMode
        onGridMoveRequested: move => {
            controller.moveSelection(move);
            window.clearImageFocus();
        }
        onGridActivateRequested: {
            controller.activateSelection();
            window.clearImageFocus();
        }
        onBrowseRequested: direction => {
            const action = direction < 0 ? viewerActions.previousImage : viewerActions.nextImage;
            if (action.enabled)
                action.trigger();
        }
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

    ViewerActions {
        id: viewerActions
        document: window.document
        modalActive: window.modalActive
        hasPath: window.hasPath
        canToggleGrid: window.canToggleGrid
        canInspect: window.canInspect
        canShowInformation: window.canShowInformation
        gridMode: window.gridMode
        actionsMenuOpen: window.actionsMenuOpen
        canPrevious: window.canBrowse(-1)
        canNext: window.canBrowse(1)
        onPasteRequested: controller.paste()
        onOpenRequested: window.dialogRequested = true
        onInformationRequested: window.informationOpen = true
        onHelpRequested: window.helpOpen = true
        onRefreshRequested: window.refreshFolder()
        onGridRequested: window.toggleGrid()
        onBrowseRequested: direction => window.browse(direction)
        onFitRequested: window.fitImage()
        onActualSizeRequested: window.actualSizeImage()
        onZoomRequested: steps => window.zoomImage(steps)
        onFullscreenRequested: window.toggleFullscreen()
        onQuitRequested: window.close()
        onTransformRequested: operation => window.transformImage(operation)
        onResetRequested: {
            window.document.resetTransform();
            window.fitImage();
            ++window.inputEpoch;
        }
        onFocusRequested: window.clearImageFocus()
        onLeaveGridRequested: window.leaveGrid()
        onLeaveFullscreenRequested: window.leaveFullscreen()
    }

    function canBrowse(direction: int): bool {
        return direction < 0 ? controller.canPrevious : controller.canNext;
    }
    function browse(direction: int): void {
        controller.browse(direction);
        window.clearImageFocus();
    }
    function enterGrid(): void {
        controller.enterGrid();
        window.clearImageFocus();
    }
    function leaveGrid(): void {
        controller.leaveGrid();
        window.clearImageFocus();
    }
    function toggleGrid(): void {
        controller.toggleGrid();
        window.clearImageFocus();
    }
    function refreshFolder(): void {
        controller.refresh();
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
            // Acceptance enters single view; cancellation preserves the grid and its selection.
            controller.open(urls);
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
        ViewerHeader {
            id: viewerHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            title: window.headerTitle
            informationAction: viewerActions.imageInformation
            fullscreenAction: viewerActions.fullscreenCommand
            modalActive: window.modalActive
            visible: !window.fullscreen || window.gridMode || window.arrowsShown || window.actionsMenuOpen || viewerHeader.controlFocused
            z: 2
            onMenuRequested: actionsMenu.open()
        }
        ViewerActionsMenu {
            id: actionsMenu
            parent: viewerHeader.menuAnchor
            windowWidth: window.width
            windowHeight: window.height
            gridMode: window.gridMode
            pastePathAction: viewerActions.pastePath
            openImageAction: viewerActions.openImage
            refreshAction: viewerActions.refresh
            gridToggleAction: viewerActions.gridToggle
            previousImageAction: viewerActions.previousImage
            nextImageAction: viewerActions.nextImage
            fitAction: viewerActions.fit
            actualSizeAction: viewerActions.actualSize
            zoomInAction: viewerActions.zoomIn
            zoomOutAction: viewerActions.zoomOut
            fullscreenCommandAction: viewerActions.fullscreenCommand
            quitAction: viewerActions.quit
            rotateClockwiseAction: viewerActions.rotateClockwise
            rotateCounterclockwiseAction: viewerActions.rotateCounterclockwise
            flipHorizontalAction: viewerActions.flipHorizontal
            flipVerticalAction: viewerActions.flipVertical
            resetTransformAction: viewerActions.resetTransform
            copyImageAction: viewerActions.copyImage
            copyPathAction: viewerActions.copyPath
            imageInformationAction: viewerActions.imageInformation
            shortcutHelpAction: viewerActions.shortcutHelp
            onOpened: window.actionsMenuOpen = true
            onClosed: {
                window.actionsMenuOpen = false;
                window.clearImageFocus();
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
            // Fullscreen grid keeps the header and footer, since the canvas hover that reveals them is gone.
            anchors.top: window.fullscreen && !window.gridMode ? parent.top : folderError.visible ? folderError.bottom : viewerHeader.bottom
            anchors.bottom: window.fullscreen && !window.gridMode ? parent.bottom : viewerFooter.top

            ViewerImageView {
                id: canvas
                objectName: "imageCanvas"
                anchors.fill: parent
                visible: !window.gridMode
                image: window.document.image
                svgRenderer: window.document.svgRenderer
                svgSize: window.document.svgSize
                orientation: window.document.orientation
                onOrientationChanged: ++window.inputEpoch
                displayPixelRatio: window.devicePixelRatio
                activeFocusOnTab: false
                onContentReset: {
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

                inputEpoch: window.inputEpoch
                canInspect: window.canInspect
                onFocusRequested: window.clearImageFocus()
                onDetailsRequested: window.revealDetails()
            }
            ThumbnailGrid {
                id: grid
                anchors.fill: parent
                visible: window.gridMode
                model: window.document.folder
                selectionController: controller.selection
                scanning: window.document.scanning
                generation: window.document.thumbnailGeneration
                devicePixelRatio: window.devicePixelRatio
                onSelectionInteraction: window.clearImageFocus()
                onActivated: {
                    controller.activateSelection();
                    window.clearImageFocus();
                }
            }
            ViewerButton {
                id: previousButton
                objectName: "previousButton"
                // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
                readonly property bool shown: window.arrowsShown && window.document.count > 1 && !window.gridMode
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
                action: viewerActions.previousImage
            }
            ViewerButton {
                id: playPauseButton
                objectName: "playPauseButton"
                readonly property bool shown: window.arrowsShown && !window.gridMode
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
                readonly property bool shown: window.arrowsShown && window.document.count > 1 && !window.gridMode
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
                action: viewerActions.nextImage
            }
            ImageDetailsStrip {
                document: window.document
                magnification: canvas.magnification
                shown: window.detailsShown && !window.gridMode
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: window.fullscreen && viewerFooter.visible ? viewerFooter.height + 12 : 12
            }
            ViewerEmptyState {
                anchors.centerIn: parent
                visible: window.document.state === ImageDocument.Empty || (window.gridMode && grid.showsEmpty)
                availableWidth: canvasArea.width
                availableHeight: canvasArea.height
                devicePixelRatio: window.devicePixelRatio
            }
            HnLabel {
                objectName: "documentFeedback"
                textFormat: Text.PlainText
                anchors.centerIn: parent
                width: canvasArea.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: !window.gridMode && (window.document.state === ImageDocument.Loading || window.document.state === ImageDocument.Error)
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
            visible: !window.fullscreen || window.gridMode || window.arrowsShown
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
                gridMode: window.gridMode
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
            controller.open(drop.urls);
            drop.acceptProposedAction();
        }
    }
}
