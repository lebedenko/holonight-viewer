pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import HolonightViewer
import "overlays"
import "grid"

Item {
    id: root
    required property ImageDocument document
    required property ViewerController controller
    required property ViewerActions actions
    required property bool fullscreen
    required property bool modalActive
    required property real displayPixelRatio
    required property ViewerOverlayController overlayController
    required property bool footerVisible
    required property real footerHeight
    readonly property bool gridMode: controller.gridMode
    readonly property bool canInspect: controller.canInspect
    readonly property bool controlsHovered: previousButton.hovered || nextButton.hovered || playPauseButton.hovered
    property bool rendered: false
    property int inputEpoch: 0
    property alias playbackButton: playPauseButton
    property alias canvas: imageCanvas
    signal focusRequested
    signal detailsRequested
    signal playbackRequested

    function pan(horizontal: real, vertical: real): void {
        imageCanvas.pan(Qt.point(horizontal, vertical));
    }
    function fit(): void {
        imageCanvas.fit();
    }
    function actualSize(): void {
        imageCanvas.actualSize();
    }
    function zoom(steps: real): void {
        imageCanvas.zoomSteps(steps, Qt.point(imageCanvas.width / 2, imageCanvas.height / 2));
    }

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
    ViewerImageView {
        id: imageCanvas
        objectName: "imageCanvas"
        anchors.fill: parent
        visible: !root.gridMode
        image: root.document.image
        svgRenderer: root.document.svgRenderer
        svgSize: root.document.svgSize
        orientation: root.document.orientation
        onOrientationChanged: ++root.inputEpoch
        displayPixelRatio: root.displayPixelRatio
        activeFocusOnTab: false
        onContentReset: {
            ++root.inputEpoch;
            root.rendered = false;
            root.overlayController.concealDetails();
        }
        onFirstRendered: {
            root.rendered = true;
            root.detailsRequested();
        }
        onMouseMoved: {
            root.overlayController.showArrows();
            if (root.rendered)
                root.detailsRequested();
        }
        onKeyboardInput: root.overlayController.hideArrows()
        onViewportChanged: ++root.inputEpoch
        Accessible.role: Accessible.Graphic
        Accessible.name: root.document.state === ImageDocument.Empty ? qsTranslate("Main", "No image open") : root.document.fileName || qsTranslate("Main", "Image canvas")

        inputEpoch: root.inputEpoch
        canInspect: root.canInspect
        onFocusRequested: root.focusRequested()
        onDetailsRequested: root.detailsRequested()
    }
    ThumbnailGrid {
        id: grid
        anchors.fill: parent
        visible: root.gridMode
        model: root.document.folder
        selectionController: root.controller.selection
        scanning: root.document.scanning
        generation: root.document.thumbnailGeneration
        devicePixelRatio: root.displayPixelRatio
        onSelectionInteraction: root.focusRequested()
        onActivated: {
            root.controller.activateSelection();
            root.focusRequested();
        }
    }
    ViewerButton {
        id: previousButton
        objectName: "previousButton"
        // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
        readonly property bool shown: root.overlayController.arrowsShown && root.document.count > 1 && !root.gridMode
        floating: true
        implicitHeight: 48
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: qsTranslate("Main", "‹")
        Accessible.name: qsTranslate("Main", "Previous image")
        // Stays visible only while fading, so a faded arrow is neither clickable nor announced.
        opacity: shown ? 1 : 0
        visible: shown || opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }
        action: root.actions.previousImage
    }
    ViewerButton {
        id: playPauseButton
        objectName: "playPauseButton"
        readonly property bool shown: root.overlayController.arrowsShown && !root.gridMode
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
        Accessible.name: playing ? qsTranslate("Main", "Pause") : qsTranslate("Main", "Play")
        enabled: root.document.animation.canToggle && !root.modalActive
        opacity: shown ? 1 : 0
        visible: root.document.animation.animated && (shown || opacity > 0)
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }
        onClicked: root.playbackRequested()
    }
    ViewerButton {
        id: nextButton
        objectName: "nextButton"
        // Browsing needs at least two images; the shared arrow reveal also drives the play/pause button.
        readonly property bool shown: root.overlayController.arrowsShown && root.document.count > 1 && !root.gridMode
        floating: true
        implicitHeight: 48
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: qsTranslate("Main", "›")
        Accessible.name: qsTranslate("Main", "Next image")
        // Stays visible only while fading, so a faded arrow is neither clickable nor announced.
        opacity: shown ? 1 : 0
        visible: shown || opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }
        action: root.actions.nextImage
    }
    ImageDetailsStrip {
        document: root.document
        magnification: imageCanvas.magnification
        shown: root.overlayController.detailsShown && !root.gridMode
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.fullscreen && root.footerVisible ? root.footerHeight + 12 : 12
    }
    ViewerEmptyState {
        anchors.centerIn: parent
        visible: root.document.state === ImageDocument.Empty || (root.gridMode && grid.showsEmpty)
        availableWidth: root.width
        availableHeight: root.height
        devicePixelRatio: root.displayPixelRatio
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
        width: root.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: !root.gridMode && root.document.state === ImageDocument.Error
        rawText: root.document.error
        Accessible.role: Accessible.StaticText
        Accessible.name: rawText
    }
}
