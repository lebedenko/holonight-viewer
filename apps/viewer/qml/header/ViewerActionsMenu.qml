pragma ComponentBehavior: Bound

import QtQuick
import HolonightViewer
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

Controls.Menu {
    id: menu
    required property real windowWidth
    required property real windowHeight
    required property bool gridMode
    required property Controls.Action openImageAction
    required property Controls.Action refreshAction
    required property Controls.Action gridToggleAction
    required property Controls.Action previousImageAction
    required property Controls.Action nextImageAction
    required property Controls.Action fitAction
    required property Controls.Action actualSizeAction
    required property Controls.Action zoomInAction
    required property Controls.Action zoomOutAction
    required property Controls.Action fullscreenCommandAction
    required property Controls.Action quitAction
    required property Controls.Action rotateClockwiseAction
    required property Controls.Action rotateCounterclockwiseAction
    required property Controls.Action flipHorizontalAction
    required property Controls.Action flipVerticalAction
    required property Controls.Action resetTransformAction
    required property Controls.Action copyImageAction
    required property Controls.Action copyPathAction
    required property Controls.Action imageInformationAction
    required property Controls.Action shortcutHelpAction
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
        background: Rectangle {
            color: control.down ? HoloniightPalette.surface : control.hovered ? HoloniightPalette.surfaceHover : "transparent"
            border.width: control.enabled && (control.visualFocus || control.highlighted) ? HnMetrics.focusBorderWidth : 0
            border.color: control.enabled && (control.visualFocus || control.highlighted) ? HoloniightPalette.borderFocus : "transparent"
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
    width: Math.min(380, menu.windowWidth - 24)
    height: Math.min(implicitHeight, menu.windowHeight - 24)
    y: parent.height + HnMetrics.internalSpacing(HnControlSize.Normal)
    contentItem: ViewerMenuList {
        objectName: "actionsMenuList"
        implicitHeight: contentHeight
        model: menu.contentModel
        currentIndex: menu.currentIndex
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
        action: menu.openImageAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Open)
    }
    ViewerMenuItem {
        action: menu.refreshAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Refresh)
    }
    ViewerMenuItem {
        id: gridToggleItem
        objectName: "gridToggleItem"
        action: menu.gridToggleAction
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
        value: menu.gridMode
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        objectName: "previousMenuItem"
        action: menu.previousImageAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Previous)
    }
    ViewerMenuItem {
        objectName: "nextMenuItem"
        action: menu.nextImageAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Next)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        objectName: "fitButton"
        action: menu.fitAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Fit)
    }
    ViewerMenuItem {
        objectName: "actualSizeButton"
        action: menu.actualSizeAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ActualSize)
    }
    ViewerMenuItem {
        objectName: "zoomInButton"
        action: menu.zoomInAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomIn)
    }
    ViewerMenuItem {
        objectName: "zoomOutButton"
        action: menu.zoomOutAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.ZoomOut)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: menu.rotateClockwiseAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateClockwise)
    }
    ViewerMenuItem {
        action: menu.rotateCounterclockwiseAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.RotateCounterclockwise)
    }
    ViewerMenuItem {
        action: menu.flipHorizontalAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.FlipHorizontal)
    }
    ViewerMenuItem {
        action: menu.flipVerticalAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.FlipVertical)
    }
    ViewerMenuItem {
        action: menu.resetTransformAction
        objectName: "resetTransformMenuItem"
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: menu.copyImageAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyImage)
    }
    ViewerMenuItem {
        action: menu.copyPathAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.CopyPath)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: menu.imageInformationAction
        objectName: "informationMenuItem"
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Information)
    }
    ViewerMenuItem {
        action: menu.shortcutHelpAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Help)
    }
    ViewerMenuSeparator {}
    ViewerMenuItem {
        action: menu.fullscreenCommandAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Fullscreen)
    }
    ViewerMenuItem {
        action: menu.quitAction
        shortcutKeys: ViewerShortcuts.keyGroups(ViewerShortcuts.Quit)
    }
}
