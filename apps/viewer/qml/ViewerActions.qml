pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls

Item {
    id: root
    required property ImageDocument document
    required property bool modalActive
    required property bool hasPath
    required property bool canToggleGrid
    required property bool canInspect
    required property bool canShowInformation
    required property bool gridMode
    required property bool actionsMenuOpen
    required property bool canPrevious
    required property bool canNext
    property alias openImage: openImage
    property alias refresh: refresh
    property alias gridToggle: gridToggle
    property alias previousImage: previousImage
    property alias nextImage: nextImage
    property alias fit: fit
    property alias actualSize: actualSize
    property alias zoomIn: zoomIn
    property alias zoomOut: zoomOut
    property alias fullscreenCommand: fullscreenCommand
    property alias quit: quit
    property alias rotateClockwise: rotateClockwise
    property alias rotateCounterclockwise: rotateCounterclockwise
    property alias flipHorizontal: flipHorizontal
    property alias flipVertical: flipVertical
    property alias resetTransform: resetTransform
    property alias copyImage: copyImage
    property alias copyPath: copyPath
    property alias imageInformation: imageInformation
    property alias shortcutHelp: shortcutHelp
    signal openRequested
    signal informationRequested
    signal helpRequested
    signal refreshRequested
    signal gridRequested
    signal browseRequested(int direction)
    signal fitRequested
    signal actualSizeRequested
    signal zoomRequested(real steps)
    signal fullscreenRequested
    signal quitRequested
    signal transformRequested(int operation)
    signal resetRequested
    signal focusRequested
    signal leaveGridRequested
    signal leaveFullscreenRequested
    Controls.Action {
        id: openImage
        objectName: "openImageAction"
        text: qsTranslate("Main", "Open…")
        enabled: !root.modalActive
        onTriggered: root.openRequested()
    }
    Controls.Action {
        id: refresh
        objectName: "refreshAction"
        text: qsTranslate("Main", "Refresh")
        enabled: root.hasPath
        onTriggered: root.refreshRequested()
    }
    Controls.Action {
        id: gridToggle
        objectName: "gridToggleAction"
        text: qsTranslate("Main", "Grid View")
        enabled: !root.modalActive && root.canToggleGrid
        onTriggered: root.gridRequested()
    }
    Controls.Action {
        id: previousImage
        objectName: "previousImageAction"
        text: qsTranslate("Main", "Previous")
        enabled: root.canPrevious
        onTriggered: root.browseRequested(-1)
    }
    Controls.Action {
        id: nextImage
        objectName: "nextImageAction"
        text: qsTranslate("Main", "Next")
        enabled: root.canNext
        onTriggered: root.browseRequested(1)
    }
    Controls.Action {
        id: fit
        objectName: "fitAction"
        text: qsTranslate("Main", "Fit")
        enabled: root.canInspect
        onTriggered: root.fitRequested()
    }
    Controls.Action {
        id: actualSize
        objectName: "actualSizeAction"
        text: qsTranslate("Main", "Actual Size")
        enabled: root.canInspect
        onTriggered: root.actualSizeRequested()
    }
    Controls.Action {
        id: zoomIn
        objectName: "zoomInAction"
        text: qsTranslate("Main", "Zoom In")
        enabled: root.canInspect
        onTriggered: root.zoomRequested(1)
    }
    Controls.Action {
        id: zoomOut
        objectName: "zoomOutAction"
        text: qsTranslate("Main", "Zoom Out")
        enabled: root.canInspect
        onTriggered: root.zoomRequested(-1)
    }
    Controls.Action {
        id: fullscreenCommand
        objectName: "fullscreenCommandAction"
        text: qsTranslate("Main", "Fullscreen")
        enabled: !root.modalActive
        onTriggered: root.fullscreenRequested()
    }
    Controls.Action {
        id: quit
        objectName: "quitAction"
        text: qsTranslate("Main", "Quit")
        enabled: !root.modalActive
        onTriggered: root.quitRequested()
    }

    Controls.Action {
        id: rotateClockwise
        objectName: "rotateClockwiseAction"
        text: qsTranslate("Main", "Rotate Clockwise")
        shortcut: "R"
        enabled: root.canInspect
        onTriggered: root.transformRequested(1)
    }
    Controls.Action {
        id: rotateCounterclockwise
        objectName: "rotateCounterclockwiseAction"
        text: qsTranslate("Main", "Rotate Counterclockwise")
        shortcut: "Shift+R"
        enabled: root.canInspect
        onTriggered: root.transformRequested(3)
    }
    Controls.Action {
        id: flipHorizontal
        objectName: "flipHorizontalAction"
        text: qsTranslate("Main", "Flip Horizontally")
        shortcut: "X"
        enabled: root.canInspect
        onTriggered: root.transformRequested(4)
    }
    Controls.Action {
        id: flipVertical
        objectName: "flipVerticalAction"
        text: qsTranslate("Main", "Flip Vertically")
        shortcut: "Shift+X"
        enabled: root.canInspect
        onTriggered: root.transformRequested(6)
    }
    Controls.Action {
        id: resetTransform
        objectName: "resetTransformAction"
        text: qsTranslate("Main", "Reset Transform")
        enabled: root.canInspect
        onTriggered: {
            root.resetRequested();
        }
    }
    Controls.Action {
        id: copyImage
        objectName: "copyImageAction"
        text: qsTranslate("Main", "Copy Image")
        shortcut: "Ctrl+C"
        enabled: root.canInspect && !root.document.clipboard.busy
        onTriggered: {
            root.document.copyImage();
            root.focusRequested();
        }
    }
    Controls.Action {
        id: copyPath
        objectName: "copyPathAction"
        text: qsTranslate("Main", "Copy Path")
        shortcut: "Ctrl+Shift+C"
        enabled: root.hasPath && !root.document.clipboard.busy
        onTriggered: {
            root.document.copyPath();
            root.focusRequested();
        }
    }
    Controls.Action {
        id: imageInformation
        objectName: "imageInformationAction"
        text: qsTranslate("Main", "Image Information")
        shortcut: "I"
        // While open, the modal popup blocks window shortcuts and handles I itself.
        enabled: root.canShowInformation
        onTriggered: root.informationRequested()
    }
    Controls.Action {
        id: shortcutHelp
        objectName: "shortcutHelpAction"
        text: qsTranslate("Main", "Shortcut Help")
        shortcut: "?"
        // While open, the modal popup blocks window shortcuts and handles ? itself.
        enabled: !root.modalActive
        onTriggered: root.helpRequested()
    }

    Shortcut {
        sequence: "Ctrl+0"
        enabled: fit.enabled
        onActivated: fit.trigger()
    }
    Shortcut {
        sequence: "1"
        enabled: actualSize.enabled
        onActivated: actualSize.trigger()
    }
    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        enabled: zoomIn.enabled
        onActivated: zoomIn.trigger()
    }
    Shortcut {
        sequence: "Ctrl+-"
        enabled: zoomOut.enabled
        onActivated: zoomOut.trigger()
    }

    Shortcut {
        sequence: "["
        enabled: previousImage.enabled
        onActivated: previousImage.trigger()
    }
    Shortcut {
        sequence: "]"
        enabled: nextImage.enabled
        onActivated: nextImage.trigger()
    }
    // A literal sequence, so that Ctrl+Shift+G does not match.
    Shortcut {
        sequence: "Ctrl+G"
        enabled: gridToggle.enabled
        onActivated: gridToggle.trigger()
    }
    Shortcut {
        sequence: "Ctrl+R"
        enabled: refresh.enabled
        onActivated: refresh.trigger()
    }

    Shortcut {
        sequences: [StandardKey.Open]
        enabled: openImage.enabled
        onActivated: openImage.trigger()
    }
    Shortcut {
        sequence: "F"
        enabled: fullscreenCommand.enabled
        onActivated: fullscreenCommand.trigger()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !root.modalActive
        onActivated: {
            // The grid closes first and fullscreen is untouched; a menu-closing Escape must not also close the grid.
            if (root.gridMode) {
                if (!root.actionsMenuOpen)
                    root.leaveGridRequested();
            } else {
                root.leaveFullscreenRequested();
            }
        }
    }
    Shortcut {
        sequence: "Q"
        enabled: quit.enabled
        onActivated: quit.trigger()
    }
}
