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

    function leaveFullscreen(): void {
        if (window.visibility === Window.FullScreen) {
            if (window.restoreMaximized)
                window.showMaximized();
            else
                window.showNormal();
        }
    }

    Shortcut {
        sequence: StandardKey.Open
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

        Item {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            ImageCanvas {
                objectName: "imageCanvas"
                anchors.fill: parent
                image: window.document.image
                Accessible.role: Accessible.Graphic
                Accessible.name: window.document.fileName
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
            rawText: window.visibility === Window.FullScreen ? qsTr("Ctrl+O  open    f  windowed    Esc  leave fullscreen    q  quit") : qsTr("Ctrl+O  open    f  fullscreen    q  quit")
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
