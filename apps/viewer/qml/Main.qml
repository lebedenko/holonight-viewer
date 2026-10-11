pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import "header"
import "overlays"
import "footer"

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

    property alias rendered: stage.rendered
    property bool actionsMenuOpen: false
    readonly property bool fullscreen: visibility === Window.FullScreen
    readonly property bool controlsHovered: stage.controlsHovered || viewerHeader.hovered || footerHover.hovered
    property alias arrowsShown: overlayTiming.arrowsShown
    property alias countdownActive: overlayTiming.countdownActive
    property alias detailsShown: overlayTiming.detailsShown
    ViewerOverlayController {
        id: overlayTiming
        fullscreen: root.fullscreen
        controlsHovered: root.controlsHovered
        menuOpen: root.actionsMenuOpen
        imageReady: root.documentState === ImageDocument.Ready
    }
    function showArrows(): void {
        overlayTiming.showArrows();
    }
    function hideArrows(): void {
        overlayTiming.hideArrows();
    }
    function togglePlayback(): void {
        viewerController.togglePlayback();
        // The keyboard hides the arrows, so the strip is what shows the new state.
        root.revealDetails();
        root.clearImageFocus();
    }
    function revealDetails(): void {
        overlayTiming.revealDetails();
    }
    function concealDetails(): void {
        overlayTiming.concealDetails();
    }
    function toggleFullscreen(): void {
        WindowState.setFullscreen(root, root.visibility !== Window.FullScreen);
    }

    property alias dialogRequested: dialogHost.dialogRequested
    property alias dialogFocusItem: dialogHost.dialogFocusItem
    property alias informationOpen: dialogHost.informationOpen
    property alias helpOpen: dialogHost.helpOpen
    readonly property bool modalActive: dialogHost.modalActive
    // Grid view replaces the canvas; everything that acts on the picture is gated by canInspect.
    ViewerController {
        id: viewerController
        objectName: "viewerController"
        document: root.document
        modalActive: root.modalActive
        windowSuspended: !root.visible || root.visibility === Window.Minimized
    }
    readonly property bool gridMode: viewerController.gridMode
    readonly property bool canEnterGrid: viewerController.canEnterGrid
    readonly property bool canToggleGrid: viewerController.canToggleGrid
    readonly property bool canInspect: viewerController.canInspect
    readonly property bool hasPath: viewerController.hasPath
    readonly property bool canShowInformation: viewerController.canShowInformation
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
    property alias inputEpoch: stage.inputEpoch
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
        playbackButton: stage.playbackButton
        imageReady: root.canInspect
        modalActive: root.modalActive
        menuOpen: root.actionsMenuOpen
        playbackAvailable: root.document.animation.canToggle && root.canInspect
        gridActive: root.gridMode
        onGridMoveRequested: move => {
            viewerController.moveSelection(move);
            root.clearImageFocus();
        }
        onGridActivateRequested: {
            viewerController.activateSelection();
            root.clearImageFocus();
        }
        onBrowseRequested: direction => {
            const action = direction < 0 ? viewerActions.previousImage : viewerActions.nextImage;
            if (action.enabled)
                action.trigger();
        }
        onPlaybackToggleRequested: root.togglePlayback()
        onPanRequested: (horizontal, vertical) => {
            stage.pan(horizontal, vertical);
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
            stage.canvas.Accessible.announce(qsTr("Loaded %1.").arg(root.document.fileName));
        else if (documentState === ImageDocument.Error)
            stage.canvas.Accessible.announce(qsTr("Error opening %1: %2").arg(root.document.fileName).arg(root.document.error));
    }

    Connections {
        target: root.document.animation
        function onFailureNoticeChanged(notice: string): void {
            root.revealDetails();
            stage.canvas.Accessible.announce(notice);
        }
    }

    Connections {
        target: root.document.clipboard
        function onChanged(): void {
            if (!root.document.clipboard.busy && root.document.clipboard.feedback.length > 0)
                stage.canvas.Accessible.announce(root.document.clipboard.feedback);
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
        onPasteRequested: viewerController.paste()
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
        return direction < 0 ? viewerController.canPrevious : viewerController.canNext;
    }
    function browse(direction: int): void {
        viewerController.browse(direction);
        root.clearImageFocus();
    }
    function enterGrid(): void {
        viewerController.enterGrid();
        root.clearImageFocus();
    }
    function leaveGrid(): void {
        viewerController.leaveGrid();
        root.clearImageFocus();
    }
    function toggleGrid(): void {
        viewerController.toggleGrid();
        root.clearImageFocus();
    }
    function refreshFolder(): void {
        viewerController.refresh();
        root.clearImageFocus();
    }

    function fitImage(): void {
        stage.fit();
        root.revealDetails();
        root.clearImageFocus();
    }

    function actualSizeImage(): void {
        stage.actualSize();
        root.revealDetails();
        root.clearImageFocus();
    }

    function zoomImage(steps: real): void {
        stage.zoom(steps);
        root.revealDetails();
        root.clearImageFocus();
    }

    function leaveFullscreen(): void {
        if (root.visibility === Window.FullScreen)
            WindowState.setFullscreen(root, false);
    }

    ViewerDialogHost {
        id: dialogHost
        applicationWindow: root
        document: root.document
        controller: viewerController
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

        ViewerStage {
            id: stage
            anchors.left: parent.left
            anchors.right: parent.right
            // Fullscreen grid keeps the header and footer, since the canvas hover that reveals them is gone.
            anchors.top: root.fullscreen && !root.gridMode ? parent.top : folderError.visible ? folderError.bottom : viewerHeader.bottom
            anchors.bottom: root.fullscreen && !root.gridMode ? parent.bottom : viewerFooter.top

            document: root.document
            controller: viewerController
            actions: viewerActions
            fullscreen: root.fullscreen
            modalActive: root.modalActive
            displayPixelRatio: root.devicePixelRatio
            overlayController: overlayTiming
            footerVisible: viewerFooter.visible
            footerHeight: viewerFooter.height
            onFocusRequested: root.clearImageFocus()
            onDetailsRequested: root.revealDetails()
            onPlaybackRequested: root.togglePlayback()
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
            viewerController.open(drop.urls);
            drop.acceptProposedAction();
        }
    }
}
