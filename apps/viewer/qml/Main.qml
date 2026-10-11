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
    id: root
    objectName: "viewerWindow"
    required property ImageDocument document
    width: 1000
    height: 700
    minimumWidth: 420
    minimumHeight: 280
    visible: true
    title: gridMode || document.fileName ? qsTr("%1 — HoloNight Viewer").arg(headerTitle) : qsTr("HoloNight Viewer")

    component ViewerButton: Controls.Button {
        id: control
        property bool floating: false
        property real cornerRadius: HnMetrics.internalSpacing(HnControlSize.Compact)
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, HnMetrics.controlHeight(HnControlSize.Normal))
        background: HnButtonSurface {
            pressed: control.down
            hovered: control.hovered
            focusVisible: control.visualFocus
            cornerRadius: control.cornerRadius
            floating: control.floating
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
        fullscreen: root.fullscreen
        controlsHovered: root.controlsHovered
        menuOpen: root.actionsMenuOpen
        imageReady: root.documentState === ImageDocument.Ready
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
        root.revealDetails();
        root.clearImageFocus();
    }
    function revealDetails(): void {
        overlayController.revealDetails();
    }
    function concealDetails(): void {
        overlayController.concealDetails();
    }
    function toggleFullscreen(): void {
        WindowState.setFullscreen(root, root.visibility !== Window.FullScreen);
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
        document: root.document
        modalActive: root.modalActive
        windowSuspended: !root.visible || root.visibility === Window.Minimized
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
        ++root.inputEpoch;
        if (!modalActive)
            root.clearImageFocus();
    }

    function clearImageFocus(): void {
        if (!root.modalActive && !root.actionsMenuOpen)
            neutralFocus.forceActiveFocus(Qt.OtherFocusReason);
    }

    WindowKeyRouter {
        id: keyRouter
        objectName: "windowKeyRouter"
        target: root
        headerFocusTargets: viewerHeader.focusTargets
        playbackButton: playPauseButton
        imageReady: root.canInspect
        modalActive: root.modalActive
        menuOpen: root.actionsMenuOpen
        playbackAvailable: root.document.animation.canToggle && root.canInspect
        gridActive: root.gridMode
        onGridMoveRequested: move => {
            controller.moveSelection(move);
            root.clearImageFocus();
        }
        onGridActivateRequested: {
            controller.activateSelection();
            root.clearImageFocus();
        }
        onBrowseRequested: direction => {
            const action = direction < 0 ? viewerActions.previousImage : viewerActions.nextImage;
            if (action.enabled)
                action.trigger();
        }
        onPlaybackToggleRequested: root.togglePlayback()
        onPanRequested: (horizontal, vertical) => {
            canvas.pan(Qt.point(horizontal, vertical));
            root.clearImageFocus();
        }
    }

    Item {
        id: neutralFocus
        objectName: "neutralFocus"
        parent: root.contentItem
        focus: true
        activeFocusOnTab: false
        width: 1
        height: 1
        Accessible.ignored: true
    }

    readonly property int documentState: document.state
    onDocumentStateChanged: {
        if (documentState !== ImageDocument.Ready)
            root.concealDetails();
        if (documentState === ImageDocument.Ready)
            canvas.Accessible.announce(qsTr("Loaded %1.").arg(root.document.fileName));
        else if (documentState === ImageDocument.Error)
            canvas.Accessible.announce(qsTr("Error opening %1: %2").arg(root.document.fileName).arg(root.document.error));
    }

    Connections {
        target: root.document.animation
        function onFailureNoticeChanged(notice: string): void {
            root.revealDetails();
            canvas.Accessible.announce(notice);
        }
    }

    Connections {
        target: root.document.clipboard
        function onChanged(): void {
            if (!root.document.clipboard.busy && root.document.clipboard.feedback.length > 0)
                canvas.Accessible.announce(root.document.clipboard.feedback);
        }
    }

    function transformImage(operation: int): void {
        root.document.transform(operation);
        root.fitImage();
        ++root.inputEpoch;
    }

    ViewerActions {
        id: viewerActions
        document: root.document
        modalActive: root.modalActive
        hasPath: root.hasPath
        canToggleGrid: root.canToggleGrid
        canInspect: root.canInspect
        canShowInformation: root.canShowInformation
        gridMode: root.gridMode
        actionsMenuOpen: root.actionsMenuOpen
        canPrevious: root.canBrowse(-1)
        canNext: root.canBrowse(1)
        onPasteRequested: controller.paste()
        onOpenRequested: root.dialogRequested = true
        onInformationRequested: root.informationOpen = true
        onHelpRequested: root.helpOpen = true
        onRefreshRequested: root.refreshFolder()
        onGridRequested: root.toggleGrid()
        onBrowseRequested: direction => root.browse(direction)
        onFitRequested: root.fitImage()
        onActualSizeRequested: root.actualSizeImage()
        onZoomRequested: steps => root.zoomImage(steps)
        onFullscreenRequested: root.toggleFullscreen()
        onQuitRequested: root.close()
        onTransformRequested: operation => root.transformImage(operation)
        onResetRequested: {
            root.document.resetTransform();
            root.fitImage();
            ++root.inputEpoch;
        }
        onCopyImageRequested: {
            root.document.copyImage();
            root.clearImageFocus();
        }
        onCopyPathRequested: {
            root.document.copyPath();
            root.clearImageFocus();
        }
        onLeaveGridRequested: root.leaveGrid()
        onLeaveFullscreenRequested: root.leaveFullscreen()
    }

    function canBrowse(direction: int): bool {
        return direction < 0 ? controller.canPrevious : controller.canNext;
    }
    function browse(direction: int): void {
        controller.browse(direction);
        root.clearImageFocus();
    }
    function enterGrid(): void {
        controller.enterGrid();
        root.clearImageFocus();
    }
    function leaveGrid(): void {
        controller.leaveGrid();
        root.clearImageFocus();
    }
    function toggleGrid(): void {
        controller.toggleGrid();
        root.clearImageFocus();
    }
    function refreshFolder(): void {
        controller.refresh();
        root.clearImageFocus();
    }

    function fitImage(): void {
        canvas.fit();
        root.revealDetails();
        root.clearImageFocus();
    }

    function actualSizeImage(): void {
        canvas.actualSize();
        root.revealDetails();
        root.clearImageFocus();
    }

    function zoomImage(steps: real): void {
        canvas.zoomSteps(steps, Qt.point(canvas.width / 2, canvas.height / 2));
        root.revealDetails();
        root.clearImageFocus();
    }

    function leaveFullscreen(): void {
        if (root.visibility === Window.FullScreen)
            WindowState.setFullscreen(root, false);
    }

    // Every Open trigger sets dialogRequested; the portal picker runs first and the
    // FileDialog below appears only when the portal cannot serve the request.
    onDialogRequestedChanged: {
        if (root.dialogRequested) {
            root.dialogFocusItem = root.activeFocusItem;
            portalFileChooser.requestOpen();
        } else {
            root.dialogFocusItem = null;
            portalFileChooser.cancel();
        }
    }

    PortalFileChooser {
        id: portalFileChooser
        objectName: "portalFileChooser"
        window: root
        nameFilters: root.document.nameFilters
        currentLocalPath: root.document.localPath
        onFinished: urls => {
            // Acceptance enters single view; cancellation preserves the grid and its selection.
            controller.open(urls);
            root.dialogRequested = false;
        }
        onCancelled: {
            const previousFocus = root.dialogFocusItem;
            root.dialogRequested = false;
            if (!root.modalActive && previousFocus && previousFocus.visible && previousFocus.enabled)
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
                nameFilters: root.document.nameFilters
                onAccepted: portalFileChooser.fallbackAccepted(fileDialog.selectedFile)
                onRejected: portalFileChooser.fallbackRejected()
                Component.onCompleted: fileDialog.open()
            }
        }
    }

    Loader {
        id: informationLoader
        active: root.informationOpen
        sourceComponent: ImageInformationPopup {
            document: root.document
            parent: Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: root.informationOpen = false
        }
    }

    Loader {
        id: shortcutHelpLoader
        active: root.helpOpen
        sourceComponent: ShortcutHelpPopup {
            parent: Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: root.helpOpen = false
        }
    }

    Item {
        anchors.fill: parent

        Rectangle {
            objectName: "fullscreenHeaderBackground"
            anchors.fill: viewerHeader
            color: HoloniightPalette.surface
            visible: root.fullscreen && viewerHeader.visible
            z: 1
            Accessible.ignored: true
        }
        ViewerHeader {
            id: viewerHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            title: root.headerTitle
            titleVisible: root.fullscreen
            informationAction: viewerActions.imageInformation
            fullscreenAction: viewerActions.fullscreenCommand
            modalActive: root.modalActive
            visible: !root.fullscreen || root.gridMode || root.arrowsShown || root.actionsMenuOpen || viewerHeader.controlFocused
            z: 2
            onMenuRequested: actionsMenu.open()
        }
        ViewerActionsMenu {
            id: actionsMenu
            parent: viewerHeader.menuAnchor
            windowWidth: root.width
            windowHeight: root.height
            gridMode: root.gridMode
            actions: viewerActions
            onOpened: root.actionsMenuOpen = true
            onClosed: {
                root.actionsMenuOpen = false;
                root.clearImageFocus();
            }
        }
        HnLabel {
            id: folderError
            objectName: "folderError"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: viewerHeader.bottom
            anchors.topMargin: 6
            visible: root.document.scanning || root.document.folderError.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            rawText: root.document.scanning ? qsTr("Scanning…") : root.document.folderError
            Accessible.name: rawText
        }

        Item {
            id: canvasArea
            anchors.left: parent.left
            anchors.right: parent.right
            // Fullscreen grid keeps the header and footer, since the canvas hover that reveals them is gone.
            anchors.top: root.fullscreen && !root.gridMode ? parent.top : folderError.visible ? folderError.bottom : viewerHeader.bottom
            anchors.bottom: root.fullscreen && !root.gridMode ? parent.bottom : viewerFooter.top

            ViewerImageView {
                id: canvas
                objectName: "imageCanvas"
                anchors.fill: parent
                visible: !root.gridMode
                image: root.document.image
                svgRenderer: root.document.svgRenderer
                svgSize: root.document.svgSize
                orientation: root.document.orientation
                onOrientationChanged: ++root.inputEpoch
                displayPixelRatio: root.devicePixelRatio
                activeFocusOnTab: false
                onContentReset: {
                    ++root.inputEpoch;
                    root.rendered = false;
                    root.concealDetails();
                }
                onFirstRendered: {
                    root.rendered = true;
                    root.revealDetails();
                }
                onMouseMoved: {
                    root.showArrows();
                    if (root.rendered)
                        root.revealDetails();
                }
                onKeyboardInput: root.hideArrows()
                onViewportChanged: ++root.inputEpoch
                Accessible.role: Accessible.Graphic
                Accessible.name: root.document.state === ImageDocument.Empty ? qsTr("No image open") : root.document.fileName || qsTr("Image canvas")

                inputEpoch: root.inputEpoch
                canInspect: root.canInspect
                onFocusRequested: root.clearImageFocus()
                onDetailsRequested: root.revealDetails()
            }
            ThumbnailGrid {
                id: grid
                anchors.fill: parent
                visible: root.gridMode
                model: root.document.folder
                selectionController: controller.selection
                scanning: root.document.scanning
                generation: root.document.thumbnailGeneration
                devicePixelRatio: root.devicePixelRatio
                onSelectionInteraction: root.clearImageFocus()
                onActivated: {
                    controller.activateSelection();
                    root.clearImageFocus();
                }
            }
            ViewerButton {
                id: previousButton
                objectName: "previousButton"
                // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
                readonly property bool shown: root.arrowsShown && root.document.count > 1 && !root.gridMode
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
                readonly property bool shown: root.arrowsShown && !root.gridMode
                readonly property bool playing: root.document.animation.playing
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
                enabled: root.document.animation.canToggle && !root.modalActive
                opacity: shown ? 1 : 0
                visible: root.document.animation.animated && (shown || opacity > 0)
                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
                onClicked: root.togglePlayback()
            }
            ViewerButton {
                id: nextButton
                objectName: "nextButton"
                // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
                readonly property bool shown: root.arrowsShown && root.document.count > 1 && !root.gridMode
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
                document: root.document
                magnification: canvas.magnification
                shown: root.detailsShown && !root.gridMode
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: root.fullscreen && viewerFooter.visible ? viewerFooter.height + 12 : 12
            }
            ViewerEmptyState {
                anchors.centerIn: parent
                visible: root.document.state === ImageDocument.Empty || (root.gridMode && grid.showsEmpty)
                availableWidth: canvasArea.width
                availableHeight: canvasArea.height
                devicePixelRatio: root.devicePixelRatio
            }
            ViewerLoadingIndicator {
                anchors.centerIn: parent
                loading: root.document.state === ImageDocument.Loading && !root.gridMode
                suppressed: root.gridMode || root.document.state === ImageDocument.Error
            }
            HnLabel {
                objectName: "documentFeedback"
                textFormat: Text.PlainText
                anchors.centerIn: parent
                width: canvasArea.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: !root.gridMode && root.document.state === ImageDocument.Error
                rawText: root.document.error
                Accessible.role: Accessible.StaticText
                Accessible.name: rawText
            }
        }

        Rectangle {
            objectName: "fullscreenFooterBackground"
            anchors.fill: viewerFooter
            color: HoloniightPalette.surface
            visible: root.fullscreen && viewerFooter.visible
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
            visible: !root.fullscreen || root.gridMode || root.arrowsShown
            z: 2
            HoverHandler {
                id: footerHover
            }
            HnLabel {
                Layout.fillWidth: true
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                visible: root.document.clipboard.feedback.length > 0
                rawText: root.document.clipboard.feedback
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                Accessible.name: rawText
            }
            FooterKeyHints {
                gridMode: root.gridMode
                animated: root.document.animation.canToggle
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
        enabled: !root.modalActive
        onDropped: drop => {
            controller.open(drop.urls);
            drop.acceptProposedAction();
        }
    }
}
