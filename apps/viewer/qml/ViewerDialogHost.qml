pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls as Controls
import HolonightViewer
import "information"
import "shortcuts"

Item {
    id: root
    required property Window applicationWindow
    required property ImageDocument document
    required property ViewerController controller
    property bool dialogRequested: false
    property Item dialogFocusItem: null
    property bool informationOpen: false
    property bool helpOpen: false
    readonly property bool modalActive: dialogRequested || informationOpen || helpOpen

    function cancelOpen(): void {
        const previousFocus = root.dialogFocusItem;
        root.dialogRequested = false;
        if (!root.modalActive && previousFocus && previousFocus.visible && previousFocus.enabled)
            previousFocus.forceActiveFocus(Qt.OtherFocusReason);
    }

    // Every Open trigger starts with the system chooser.
    onDialogRequestedChanged: {
        if (root.dialogRequested) {
            root.dialogFocusItem = root.applicationWindow.activeFocusItem;
            portalFileChooser.requestOpen();
        } else {
            root.dialogFocusItem = null;
            recoveryDialog.close();
            portalFileChooser.cancel();
        }
    }

    PortalFileChooser {
        id: portalFileChooser
        objectName: "portalFileChooser"
        window: root.applicationWindow
        nameFilters: root.document.nameFilters
        currentLocalPath: root.document.localPath
        onFinished: urls => {
            // Acceptance enters single view; cancellation preserves the grid and its selection.
            root.controller.open(urls);
            root.dialogRequested = false;
        }
        onCancelled: root.cancelOpen()
        onPortalEnded: recoveryDialog.open()
    }

    Controls.Dialog {
        id: recoveryDialog
        objectName: "chooserRecoveryDialog"
        parent: root.applicationWindow.Controls.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Controls.Popup.CloseOnEscape
        title: qsTranslate("Main", "Open image")
        onAccepted: portalFileChooser.openBuiltInChooser()
        onRejected: root.cancelOpen()

        contentItem: Controls.Label {
            text: qsTr("The system chooser closed without returning a file.")
            wrapMode: Text.WordWrap
        }

        footer: Controls.DialogButtonBox {
            standardButtons: Controls.DialogButtonBox.Cancel

            Controls.Button {
                objectName: "openBuiltInChooserButton"
                text: qsTr("Open with built-in chooser")
                Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.AcceptRole
            }
        }
    }

    Loader {
        active: portalFileChooser.fallbackShown
        sourceComponent: Item {
            FileDialog {
                id: fileDialog
                objectName: "openDialog"
                title: qsTranslate("Main", "Open image")
                fileMode: FileDialog.OpenFile
                options: FileDialog.DontUseNativeDialog
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
            parent: root.applicationWindow.Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: root.informationOpen = false
        }
    }

    Loader {
        id: shortcutHelpLoader
        active: root.helpOpen
        sourceComponent: ShortcutHelpPopup {
            parent: root.applicationWindow.Controls.Overlay.overlay
            Component.onCompleted: open()
            onClosed: root.helpOpen = false
        }
    }
}
