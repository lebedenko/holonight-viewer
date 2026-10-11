pragma ComponentBehavior: Bound

import QtQuick
import HolonightViewer
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

Controls.Menu {
    id: root
    required property real windowWidth
    required property real windowHeight
    required property ViewerActions actions
    required property bool gridMode
    component ViewerMenuItem: Controls.MenuItem {
        id: control
        // Keeps the trailing shortcut clear of the menu's overlay scroll bar.
        rightPadding: 12 + ((control.ListView.view as ViewerMenuList)?.scrollBarReserve ?? 0)
        // Presentation only; the real sequence stays on the bound Action or Shortcut.
        property var shortcutKeys: []
        contentItem: RowLayout {
            spacing: HnMetrics.internalSpacing(HnControlSize.Normal)
            HnLabel {
                id: menuLabel
                objectName: "menuItemLabel"
                Layout.fillWidth: true
                rawText: control.text
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: control.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textDisabled
            }
            // The style draws no indicator for this custom content, so a checkable item shows its own check mark.
            HnLabel {
                objectName: "menuItemCheck"
                visible: control.checkable
                opacity: control.checked ? 1 : 0
                text: "✓"
                textFormat: Text.PlainText
                color: control.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textDisabled
                Accessible.ignored: true
            }
            HnKeySequenceLabel {
                lowercaseLetters: true
                objectName: "menuItemShortcut"
                font: menuLabel.font
                color: control.enabled ? HoloniightPalette.textMuted : HoloniightPalette.textDisabled
                visible: control.shortcutKeys.length > 0
                keyGroups: control.shortcutKeys
                Accessible.ignored: true
            }
        }
        background: HnButtonSurface {
            pressed: control.down
            hovered: control.hovered
            focusVisible: control.enabled && (control.visualFocus || control.highlighted)
            cornerRadius: 0
        }
    }
    component ViewerMenuList: ListView {
        property real scrollBarReserve: 0
        // Menu owns navigation and skips separators and disabled actions.
        keyNavigationEnabled: false
    }
    // Preserve Viewer's physical hairline at fractional menu/scroll positions.
    component ViewerMenuSeparator: Controls.MenuSeparator {
        contentItem: HnSeparator {
            color: HoloniightPalette.borderPassive
        }
    }
    objectName: "actionsMenu"
    popupType: Controls.Popup.Item
    margins: 12
    x: parent.width - width
    width: Math.min(380, root.windowWidth - 24)
    height: Math.min(implicitHeight, root.windowHeight - 24)
    y: parent.height + HnMetrics.internalSpacing(HnControlSize.Normal)
    contentItem: ViewerMenuList {
        objectName: "actionsMenuList"
        implicitHeight: contentHeight
        model: root.contentModel
        currentIndex: root.currentIndex
        interactive: contentHeight > height
        clip: true
        scrollBarReserve: interactive ? menuScrollBar.width : 0
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: menuScrollBar
            active: true
        }
    }

    ViewerMenuItem {
        objectName: "openButton"
        action: root.actions.openImage
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Open)
    }
    ViewerMenuItem {
        objectName: "pastePathMenuItem"
        action: root.actions.pastePath
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Paste)
    }
    ViewerMenuItem {
        action: root.actions.refresh
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Refresh)
    }
    ViewerMenuItem {
        id: gridToggleItem
        objectName: "gridToggleItem"
        action: root.actions.gridToggle
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Grid)
    }
    // The command is not checkable: only gridMode controls the displayed state.
    Binding {
        target: gridToggleItem
        property: "checkable"
        value: true
    }
    // Checking the item would otherwise detach a plain `checked` binding.
    Binding {
        target: gridToggleItem
        property: "checked"
        value: root.gridMode
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        objectName: "previousMenuItem"
        action: root.actions.previousImage
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Previous)
    }
    ViewerMenuItem {
        objectName: "nextMenuItem"
        action: root.actions.nextImage
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Next)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        objectName: "fitButton"
        action: root.actions.fit
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Fit)
    }
    ViewerMenuItem {
        objectName: "actualSizeButton"
        action: root.actions.actualSize
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ActualSize)
    }
    ViewerMenuItem {
        objectName: "zoomInButton"
        action: root.actions.zoomIn
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomIn)
    }
    ViewerMenuItem {
        objectName: "zoomOutButton"
        action: root.actions.zoomOut
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomOut)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: root.actions.rotateClockwise
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateClockwise)
    }
    ViewerMenuItem {
        action: root.actions.rotateCounterclockwise
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateCounterclockwise)
    }
    ViewerMenuItem {
        action: root.actions.flipHorizontal
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.FlipHorizontal)
    }
    ViewerMenuItem {
        action: root.actions.flipVertical
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.FlipVertical)
    }
    ViewerMenuItem {
        action: root.actions.resetTransform
        objectName: "resetTransformMenuItem"
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: root.actions.copyImage
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyImage)
    }
    ViewerMenuItem {
        action: root.actions.copyPath
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyPath)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: root.actions.imageInformation
        objectName: "informationMenuItem"
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Information)
    }
    ViewerMenuItem {
        action: root.actions.shortcutHelp
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Help)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: root.actions.fullscreenCommand
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Fullscreen)
    }
    ViewerMenuItem {
        action: root.actions.quit
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Quit)
    }
}
