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

    // Every Open trigger sets dialogRequested; the portal picker runs first and the
    // FileDialog below appears only when the portal cannot serve the request.
    onDialogRequestedChanged: {
        if (root.dialogRequested) {
            root.dialogFocusItem = root.applicationWindow.activeFocusItem;
            portalFileChooser.requestOpen();
        } else {
            root.dialogFocusItem = null;
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
                title: qsTranslate("Main", "Open image")
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
