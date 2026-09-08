pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
// Initialize the configured style before shared controls import Basic.
// qmllint disable unused-imports
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
// qmllint enable unused-imports
import Holonight as HnStyle
import Holonight.Core
import Holonight.Controls

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

    component ViewerButton: HnStyle.Button {
        id: control
        property bool floating: false
        implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, HnMetrics.controlHeight(HnControlSize.Normal))
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : control.floating ? Qt.alpha(HoloniightPalette.surface, 0.85) : "transparent"
            border.color: control.floating ? HoloniightPalette.borderPassive : "transparent"
            radius: HnMetrics.internalSpacing(HnControlSize.Compact)
        }
    }
    component ViewerHeaderButton: ViewerButton {
        display: AbstractButton.IconOnly
        icon.width: HnMetrics.iconSize(HnControlSize.Hero)
        icon.height: HnMetrics.iconSize(HnControlSize.Hero)
        implicitWidth: HnMetrics.controlHeight(HnControlSize.Large)
        implicitHeight: HnMetrics.controlHeight(HnControlSize.Large)
    }
    component ViewerMenuItem: HnStyle.MenuItem {
        id: control
        contentItem: HnLabel {
            rawText: control.text
            textFormat: Text.PlainText
            color: control.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textDisabled
        }
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : "transparent"
        }
    }
    property bool rendered: false
    Timer {
        id: arrowTimer
        objectName: "arrowTimer"
        interval: 5000
    }
    Timer {
        id: detailsTimer
        objectName: "detailsTimer"
        interval: 5000
    }
    function toggleFullscreen(): void {
        WindowState.setFullscreen(window, window.visibility !== Window.FullScreen);
    }

    property bool dialogRequested: false
    property int detailDialog: 0
    readonly property bool modalActive: dialogRequested || detailDialog !== 0
    readonly property bool canInspect: document.state === ImageDocument.Ready && !modalActive
    readonly property bool hasPath: document.localPath.length > 0 && !modalActive
    property int inputEpoch: 0
    onModalActiveChanged: {
        ++window.inputEpoch;
        if (!modalActive)
            canvas.forceActiveFocus(Qt.OtherFocusReason);
    }

    readonly property int documentState: document.state
    onDocumentStateChanged: {
        if (documentState !== ImageDocument.Ready)
            detailsTimer.stop();
        if (documentState === ImageDocument.Ready)
            canvas.Accessible.announce(qsTr("Loaded %1.").arg(window.document.fileName));
        else if (documentState === ImageDocument.Error)
            canvas.Accessible.announce(qsTr("Error opening %1: %2").arg(window.document.fileName).arg(window.document.error));
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

    Action {
        id: rotateClockwise
        objectName: "rotateClockwiseAction"
        text: qsTr("Rotate Clockwise")
        shortcut: "R"
        enabled: window.canInspect
        onTriggered: window.transformImage(1)
    }
    Action {
        id: rotateCounterclockwise
        objectName: "rotateCounterclockwiseAction"
        text: qsTr("Rotate Counterclockwise")
        shortcut: "Shift+R"
        enabled: window.canInspect
        onTriggered: window.transformImage(3)
    }
    Action {
        id: flipHorizontal
        objectName: "flipHorizontalAction"
        text: qsTr("Flip Horizontally")
        shortcut: "H"
        enabled: window.canInspect
        onTriggered: window.transformImage(4)
    }
    Action {
        id: flipVertical
        objectName: "flipVerticalAction"
        text: qsTr("Flip Vertically")
        shortcut: "V"
        enabled: window.canInspect
        onTriggered: window.transformImage(6)
    }
    Action {
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
    Action {
        id: copyImage
        objectName: "copyImageAction"
        text: qsTr("Copy Image")
        shortcut: "Ctrl+C"
        enabled: window.canInspect && !window.document.clipboard.busy
        onTriggered: window.document.copyImage()
    }
    Action {
        id: copyPath
        objectName: "copyPathAction"
        text: qsTr("Copy Path")
        shortcut: "Ctrl+Shift+C"
        enabled: window.hasPath && !window.document.clipboard.busy
        onTriggered: window.document.copyPath()
    }
    Action {
        id: imageInformation
        objectName: "imageInformationAction"
        text: qsTr("Image Information")
        shortcut: "I"
        enabled: window.hasPath
        onTriggered: window.detailDialog = 1
    }
    Action {
        id: shortcutHelp
        objectName: "shortcutHelpAction"
        text: qsTr("Shortcut Help")
        shortcut: "F1"
        enabled: !window.modalActive
        onTriggered: window.detailDialog = 2
    }

    Shortcut {
        sequence: "0"
        enabled: window.canInspect
        onActivated: window.fitImage()
    }
    Shortcut {
        sequence: "1"
        enabled: window.canInspect
        onActivated: window.actualSizeImage()
    }
    Shortcut {
        sequences: ["+", "="]
        enabled: window.canInspect
        onActivated: window.zoomImage(1)
    }
    Shortcut {
        sequence: "-"
        enabled: window.canInspect
        onActivated: window.zoomImage(-1)
    }

    Shortcut {
        sequence: "PgUp"
        enabled: !window.modalActive && window.document.canPrevious
        onActivated: window.browse(-1)
    }
    Shortcut {
        sequence: "PgDown"
        enabled: !window.modalActive && window.document.canNext
        onActivated: window.browse(1)
    }
    Shortcut {
        sequence: "F5"
        enabled: !window.modalActive && window.document.localPath.length > 0
        onActivated: window.refreshFolder()
    }

    function browse(direction: int): void {
        if (direction < 0)
            window.document.previous();
        else
            window.document.next();
        canvas.forceActiveFocus(Qt.OtherFocusReason);
    }

    function refreshFolder(): void {
        window.document.refresh();
        canvas.forceActiveFocus(Qt.OtherFocusReason);
    }

    function fitImage(): void {
        canvas.fit();
        canvas.forceActiveFocus(Qt.OtherFocusReason);
    }

    function actualSizeImage(): void {
        canvas.actualSize();
        canvas.forceActiveFocus(Qt.OtherFocusReason);
    }

    function zoomImage(steps: real): void {
        canvas.zoomSteps(steps, Qt.point(canvas.width / 2, canvas.height / 2));
        canvas.forceActiveFocus(Qt.OtherFocusReason);
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

    Loader {
        active: window.dialogRequested
        sourceComponent: Item {
            FileDialog {
                id: fileDialog
                objectName: "openDialog"
                title: qsTr("Open image")
                fileMode: FileDialog.OpenFile
                nameFilters: window.document.nameFilters
                onAccepted: {
                    window.document.open([fileDialog.selectedFile]);
                    window.dialogRequested = false;
                }
                onRejected: window.dialogRequested = false
                Component.onCompleted: fileDialog.open()
            }
        }
    }

    Loader {
        id: detailLoader
        active: window.detailDialog !== 0
        sourceComponent: Item {
            Basic.Dialog {
                id: details
                objectName: "detailsDialog"
                parent: Overlay.overlay
                anchors.centerIn: parent
                width: Math.min(620, window.width - 24)
                height: Math.min(500, window.height - 24)
                palette.window: HoloniightPalette.surfaceRaised
                palette.windowText: HoloniightPalette.textPrimary
                palette.button: HoloniightPalette.surface
                palette.buttonText: HoloniightPalette.textPrimary
                palette.mid: HoloniightPalette.borderPassive
                modal: true
                focus: true
                title: window.detailDialog === 1 ? qsTr("Image Information") : qsTr("Shortcut Help")
                footer: Item {
                    implicitHeight: closeDetails.height + 12
                    ViewerButton {
                        id: closeDetails
                        objectName: "closeDetailsButton"
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        text: qsTr("Close")
                        onClicked: details.close()
                    }
                }
                closePolicy: Popup.CloseOnEscape
                onClosed: window.detailDialog = 0
                Component.onCompleted: open()
                contentItem: ScrollView {
                    objectName: "detailsScroll"
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.active: true
                    HnStyle.TextArea {
                        objectName: "detailsText"
                        background: Rectangle {
                            color: HoloniightPalette.surface
                            border.color: HoloniightPalette.borderPassive
                        }
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        Accessible.name: details.title
                        text: window.detailDialog === 1 ? window.document.informationText : qsTr("Ctrl+O — Open image\nPage Up / Page Down — Previous / next image\nF5 — Refresh folder and image\n\n0 — Fit\n1 — Actual Size (physical pixels)\n+ / = / − — Zoom at center\nMouse wheel / touchpad scroll — Zoom at pointer\nLeft-button drag — Pan\nArrow keys — Pan focused canvas\nTab / Shift+Tab — Move keyboard focus\n\nR / Shift+R — Rotate clockwise / counterclockwise\nH / V — Flip horizontally / vertically\nActions → Reset Transform — Clear temporary transforms\nCtrl+C — Copy entire transformed image\nCtrl+Shift+C — Copy file path\nI — Image Information\nF1 — Shortcut Help\n\nF — Toggle fullscreen\nEscape — Close dialog, or leave fullscreen\nQ — Quit\nDrop one local image — Open image")
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)

        HnHeaderBar {
            Layout.fillWidth: true
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
                        objectName: "informationButton"
                        icon.source: "icons/information.svg"
                        Accessible.name: qsTr("Image Information")
                        enabled: window.hasPath
                        onClicked: window.detailDialog = 1
                    }
                    ViewerHeaderButton {
                        objectName: "fullscreenButton"
                        icon.source: "icons/fullscreen.svg"
                        Accessible.name: qsTr("Fullscreen")
                        enabled: !window.modalActive
                        onClicked: window.toggleFullscreen()
                    }
                    ViewerHeaderButton {
                        id: actionsButton
                        objectName: "actionsButton"
                        icon.source: "icons/menu.svg"
                        Accessible.name: qsTr("Actions")
                        enabled: !window.modalActive
                        onClicked: actionsMenu.open()
                        HnStyle.Menu {
                            id: actionsMenu
                            objectName: "actionsMenu"
                            popupType: Popup.Item
                            margins: 12
                            x: actionsButton.width - width
                            width: Math.min(340, window.width - 24)
                            height: Math.min(implicitHeight, window.height - 24)
                            onClosed: if (!window.modalActive)
                                canvas.forceActiveFocus(Qt.OtherFocusReason)
                            y: actionsButton.height + HnMetrics.internalSpacing(HnControlSize.Normal)
                            contentItem: ListView {
                                implicitHeight: contentHeight
                                model: actionsMenu.contentModel
                                currentIndex: actionsMenu.currentIndex
                                interactive: contentHeight > height
                                clip: true
                                ScrollBar.vertical: ScrollBar {
                                    active: true
                                }
                            }

                            ViewerMenuItem {
                                objectName: "openButton"
                                text: qsTr("Open…")
                                enabled: !window.modalActive
                                onTriggered: window.dialogRequested = true
                            }
                            ViewerMenuItem {
                                objectName: "fitButton"
                                text: qsTr("Fit")
                                enabled: window.canInspect
                                onTriggered: window.fitImage()
                            }
                            ViewerMenuItem {
                                objectName: "actualSizeButton"
                                text: qsTr("Actual Size")
                                enabled: window.canInspect
                                onTriggered: window.actualSizeImage()
                            }
                            ViewerMenuItem {
                                objectName: "zoomOutButton"
                                text: qsTr("Zoom out")
                                enabled: window.canInspect
                                onTriggered: window.zoomImage(-1)
                            }
                            ViewerMenuItem {
                                objectName: "zoomInButton"
                                text: qsTr("Zoom in")
                                enabled: window.canInspect
                                onTriggered: window.zoomImage(1)
                            }
                            ViewerMenuItem {
                                text: qsTr("Previous")
                                enabled: !window.modalActive && window.document.canPrevious
                                onTriggered: window.browse(-1)
                            }
                            ViewerMenuItem {
                                text: qsTr("Next")
                                enabled: !window.modalActive && window.document.canNext
                                onTriggered: window.browse(1)
                            }
                            ViewerMenuItem {
                                text: qsTr("Refresh")
                                enabled: window.hasPath
                                onTriggered: window.refreshFolder()
                            }
                            ViewerMenuItem {
                                text: qsTr("Fullscreen")
                                onTriggered: window.toggleFullscreen()
                            }
                            ViewerMenuItem {
                                text: qsTr("Quit")
                                onTriggered: window.close()
                            }
                            ViewerMenuItem {
                                action: rotateClockwise
                            }
                            ViewerMenuItem {
                                action: rotateCounterclockwise
                            }
                            ViewerMenuItem {
                                action: flipHorizontal
                            }
                            ViewerMenuItem {
                                action: flipVertical
                            }
                            ViewerMenuItem {
                                action: resetTransform
                                objectName: "resetTransformMenuItem"
                            }
                            ViewerMenuItem {
                                action: copyImage
                            }
                            ViewerMenuItem {
                                action: copyPath
                            }
                            ViewerMenuItem {
                                action: imageInformation
                                objectName: "informationMenuItem"
                            }
                            ViewerMenuItem {
                                action: shortcutHelp
                            }
                        }
                    }
                }
            }
        }
        HnLabel {
            objectName: "folderError"
            Layout.fillWidth: true
            visible: window.document.scanning || window.document.folderError.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            rawText: window.document.scanning ? qsTr("Scanning…") : window.document.folderError
            Accessible.name: rawText
        }

        Item {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            ImageCanvas {
                id: canvas
                objectName: "imageCanvas"
                anchors.fill: parent
                image: window.document.image
                orientation: window.document.orientation
                onOrientationChanged: ++window.inputEpoch
                displayPixelRatio: window.devicePixelRatio
                activeFocusOnTab: true
                onImageChanged: {
                    ++window.inputEpoch;
                    window.rendered = false;
                    detailsTimer.stop();
                }
                onFirstRendered: {
                    window.rendered = true;
                    detailsTimer.restart();
                }
                onMouseMoved: {
                    arrowTimer.restart();
                    if (window.rendered && window.documentState === ImageDocument.Ready)
                        detailsTimer.restart();
                }
                onKeyboardInput: arrowTimer.stop()
                onViewportChanged: ++window.inputEpoch
                Keys.enabled: window.canInspect
                Keys.onLeftPressed: canvas.pan(Qt.point(40, 0))
                Keys.onRightPressed: canvas.pan(Qt.point(-40, 0))
                Keys.onUpPressed: canvas.pan(Qt.point(0, 40))
                Keys.onDownPressed: canvas.pan(Qt.point(0, -40))
                Accessible.role: Accessible.Graphic
                Accessible.name: window.document.fileName || qsTr("Image canvas")

                TapHandler {
                    enabled: window.canInspect
                    acceptedButtons: Qt.LeftButton
                    onPressedChanged: if (pressed)
                        canvas.forceActiveFocus(Qt.MouseFocusReason)
                }
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
                            canvas.forceActiveFocus(Qt.MouseFocusReason);
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
                            canvas.forceActiveFocus(Qt.MouseFocusReason);
                            event.accepted = true;
                        } else {
                            event.accepted = false;
                        }
                    }
                }
            }
            ViewerButton {
                objectName: "previousButton"
                floating: true
                implicitHeight: 48
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("‹")
                Accessible.name: qsTr("Previous image")
                visible: arrowTimer.running
                enabled: !window.modalActive && window.document.canPrevious
                onClicked: window.browse(-1)
            }
            ViewerButton {
                objectName: "nextButton"
                floating: true
                implicitHeight: 48
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("›")
                Accessible.name: qsTr("Next image")
                visible: arrowTimer.running
                enabled: !window.modalActive && window.document.canNext
                onClicked: window.browse(1)
            }
            Rectangle {
                objectName: "detailsStrip"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                width: Math.min(parent.width - 24, stripFilename.implicitWidth + metadata.implicitWidth + 36)
                height: detailsFlow.height + 16
                radius: HnMetrics.internalSpacing(HnControlSize.Compact)
                color: Qt.alpha(HoloniightPalette.surface, 0.85)
                border.color: HoloniightPalette.borderPassive
                visible: window.documentState === ImageDocument.Ready && detailsTimer.running
                Flow {
                    id: detailsFlow
                    x: 12
                    y: 8
                    width: parent.width - 24
                    spacing: 12
                    HnLabel {
                        id: stripFilename
                        width: Math.min(implicitWidth, Math.max(80, detailsFlow.width - metadata.implicitWidth - 12))
                        rawText: window.document.fileName
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                    }
                    HnLabel {
                        id: metadata
                        width: Math.min(implicitWidth, detailsFlow.width)
                        wrapMode: Text.Wrap
                        rawText: qsTr("|  %1 × %2  |  %3  |  %4%  |  %5 / %6").arg(window.document.transformedDimensions.width).arg(window.document.transformedDimensions.height).arg(window.document.formattedFileSize).arg(Number(canvas.magnification * 100).toLocaleString(Qt.locale(), 'f', canvas.magnification < 0.01 ? 4 : 1)).arg(window.document.position).arg(window.document.count)
                        textFormat: Text.PlainText
                    }
                }
            }
            HnEmptyState {
                objectName: "emptyState"
                anchors.centerIn: parent
                visible: window.document.state === ImageDocument.Empty
                titleText: qsTr("No image open")
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

        HnLabel {
            Layout.fillWidth: true
            visible: window.document.clipboard.feedback.length > 0
            rawText: window.document.clipboard.feedback
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            Accessible.name: rawText
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 6
            spacing: 12
            Repeater {
                model: [qsTr("PgUp/PgDown  navigate"), qsTr("+/−  zoom"), qsTr("0  fit"), qsTr("1  100%"), qsTr("R  rotate"), qsTr("F  fullscreen"), qsTr("I  information"), qsTr("F1  help"), qsTr("Q  quit")]
                HnLabel {
                    required property string modelData
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                    rawText: modelData
                }
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
