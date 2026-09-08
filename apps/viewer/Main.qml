pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
// Initialize the configured style before shared controls import Basic.
// qmllint disable unused-imports
import QtQuick.Controls
// qmllint enable unused-imports
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

    property bool restoreMaximized: false
    property bool dialogRequested: false
    readonly property bool canInspect: document.state === ImageDocument.Ready && !dialogRequested
    property int inputEpoch: 0
    onDialogRequestedChanged: ++window.inputEpoch

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
        if (window.visibility === Window.FullScreen) {
            if (window.restoreMaximized)
                window.showMaximized();
            else
                window.showNormal();
        }
    }

    Shortcut {
        sequences: [StandardKey.Open]
        enabled: !window.dialogRequested
        onActivated: window.dialogRequested = true
    }
    Shortcut {
        sequence: "F"
        enabled: !window.dialogRequested
        onActivated: {
            if (window.visibility === Window.FullScreen) {
                window.leaveFullscreen();
            } else {
                window.restoreMaximized = window.visibility === Window.Maximized;
                window.showFullScreen();
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: !window.dialogRequested
        onActivated: window.leaveFullscreen()
    }
    Shortcut {
        sequence: "Q"
        enabled: !window.dialogRequested
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: HnMetrics.internalSpacing(HnControlSize.Normal)
        spacing: HnMetrics.internalSpacing(HnControlSize.Normal)

        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: "openButton"
                text: qsTr("Open…")
                enabled: !window.dialogRequested
                onClicked: window.dialogRequested = true
            }
            HnLabel {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                rawText: window.document.fileName
                elide: Text.ElideMiddle
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: HnMetrics.internalSpacing(HnControlSize.Normal)

            Button {
                objectName: "fitButton"
                text: qsTr("Fit")
                enabled: window.canInspect
                onClicked: window.fitImage()
            }
            Button {
                objectName: "actualSizeButton"
                text: qsTr("Actual Size")
                enabled: window.canInspect
                onClicked: window.actualSizeImage()
            }
            Button {
                objectName: "zoomOutButton"
                text: qsTr("−")
                Accessible.name: qsTr("Zoom out")
                enabled: window.canInspect
                onClicked: window.zoomImage(-1)
            }
            Button {
                objectName: "zoomInButton"
                text: qsTr("+")
                Accessible.name: qsTr("Zoom in")
                enabled: window.canInspect
                onClicked: window.zoomImage(1)
            }
        }

        HnLabel {
            objectName: "zoomStatus"
            Layout.fillWidth: true
            textFormat: Text.PlainText
            rawText: window.document.state !== ImageDocument.Ready ? qsTr("—") : canvas.fitting ? qsTr("Fit") : qsTr("%1%").arg(Number(canvas.magnification * 100).toLocaleString(Qt.locale(), 'f', canvas.magnification < 0.01 ? 4 : 1))
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
                displayPixelRatio: window.devicePixelRatio
                activeFocusOnTab: window.canInspect
                onImageChanged: ++window.inputEpoch
                onViewportChanged: ++window.inputEpoch
                Keys.enabled: window.canInspect
                Keys.onLeftPressed: canvas.pan(Qt.point(40, 0))
                Keys.onRightPressed: canvas.pan(Qt.point(-40, 0))
                Keys.onUpPressed: canvas.pan(Qt.point(0, 40))
                Keys.onDownPressed: canvas.pan(Qt.point(0, -40))
                Accessible.role: Accessible.Graphic
                Accessible.name: window.document.fileName

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
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: HnMetrics.borderWidth
                    border.color: HoloniightPalette.borderFocus
                    visible: canvas.activeFocus && window.canInspect
                    Accessible.ignored: true
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
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            rawText: window.visibility === Window.FullScreen ? qsTr("Ctrl+O  open    f  windowed    Esc  leave fullscreen    q  quit\n0  fit    1  actual size    +/− or wheel  zoom    drag  pan    arrows  pan focused canvas") : qsTr("Ctrl+O  open    f  fullscreen    q  quit\n0  fit    1  actual size    +/− or wheel  zoom    drag  pan    arrows  pan focused canvas")
        }
    }

    DropArea {
        objectName: "imageDropArea"
        anchors.fill: parent
        enabled: !window.dialogRequested
        onDropped: drop => {
            window.document.open(drop.urls);
            drop.acceptProposedAction();
        }
    }
}
